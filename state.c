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
	int error_base;
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

	if (!XRRQueryExtension(state->display, &state->xrandr_event_base, &error_base)) {
		fprintf(stderr, "RandR extension missing\n");
		return False;
	}

	if (!state_update_screens(state)) {
		state_free(state);
		return NULL;
	}

	if (!state_update_clients(state)) {
		state_free(state);
		return NULL;
	}

	attributes.event_mask =
		SubstructureRedirectMask |
		SubstructureNotifyMask |
		EnterWindowMask |
		PropertyChangeMask |
		ButtonPressMask;
	XChangeWindowAttributes(state->display, state->root, CWEventMask, &attributes);

	XRRSelectInput(state->display, state->root, RRScreenChangeNotifyMask);

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

		client_activate(state, client);
	}

	return True;
}


Bool
state_update_screens(state_t *state)
{
	client_t *client;
	geometry_t geometry;
	group_t *group;
	int i;
	screen_t *screen;
	XRRCrtcInfo *crtc;
	XRROutputInfo *output;
	XRRScreenResources *resources;

	resources = XRRGetScreenResources(state->display, state->root);
	if (!resources) {
		fprintf(stderr, "Could not get screen resources");
		return False;
	}

	TAILQ_FOREACH(screen, &state->screens, entry) {
		screen->wired = False;
	}

	for (i = 0; i < resources->noutput; i++) {
		output = XRRGetOutputInfo(state->display, resources, resources->outputs[i]);
		if ((!output) || (output->connection != RR_Connected)) {
			continue;
		}

		crtc = XRRGetCrtcInfo(state->display, resources, output->crtc);
		if (!crtc) {
			continue;
		}

		geometry.x = crtc->x;
		geometry.y = crtc->y;
		geometry.width = crtc->width;
		geometry.height = crtc->height;

		screen = screen_find_by_name(state, output->name);
		if (!screen) {
			screen = screen_init(state, output->name, output->crtc, geometry, output->mm_width, output->mm_height);
			TAILQ_INSERT_TAIL(&state->screens, screen, entry);
		}

		screen_update_geometry(state, screen, geometry);
		screen->wired = True;
	}

	XFree(resources);

	while ((screen = screen_find_unwired(state)) != NULL) {
		TAILQ_REMOVE(&state->screens, screen, entry);

		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				while ((client = TAILQ_FIRST(&group->clients)) != NULL) {
					TAILQ_REMOVE(&group->clients, client, entry);
					screen_adopt(state, screen_for_client(state, client), client);
				}
			}
		}

		screen_free(screen);
	}

	return True;
}
