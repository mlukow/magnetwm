#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>

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
void state_set_current_desktop(state_t *);
void state_set_desktop_names(state_t *);
void state_set_net_supported(state_t *);
void state_set_number_of_desktops(state_t *);
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
	fprintf(stderr, "%s(0x%x): %s\n", request, (unsigned int)event->resourceid, message);

	return 0;
}

void
state_free(state_t *state)
{
	int i;
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

	for (i = 0; i < COLOR_NITEMS; i++) {
		XftColorFree(state->display, state->visual, state->colormap, &state->colors[i]);
	}

	for (i = 0; i < CURSOR_NITEMS; i++) {
		XFreeCursor(state->display, state->cursors[i]);
	}

	free(state->colors);
	free(state->fonts);

	XSync(state->display, False);
	XSetInputFocus(state->display, PointerRoot, RevertToPointerRoot, CurrentTime);
	XCloseDisplay(state->display);

	config_free(state->config);

	free(state);
}

state_t *
state_init(char *display_name, char *config_path)
{
	int error_base, i, result;
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
	state->colormap = DefaultColormap(state->display, state->primary_screen);
	state->visual = DefaultVisual(state->display, state->primary_screen);
	state->root = RootWindow(state->display, state->primary_screen);
	state->fd = ConnectionNumber(state->display);
	state->config = config_init(config_path);

	state->cursors[CURSOR_NORMAL] = XCreateFontCursor(state->display, XC_left_ptr);
	state->cursors[CURSOR_MOVE] = XCreateFontCursor(state->display, XC_fleur);
	state->cursors[CURSOR_RESIZE_BOTTOM_LEFT] = XCreateFontCursor(state->display, XC_bottom_left_corner);
	state->cursors[CURSOR_RESIZE_BOTTOM_RIGHT] = XCreateFontCursor(state->display, XC_bottom_right_corner);
	state->cursors[CURSOR_RESIZE_TOP_LEFT] = XCreateFontCursor(state->display, XC_top_left_corner);
	state->cursors[CURSOR_RESIZE_TOP_RIGHT] = XCreateFontCursor(state->display, XC_top_right_corner);

	state->colors = calloc(COLOR_NITEMS, sizeof(XftColor));
	for (i = 0; i < COLOR_NITEMS; i++) {
		result = XftColorAllocName(
				state->display,
				state->visual,
				state->colormap,
				state->config->colors[i],
				&state->colors[i]);
		if (!result) {
			fprintf(stderr, "Invalid color: %s\n", state->config->colors[i]);
		}
	}

	state->fonts = calloc(FONT_NITEMS, sizeof(XftFont *));
	memset(state->fonts, 0, sizeof(XftFont *));
	for (i = 0; i < FONT_NITEMS; i++) {
		state->fonts[i] = XftFontOpenXlfd(state->display, state->primary_screen, state->config->fonts[i]);

		if (!state->fonts[i]) {
			state->fonts[i] = XftFontOpenName(state->display, state->primary_screen, state->config->fonts[i]);
		}

		if (!state->fonts[i]) {
			fprintf(stderr, "Invalid font: %s\n", state->config->fonts[i]);
		}
	}

	TAILQ_INIT(&state->screens);

	if (!XRRQueryExtension(state->display, &state->xrandr_event_base, &error_base)) {
		fprintf(stderr, "RandR extension missing\n");
		return False;
	}

	state_set_net_supported(state);

	if (!state_update_screens(state)) {
		state_free(state);
		return NULL;
	}

	if (!state_update_clients(state)) {
		state_free(state);
		return NULL;
	}

	state_set_number_of_desktops(state);
	state_set_desktop_names(state);
	state_set_current_desktop(state);

	attributes.cursor = state->cursors[CURSOR_NORMAL];
	attributes.event_mask =
		SubstructureRedirectMask |
		SubstructureNotifyMask |
		EnterWindowMask |
		PropertyChangeMask |
		ButtonPressMask;
	XChangeWindowAttributes(state->display, state->root, CWCursor | CWEventMask, &attributes);

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
		"UTF8_STRING",
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
		"_NET_CLOSE_WINDOW",
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
		"_NET_WM_BYPASS_COMPOSITOR",
		"_NET_WM_STATE_STICKY",
		"_NET_WM_STATE_MAXIMIZED_VERT",
		"_NET_WM_STATE_MAXIMIZED_HORZ",
		"_NET_WM_STATE_HIDDEN",
		"_NET_WM_STATE_FULLSCREEN",
		"_NET_WM_STATE_DEMANDS_ATTENTION",
		"_NET_WM_STATE_SKIP_PAGER",
		"_NET_WM_STATE_SKIP_TASKBAR",
	};

	state->atoms = calloc(48, sizeof(Atom));
	if (!XInternAtoms(state->display, names, 48, False, state->atoms)) {
		free(state->atoms);
		return False;
	}

	return True;
}

