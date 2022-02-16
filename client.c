#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "group.h"
#include "screen.h"
#include "state.h"
#include "utils.h"
#include "xutils.h"

void client_configure(state_t *, client_t *);
Atom *client_get_wm_state(state_t *, client_t *, int *);
void client_restore_net_wm_state(state_t *, client_t *);
void client_send_message(state_t *, client_t *, Atom, Time);
void client_set_net_wm_state(state_t *, client_t *);
void client_set_wm_state(state_t *, client_t *, long);
void client_update_class(state_t *, client_t *);

void
client_activate(state_t *state, client_t *client)
{
	client_t *current;

	current = client_find_active(state);
	if (current) {
		current->flags &= ~CLIENT_ACTIVE;
		client_draw_border(state, current);
	}

	if ((client->flags & CLIENT_INPUT) || (!(client->flags & CLIENT_WM_TAKE_FOCUS))) {
		XSetInputFocus(state->display, client->window, RevertToPointerRoot, CurrentTime);
	}

	if (client->flags & CLIENT_WM_TAKE_FOCUS) {
		client_send_message(state, client, state->atoms[WM_TAKE_FOCUS], CurrentTime);
	}

	client->flags |= CLIENT_ACTIVE;
	client->flags &= ~CLIENT_URGENCY;

	client_draw_border(state, client);
	// TODO: ewmh set net active window
}

void
client_configure(state_t *state, client_t *client)
{
	XConfigureEvent event;

	(void)memset(&event, 0, sizeof(XConfigureEvent));
	event.type = ConfigureNotify;
	event.event = client->window;
	event.window = client->window;
	event.x = client->geometry.x;
	event.y = client->geometry.y;
	event.width = client->geometry.width;
	event.height = client->geometry.height;
	event.border_width = client->border_width;
	event.above = None;
	event.override_redirect = 0;

	XSendEvent(state->display, client->window, False, StructureNotifyMask, (XEvent *)&event);
}

void
client_deactivate(state_t *state, client_t *client)
{
	if (!(client->flags & CLIENT_ACTIVE)) {
		return;
	}

	client->flags &= ~CLIENT_ACTIVE;

	client_draw_border(state, client);
}

void
client_draw_border(state_t *state, client_t *client)
{
	unsigned long pixel;

	if (client->flags & CLIENT_ACTIVE) {
		pixel = state->colors[COLOR_BORDER_ACTIVE].pixel;
	} else if (client->flags & CLIENT_URGENCY) {
		pixel = state->colors[COLOR_BORDER_URGENT].pixel;
	} else {
		pixel = state->colors[COLOR_BORDER_INACTIVE].pixel;
	}

	XSetWindowBorderWidth(state->display, client->window, client->border_width);
	XSetWindowBorder(state->display, client->window, pixel);
}

client_t *
client_find(state_t *state, Window window)
{
	client_t *client;
	group_t *group;
	int i;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				TAILQ_FOREACH(client, &group->clients, entry) {
					if (client->window == window) {
						return client;
					}
				}
			}
		}
	}

	return NULL;
}

client_t *
client_find_active(state_t *state)
{
	client_t *client;
	group_t *group;
	int i;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				TAILQ_FOREACH(client, &group->clients, entry) {
					if (client->flags & CLIENT_ACTIVE) {
						return client;
					}
				}
			}
		}
	}

	return NULL;
}

void
client_free(client_t *client)
{
	if (!client) {
		return;
	}

	if (client->name) {
		free(client->name);
	}

	if (client->class_name) {
		free(client->class_name);
	}

	if (client->instance_name) {
		free(client->instance_name);
	}

	free(client);
}

void
client_hide(state_t *state, client_t *client)
{
	XUnmapWindow(state->display, client->window);

	if (client->flags & CLIENT_ACTIVE) {
		client->flags &= ~CLIENT_ACTIVE;
		// TODO: net active window
	}

	client->flags |= CLIENT_HIDDEN;
	client_set_wm_state(state, client, IconicState);
}

