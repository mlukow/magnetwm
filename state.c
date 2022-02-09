#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/extensions/Xrandr.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "group.h"
#include "screen.h"
#include "state.h"
#include "utils.h"
#include "xutils.h"

void state_bind(state_t *);
int state_error_handler(Display *, XErrorEvent *);
Bool state_init_atoms(state_t *);
Bool state_update_clients(state_t *);
Bool state_update_screens(state_t *);

void
state_bind(state_t *state)
{
	binding_t *binding;

	XUngrabButton(state->display, AnyButton, AnyModifier, state->root);
	XUngrabKey(state->display, AnyKey, AnyModifier, state->root);

	/*
	XGrabButton(
			state->display,
			AnyButton,
			AnyModifier,
			state->root,
			False,
			ButtonPressMask | ButtonReleaseMask,
			GrabModeSync,
			GrabModeSync,
			None,
			None);
			*/

	TAILQ_FOREACH(binding, &state->config->keybindings, entry) {
		XGrabKey(
				state->display,
				XKeysymToKeycode(state->display, binding->button),
				binding->modifier,
				state->root,
				True,
				GrabModeAsync,
				GrabModeAsync);
	}
}

int
state_error_handler(Display *display, XErrorEvent *event)
{
	char message[80], number[80], request[80];

	XGetErrorText(display, event->error_code, message, sizeof(message));
	snprintf(number, sizeof(number), "%d", event->request_code);
	XGetErrorDatabaseText(display, "XRequest", number, "<unknown>", request, sizeof(request));
	fprintf(stderr, "%s(0x%x): %s", request, (unsigned int)event->resourceid, message);

	return 0;
}

