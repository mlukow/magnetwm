#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xutil.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "group.h"
#include "screen.h"
#include "state.h"
#include "utils.h"
#include "xutils.h"

void client_send_message(state_t *, client_t *, Atom, Time);
void client_update_class(state_t *, client_t *);

void
client_activate(state_t *state, client_t *client)
{
	client_t *current;

	current = client_find_active(state);
	if (current) {
		current->active = False;
		client_draw_border(state, current);
	}

	if ((client->flags & InputHint) || (!client->take_focus)) {
		XSetInputFocus(state->display, client->window, RevertToPointerRoot, CurrentTime);
	}

	if (client->take_focus) {
		client_send_message(state, client, state->atoms[WM_TAKE_FOCUS], CurrentTime);
	}

	client->active = True;
	client->flags &= ~XUrgencyHint;

	client_draw_border(state, client);
	// TODO: ewmh set net active window
}

void
client_deactivate(state_t *state, client_t *client)
{
	if (!client->active) {
		return;
	}

	client->active = False;

	client_draw_border(state, client);
}

void
client_draw_border(state_t *state, client_t *client)
{
	unsigned long pixel;

	if (client->active) {
		pixel = 0x00ff00;
	} else if (client->flags & XUrgencyHint) {
		pixel = 0xff0000;
	} else {
		pixel = 0x0000ff;
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
					if (client->active) {
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

client_t *
client_init(state_t *state, Window window)
{
	client_t *client;
	XWindowAttributes attributes;

	if (!XGetWindowAttributes(state->display, window, &attributes)) {
		return NULL;
	}

	if (attributes.override_redirect) {
		return NULL;
	}

	client = calloc(1, sizeof(client_t));
	client->window = window;
	client->name = NULL;
	client->class_name = NULL;
	client->instance_name = NULL;
	client->border_width = state->config->border_width;
	client->flags = 0;
	client->delete_window = False;
	client->take_focus = False;

	client->geometry.x = attributes.x;
	client->geometry.y = attributes.y;
	client->geometry.width = attributes.width;
	client->geometry.height = attributes.height;

	client_update_class(state, client);
	client_update_size_hints(state, client);
	client_update_wm_hints(state, client);
	client_update_wm_name(state, client);

	XSelectInput(state->display, window, EnterWindowMask | PropertyChangeMask | KeyReleaseMask);

	/*
	x_ewmh_set_client_list(state);
	x_ewmh_set_client_list_stacking(state);
	// TODO: restore net wm state
	*/

	return client;
}

void
client_lower(state_t *state, client_t *client)
{
	XLowerWindow(state->display, client->window);
}

void
client_move_resize(state_t *state, client_t *client)
{
	XMoveResizeWindow(
			state->display,
			client->window,
			client->geometry.x,
			client->geometry.y,
			client->geometry.width,
			client->geometry.height);
}

void
client_raise(state_t *state, client_t *client)
{
	XRaiseWindow(state->display, client->window);
}

void
client_remove(state_t *state, client_t *client)
{
	client_t *current;
	group_t *group;
	int i;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				TAILQ_FOREACH(current, &group->clients, entry) {
					if (current->window == client->window) {
						group_unassign(screen->desktops[i], client);
					}
				}
			}
		}
	}
	/*
	group_t *group;

	x_ewmh_set_client_list(state);
	x_ewmh_set_client_list_stacking(state);

	if (client == client_active(state)) {
		// TODO: ewmh set net active window to none
	}

	group_unassign(state, client);
	client_free(client);

	group = TAILQ_LAST(&state->groups, group_q);
	if (!group) {
		return;
	}

	client = TAILQ_LAST(&group->clients, client_q);
	if (!client) {
		return;
	}

	client_activate(state, client);
	*/
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

	client->flags = hints->flags;
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
			client->delete_window = True;
		} else if (protocols[i] == state->atoms[WM_TAKE_FOCUS]) {
			client->take_focus = True;
		}
	}
}