client_t *
client_init(state_t *state, Window window)
{
	client_t *client;
	XWindowAttributes attributes;

	if (!XGetWindowAttributes(state->display, window, &attributes)) {
		return NULL;
	}

	if (attributes.override_redirect || (attributes.map_state != IsViewable)) {
		return NULL;
	}

	client = calloc(1, sizeof(client_t));
	client->window = window;
	client->name = NULL;
	client->class_name = NULL;
	client->instance_name = NULL;
	client->border_width = state->config->border_width;
	client->flags = 0;

	client->geometry.x = attributes.x;
	client->geometry.y = attributes.y;
	client->geometry.width = attributes.width;
	client->geometry.height = attributes.height;

	client_update_class(state, client);
	client_update_size_hints(state, client);
	client_update_wm_hints(state, client);
	client_update_wm_name(state, client);

	client_restore_net_wm_state(state, client);

	if (attributes.map_state != IsViewable) {

	}

	client_configure(state, client);

	XSelectInput(state->display, window, EnterWindowMask | PropertyChangeMask | KeyReleaseMask);

	/*
	x_ewmh_set_client_list(state);
	x_ewmh_set_client_list_stacking(state);
	*/

	return client;
}

void
client_lower(state_t *state, client_t *client)
{
	XLowerWindow(state->display, client->window);
}

void
client_move_resize(state_t *state, client_t *client, Bool reset)
{
	if (reset) {
		client->flags &= ~CLIENT_MAXIMIZED;
		client_set_net_wm_state(state, client);
	}

	XMoveResizeWindow(
			state->display,
			client->window,
			client->geometry.x,
			client->geometry.y,
			client->geometry.width,
			client->geometry.height);

	client_configure(state, client);
}

void
client_raise(state_t *state, client_t *client)
{
	XRaiseWindow(state->display, client->window);
}

void
client_remove(state_t *state, client_t *client)
{
	group_t *group;

	/*
	x_ewmh_set_client_list(state);
	x_ewmh_set_client_list_stacking(state);

	if (client->flags & CLIENT_ACTIVE) {
		// TODO: ewmh set net active window to none
	}
	*/

	group = client->group;
	group_unassign(client);
	client_free(client);

	client = TAILQ_LAST(&group->clients, client_q);
	if (client) {
		client_activate(state, client);
	}
}

Atom *
client_get_wm_state(state_t *state, client_t *client, int *count)
{
	Atom *wm_state;
	unsigned char *output;
	if ((*count = x_get_property(state->display, client->window, state->atoms[_NET_WM_STATE], XA_ATOM, 64L, &output)) <= 0) {
		return NULL;
	}

	wm_state = calloc(*count, sizeof(Atom));
	(void)memcpy(wm_state, output, *count * sizeof(Atom));
	XFree(output);

	return wm_state;
}

void
client_restore_net_wm_state(state_t *state, client_t *client)
{
	Atom *atoms;
	int count, i;

	atoms = client_get_wm_state(state, client, &count);
	for (i = 0; i < count; i++) {
		if (atoms[i] == state->atoms[_NET_WM_STATE_STICKY]) {
			client_toggle_sticky(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_MAXIMIZED_VERT]) {
			client_toggle_vmaximize(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_MAXIMIZED_HORZ]) {
			client_toggle_hmaximize(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_HIDDEN]) {
			client_toggle_hidden(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_FULLSCREEN]) {
			client_toggle_fullscreen(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_DEMANDS_ATTENTION]) {
			client_toggle_urgent(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_SKIP_PAGER]) {
			client_toggle_skip_pager(state, client);
		} else if (atoms[i] == state->atoms[_NET_WM_STATE_SKIP_TASKBAR]) {
			client_toggle_skip_taskbar(state, client);
		} else if (atoms[i] == state->atoms[_CWM_WM_STATE_FREEZE]) {
			client_toggle_freeze(state, client);
		}
	}

	free(atoms);
}

void
client_send_message(state_t *state, client_t *client, Atom atom, Time time)
{
	XClientMessageEvent event;

	(void)memset(&event, 0, sizeof(XClientMessageEvent));
	event.type = ClientMessage;
	event.window = client->window;
	event.message_type = state->atoms[WM_PROTOCOLS];
	event.format = 32;
	event.data.l[0] = atom;
	event.data.l[1] = time;

	XSendEvent(state->display, client->window, False, NoEventMask, (XEvent *)&event);
}