void
state_free(state_t *state)
{
	screen_t *screen;

	if (!state) {
		return;
	}

	XUngrabPointer(state->display, CurrentTime);
	XUngrabKeyboard(state->display, CurrentTime);

	while ((screen = TAILQ_FIRST(&state->screens)) != NULL) {
		TAILQ_REMOVE(&state->screens, screen, entry);
		screen_free(screen);
	}

	XSync(state->display, False);
	XSetInputFocus(state->display, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(state->display);

	config_free(state->config);

	free(state);
}

state_t *
state_init(char *display_name, char *config_path)
{
	state_t *state;
	XSetWindowAttributes attributes;

	state = malloc(sizeof(state_t));
	state->display = XOpenDisplay(display_name);
	if (!state->display) {
		fprintf(stderr, "Can't open display %s\n", XDisplayName(display_name));
		free(state);
		return NULL;
	}

	if (!state_init_atoms(state)) {
		free(state);
		return NULL;
	}

	state->primary_screen = DefaultScreen(state->display);
	state->root = RootWindow(state->display, state->primary_screen);
	state->fd = ConnectionNumber(state->display);
	state->config = config_init(config_path);

	TAILQ_INIT(&state->screens);

	if (!state_update_screens(state)) {
		state_free(state);
		return NULL;
	}

	if (!state_update_clients(state)) {
		state_free(state);
		return NULL;
	}

	attributes.event_mask =
		SubstructureRedirectMask | SubstructureNotifyMask | EnterWindowMask | PropertyChangeMask | ButtonPressMask;
	XChangeWindowAttributes(state->display, state->root, CWEventMask, &attributes);

	state_bind(state);

	XSetErrorHandler(state_error_handler);

	return state;
}

Bool
state_init_atoms(state_t *state)
{
	char *names[] = {
		"WM_STATE",
		"WM_DELETE_WINDOW",
		"WM_TAKE_FOCUS",
		"WM_PROTOCOLS",
		"WM_CHANGE_STATE",
		"_NET_SUPPORTED",
		"_NET_CLIENT_LIST",
		"_NET_CLIENT_LIST_STACKING",
		"_NET_NUMBER_OF_DESKTOPS",
		"_NET_DESKTOP_GEOMETRY",
		"_NET_DESKTOP_VIEWPORT",
		"_NET_CURRENT_DESKTOP",
		"_NET_DESKTOP_NAMES",
		"_NET_ACTIVE_WINDOW",
		"_NET_WORKAREA",
		"_NET_SUPPORTING_WM_CHECK",
		"_NET_VIRTUAL_ROOTS",
		"_NET_DESKTOP_LAYOUT",
		"_NET_SHOWING_DESKTOP",
		"_NET_WM_NAME",
		"_NET_WM_VISIBLE_NAME",
		"_NET_WM_ICON_NAME",
		"_NET_WM_VISIBLE_ICON_NAME",
		"_NET_WM_DESKTOP",
		"_NET_WM_WINDOW_TYPE",
		"_NET_WM_STATE",
		"_NET_WM_ALLOWED_ACTIONS",
		"_NET_WM_STRUT",
		"_NET_WM_STRUT_PARTIAL",
		"_NET_WM_ICON_GEOMETRY",
		"_NET_WM_ICON",
		"_NET_WM_PID",
		"_NET_WM_HANDLED_ICONS",
		"_NET_WM_USER_TIME",
		"_NET_WM_USER_TIME_WINDOW",
		"_NET_FRAME_EXTENTS",
		"_NET_WM_OPAQUE_REGION",
		"_NET_WM_BYPASS_COMPOSITOR"
	};

	state->atoms = calloc(32, sizeof(Atom));
	if (!XInternAtoms(state->display, names, 33, False, state->atoms)) {
		free(state->atoms);
		return False;
	}

	return True;
}

Bool
state_update_clients(state_t *state)
{
	client_t *client;
	screen_t *screen;
	unsigned int count, i;
	Window *windows, root, parent;

	if (!XQueryTree(state->display, state->root, &root, &parent, &windows, &count)) {
		return False;
	}

	for (i = 0; i < count; i++) {
		client = client_init(state, windows[i]);
		screen = screen_for_client(state, client);
		screen_adopt(state, screen, client);

		/*
		client_set_active(state, client);
		*/
	}

	return True;
}

Bool
state_find_crtc(state_t *state, XRRScreenResources *resources, screen_binding_t *binding, RROutput *output)
{
	int i;
	XRRCrtcInfo *crtc;

	for (i = 0; i < resources->ncrtc; i++) {
		crtc = XRRGetCrtcInfo(state->display, resources, resources->crtcs[i]);
		if (!crtc) {
			return False;
		}

		if ((crtc->width == binding->width) && (crtc->height == binding->height)) {
			*output = resources->crtcs[i];
			return True;
		}

	}

	return False;
}

Bool
state_find_mode(XRRScreenResources *resources, screen_binding_t *binding, RRMode *mode)
{
	int i;

	for (i = 0; i < resources->nmode; i++) {
		if ((resources->modes[i].width == binding->width) && (resources->modes[i].height == binding->height)) {
			*mode = resources->modes[i].id;
			return True;
		}
	}

	return False;
}

screen_binding_t *
state_find_screen_binding(state_t *state, char *screen_name)
{
	screen_binding_t *binding;

	TAILQ_FOREACH(binding, &state->config->screenbindings, entry) {
		if (!strcmp(binding->name, screen_name)) {
			return binding;
		}
	}

	return NULL;
}

Bool
state_update_screens(state_t *state)
{
	client_t *client;
	double dpi, height_mm, width_mm;
	geometry_t geometry;
	group_t *group;
	int event_base, error_base, height_px = 0, i, j, major, minor, width_px = 0;
	RRMode mode_id;
	screen_binding_t *binding;
	screen_t *screen;
	XRRCrtcInfo *crtc;
	XRROutputInfo *output;
	XRRScreenResources *resources;

	if (!XRRQueryExtension(state->display, &event_base, &error_base)) {
		fprintf(stderr, "RandR extension missing\n");
		return False;
	}

	if (!XRRQueryVersion(state->display, &major, &minor)) {
		fprintf(stderr, "RandR extension missing\n");
		return False;
	}

	if ((major < 1) || (minor < 2)) {
		fprintf(stderr, "RandR extension too old: %d.%d\n", major, minor);
		return False;
	}

	resources = XRRGetScreenResources(state->display, state->root);
	if (!resources) {
		fprintf(stderr, "Could not get screen resources");
		return False;
	}

	TAILQ_FOREACH(screen, &state->screens, entry) {
		screen->wired = False;
	}

	TAILQ_FOREACH(binding, &state->config->screenbindings, entry) {
		if (binding->x > width_px) {
			width_px = binding->x;
		}

		if (binding->y > height_px) {
			height_px = binding->y;
		}

		if (binding->x + binding->width > width_px) {
			width_px = binding->x + binding->width;
		}

		if (binding->y + binding->height > height_px) {
			height_px = binding->y + binding->height;
		}
	}

	dpi = (25.4 * DisplayHeight(state->display, state->primary_screen)) /
		DisplayHeightMM(state->display, state->primary_screen);
	width_mm = (25.4 * width_px) / dpi;
	height_mm = (25.4 * height_px) / dpi;

	for (i = 0; i < resources->noutput; i++) {
		output = XRRGetOutputInfo(state->display, resources, resources->outputs[i]);

		if (output->connection != RR_Connected) {
			continue;
		}

		screen = screen_init(state, output->name, output->crtc, geometry, output->mm_width, output->mm_height);
		TAILQ_INSERT_TAIL(&state->screens, screen, entry);

		XRRSetCrtcConfig(
				state->display,
				resources,
				output->crtc,
				CurrentTime,
				0,
				0,
				None,
				RR_Rotate_0,
				NULL,
				0);
	}

	FILE *f = fopen("/tmp/torwm.out", "w");
	fprintf(f, "Setting screen size %dx%d (%-.2fx%-.2f)\n", width_px, height_px, width_mm, height_mm);
	XRRSetScreenSize(state->display, state->root, width_px, height_px, width_mm, height_mm);

	for (i = 0; i < resources->noutput; i++) {
		output = XRRGetOutputInfo(state->display, resources, resources->outputs[i]);
		if (!output) {
			fprintf(stderr, "Could not get output 0x%lx information\n", resources->outputs[i]);
			fprintf(f, "Could not get output 0x%lx information\n", resources->outputs[i]);
			fclose(f);
			XFree(resources);
			return False;
		}

		if (output->connection != RR_Connected) {
			continue;
		}

		screen = screen_find_by_name(state, output->name);
		binding = state_find_screen_binding(state, output->name);
		if (binding) {
			if (!state_find_mode(resources, binding, &mode_id)) {
				fprintf(stderr, "Could not find screen mode\n");
				fprintf(f, "Could not find screen mode\n");
				fclose(f);
				XFree(resources);
				return False;
			}

			crtc = XRRGetCrtcInfo(state->display, resources, screen->id);
			fprintf(f, "setting crt %d\n", screen->id);
			XRRSetCrtcConfig(
					state->display,
					resources,
					screen->id,
					CurrentTime,
					binding->x,
					binding->y,
					mode_id,
					RR_Rotate_0,
					crtc->outputs,
					crtc->noutput);

			geometry.x = binding->x,
			geometry.y = binding->y;
			geometry.width = binding->width;
			geometry.height = binding->height;
		} else {
			crtc = XRRGetCrtcInfo(state->display, resources, output->crtc);
			geometry.x = crtc->x;
			geometry.y = crtc->y;
			geometry.width = crtc->width;
			geometry.height = crtc->height;
		}

		screen_update_geometry(state, screen, geometry);
		screen->wired = True;
	}

	while ((screen = screen_find_unwired(state)) != NULL) {
		TAILQ_REMOVE(&state->screens, screen, entry);

		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				TAILQ_FOREACH(client, &group->clients, entry) {
					group_unassign(screen->desktops[i], client);
					screen = screen_for_client(state, client);
					screen_adopt(state, screen, client);
				}
			}
		}

		screen_free(screen);
	}

	XFree(resources);

	return True;
}