void
state_set_current_desktop(state_t *state)
{
	int index = 0;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (screen->active) {
			index += screen->desktop_index;
			break;
		} else {
			index += screen->desktop_count;
		}
	}

	XChangeProperty(
			state->display,
			state->root,
			state->atoms[_NET_CURRENT_DESKTOP],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)&index,
			1);
}

void
state_set_desktop_names(state_t *state)
{
	char *names = NULL;
	int i, names_length = 0;
	screen_t *screen;
	ssize_t name_length;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			name_length = strlen(screen->desktops[i]->name);
			if (names_length == 0) {
				names = calloc(name_length + 1, sizeof(char));
			} else {
				names = realloc(names, (names_length + name_length + 1) * sizeof(char));
			}

			memcpy(names + names_length, screen->desktops[i]->name, name_length);
			names_length += name_length;
			names[names_length++] = '\0';
		}
	}

	XChangeProperty(
			state->display,
			state->root,
			state->atoms[_NET_DESKTOP_NAMES],
			state->atoms[UTF8_STRING],
			8,
			PropModeReplace,
			(unsigned char *)names,
			names_length);
}

void
state_set_net_supported(state_t *state)
{
	Atom supported[] = {
		state->atoms[_NET_SUPPORTED],
		state->atoms[_NET_SUPPORTING_WM_CHECK],
		state->atoms[_NET_ACTIVE_WINDOW],
		state->atoms[_NET_CLIENT_LIST],
		state->atoms[_NET_CLIENT_LIST_STACKING],
		state->atoms[_NET_NUMBER_OF_DESKTOPS],
		state->atoms[_NET_CURRENT_DESKTOP],
		state->atoms[_NET_DESKTOP_VIEWPORT],
		state->atoms[_NET_DESKTOP_GEOMETRY],
		state->atoms[_NET_VIRTUAL_ROOTS],
		state->atoms[_NET_SHOWING_DESKTOP],
		state->atoms[_NET_DESKTOP_NAMES],
		state->atoms[_NET_WORKAREA],
		state->atoms[_NET_WM_NAME],
		state->atoms[_NET_WM_DESKTOP],
		state->atoms[_NET_CLOSE_WINDOW],
		state->atoms[_NET_WM_STATE],
		state->atoms[_NET_WM_STATE_STICKY],
		state->atoms[_NET_WM_STATE_MAXIMIZED_VERT],
		state->atoms[_NET_WM_STATE_MAXIMIZED_HORZ],
		state->atoms[_NET_WM_STATE_HIDDEN],
		state->atoms[_NET_WM_STATE_FULLSCREEN],
		state->atoms[_NET_WM_STATE_DEMANDS_ATTENTION],
		state->atoms[_NET_WM_STATE_SKIP_PAGER],
		state->atoms[_NET_WM_STATE_SKIP_TASKBAR]
	};

	XChangeProperty(
			state->display,
			state->root,
			state->atoms[_NET_SUPPORTED],
			XA_ATOM,
			32,
			PropModeReplace,
			(unsigned char *)supported,
			26);
}

void
state_set_number_of_desktops(state_t *state)
{
	int count = 0;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		count += screen->desktop_count;
	}

	XChangeProperty(
			state->display,
			state->root,
			state->atoms[_NET_NUMBER_OF_DESKTOPS],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)&count,
			1);
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
		client = client_init(state, windows[i], True);
		if (client) {
			screen = screen_for_client(state, client);
			screen_adopt(state, screen, client);

			client_activate(state, client);
		}
	}

	return True;
}


Bool
state_update_screens(state_t *state)
{
	client_t *client;
	geometry_t geometry;
	group_t *group;
	int i, x, y;
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

	screen = screen_find_active(state);
	if (!screen) {
		if (x_get_pointer(state->display, state->root, &x, &y)) {
			screen = screen_for_point(state, x, y);
		}

		if (!screen) {
			screen = TAILQ_LAST(&state->screens, screen_q);
		}

		if (screen) {
			screen_activate(state, screen);
		}
	}

	return True;
}