void
client_set_net_wm_state(state_t *state, client_t *client)
{
	Atom *input, *output;
	int count, i, j;

	input = client_get_wm_state(state, client, &count);
	output = calloc(count + 9, sizeof(Atom));

	for (i = j = 0; i < count; i++) {
		if ((input[i] != state->atoms[_NET_WM_STATE_STICKY]) &&
			(input[i] != state->atoms[_NET_WM_STATE_MAXIMIZED_VERT]) &&
			(input[i] != state->atoms[_NET_WM_STATE_MAXIMIZED_HORZ]) &&
			(input[i] != state->atoms[_NET_WM_STATE_HIDDEN]) &&
			(input[i] != state->atoms[_NET_WM_STATE_FULLSCREEN]) &&
			(input[i] != state->atoms[_NET_WM_STATE_DEMANDS_ATTENTION]) &&
			(input[i] != state->atoms[_NET_WM_STATE_SKIP_PAGER]) &&
			(input[i] != state->atoms[_NET_WM_STATE_SKIP_TASKBAR]) &&
			(input[i] != state->atoms[_CWM_WM_STATE_FREEZE])) {
			output[j++] = input[i];
		}
	}

	free(input);

	if (client->flags & CLIENT_STICKY) {
		output[j++] = state->atoms[_NET_WM_STATE_STICKY];
	}

	if (client->flags & CLIENT_HIDDEN) {
		output[j++] = state->atoms[_NET_WM_STATE_HIDDEN];
	}

	if (client->flags & CLIENT_FULLSCREEN) {
		output[j++] = state->atoms[_NET_WM_STATE_FULLSCREEN];
	} else if (client->flags & CLIENT_VMAXIMIZED) {
		output[j++] = state->atoms[_NET_WM_STATE_MAXIMIZED_VERT];
	} else if (client->flags & CLIENT_HMAXIMIZED) {
		output[j++] = state->atoms[_NET_WM_STATE_MAXIMIZED_HORZ];
	}

	if (client->flags & CLIENT_URGENCY) {
		output[j++] = state->atoms[_NET_WM_STATE_DEMANDS_ATTENTION];
	}

	if (client->flags & CLIENT_SKIP_PAGER) {
		output[j++] = state->atoms[_NET_WM_STATE_SKIP_PAGER];
	}

	if (client->flags & CLIENT_SKIP_TASKBAR) {
		output[j++] = state->atoms[_NET_WM_STATE_SKIP_TASKBAR];
	}

	if (client->flags & CLIENT_FREEZE) {
		output[j++] = state->atoms[_CWM_WM_STATE_FREEZE];
	}

	if (j > 0) {
		XChangeProperty(
				state->display,
				client->window,
				state->atoms[_NET_WM_STATE],
				XA_ATOM,
				32,
				PropModeReplace,
				(unsigned char *)output,
				j);
	} else {
		XDeleteProperty(state->display, client->window, state->atoms[_NET_WM_STATE]);
	}

	free(output);
}

void
client_set_wm_state(state_t *state, client_t *client, long wm_state)
{
	long data[] = { wm_state, None };

	XChangeProperty(
			state->display,
			client->window,
			state->atoms[WM_STATE],
			state->atoms[WM_STATE],
			32,
			PropModeReplace,
			(unsigned char *)data,
			2);
}

void
client_show(state_t *state, client_t *client)
{
	XMapWindow(state->display, client->window);

	client->flags &= ~CLIENT_HIDDEN;
	client_set_wm_state(state, client, NormalState);
	client_draw_border(state, client);
}

void
client_toggle_freeze(state_t *state, client_t *client)
{
	if (client->flags & CLIENT_FULLSCREEN) {
		return;
	}

	client->flags ^= CLIENT_FREEZE;
	client_set_net_wm_state(state, client);
}

void
client_toggle_fullscreen(state_t *state, client_t *client)
{
	screen_t *screen;

	if (client->flags & CLIENT_FREEZE) {
		return;
	}

	screen = screen_for_client(state, client);
	client->geometry = screen->geometry;

	client_move_resize(state, client, False);
	client_set_net_wm_state(state, client);
}

void
client_toggle_hidden(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_HIDDEN;
	client_set_net_wm_state(state, client);
}

void
client_toggle_hmaximize(state_t *state, client_t *client)
{
	screen_t *screen;

	if (client->flags & CLIENT_FREEZE) {
		return;
	}

	screen = screen_for_client(state, client);
	client->geometry.x = screen->geometry.x;
	client->geometry.width = screen->geometry.width - 2 * client->border_width;

	client_move_resize(state, client, False);
	client_set_net_wm_state(state, client);
}

void
client_toggle_skip_pager(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_SKIP_PAGER;
	client_set_net_wm_state(state, client);
}

void
client_toggle_skip_taskbar(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_SKIP_TASKBAR;
	client_set_net_wm_state(state, client);
}

void
client_toggle_sticky(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_STICKY;
	client_set_net_wm_state(state, client);
}

void
client_toggle_urgent(state_t *state, client_t *client)
{
	if (!(client->flags & CLIENT_ACTIVE)) {
		client->flags |= CLIENT_URGENCY;
	}
}

void
client_toggle_vmaximize(state_t *state, client_t *client)
{
	screen_t *screen;

	if (client->flags & CLIENT_FREEZE) {
		return;
	}

	screen = screen_for_client(state, client);
	client->geometry.y = screen->geometry.y;
	client->geometry.width = screen->geometry.height - 2 * client->border_width;

	client_move_resize(state, client, False);
	client_set_net_wm_state(state, client);
}

void
client_update_class(state_t *state, client_t *client)
{
	XClassHint hint;

	if (!XGetClassHint(state->display, client->window, &hint)) {
		client->class_name = strdup("");
		client->instance_name = strdup("");
		return;
	}

	client->class_name = strdup(hint.res_class);
	XFree(hint.res_class);

	client->instance_name = strdup(hint.res_name);
	XFree(hint.res_name);
}

void
client_update_size_hints(state_t *state, client_t *client)
{
	long supplied_return;
	XSizeHints hints;

	if (!XGetWMNormalHints(state->display, client->window, &hints, &supplied_return)) {
		hints.flags = 0;
	}

	client->hints.flags = hints.flags;

	if (hints.flags & PBaseSize) {
		client->hints.desired_width = hints.base_width;
		client->hints.desired_height = hints.base_height;
	}

	if (hints.flags & PMinSize) {
		client->hints.min_width = hints.min_width;
		client->hints.min_height = hints.min_height;
	}

	if (hints.flags & PMaxSize) {
		client->hints.max_width = hints.max_width;
		client->hints.max_height = hints.max_height;
	}

	if (hints.flags & PResizeInc) {
		client->hints.width_increment = hints.width_inc;
		client->hints.height_increment = hints.height_inc;
	}

	client->hints.width_increment = MAX(1, client->hints.width_increment);
	client->hints.height_increment = MAX(1, client->hints.height_increment);
	client->hints.min_width = MAX(1, client->hints.min_width);
	client->hints.min_height = MAX(1, client->hints.min_height);

	if (hints.flags & PAspect) {
		if (hints.min_aspect.x > 0) {
			client->hints.min_aspect_ratio = (float)hints.min_aspect.y / hints.min_aspect.x;
		}

		if (hints.max_aspect.y > 0) {
			client->hints.max_aspect_ratio = (float)hints.max_aspect.x / hints.max_aspect.y;
		}
	}
}

void
client_update_wm_hints(state_t *state, client_t *client)
{
	XWMHints *hints;

	hints = XGetWMHints(state->display, client->window);
	if (!hints) {
		return;
	}

	if ((hints->flags & InputHint) && (hints->input)) {
		client->flags |= CLIENT_INPUT;
	}

	if (hints->flags & XUrgencyHint) {
		client->flags |= CLIENT_URGENCY;
	}

	if (hints->flags & StateHint) {
		client->initial_state = hints->initial_state;
	}

	// TODO: icon

	XFree(hints);
}

void
client_update_wm_name(state_t *state, client_t *client)
{
	XTextProperty text;

	if (XGetTextProperty(state->display, client->window, &text, state->atoms[_NET_WM_NAME]) != Success) {
		return;
	}

	if (text.nitems == 0) {
		if (!XGetWMName(state->display, client->window, &text)) {
			return;
		}
	}

	client->name = strdup((char *)text.value);
	XFree(text.value);
}

void
client_update_wm_protocols(state_t *state, client_t *client)
{
	Atom *protocols;
	int count, i;

	if (!XGetWMProtocols(state->display, client->window, &protocols, &count)) {
		return;
	}

	for (i = 0; i < count; i++) {
		if (protocols[i] == state->atoms[WM_DELETE_WINDOW]) {
			client->flags |= CLIENT_WM_DELETE_WINDOW;
		} else if (protocols[i] == state->atoms[WM_TAKE_FOCUS]) {
			client->flags |= CLIENT_WM_TAKE_FOCUS;
		}
	}
}
