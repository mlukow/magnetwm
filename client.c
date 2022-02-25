#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xutil.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "ewmh.h"
#include "group.h"
#include "icccm.h"
#include "screen.h"
#include "state.h"
#include "utils.h"
#include "xutils.h"

void client_configure(state_t *, client_t *);
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
		icccm_take_focus(state, client);
	}

	TAILQ_REMOVE(&client->group->clients, client, entry);
	TAILQ_INSERT_TAIL(&client->group->clients, client, entry);

	client->flags |= CLIENT_ACTIVE;
	client->flags &= ~CLIENT_URGENCY;

	client_draw_border(state, client);
	ewmh_set_net_active_window(state, client);
}

void
client_close(state_t *state, client_t *client)
{
	if (client->flags & CLIENT_WM_DELETE_WINDOW) {
		icccm_delete_window(state, client);
	} else {
		XKillClient(state->display, client->window);
	}
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
	ewmh_set_net_active_window(state, client);
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
		ewmh_set_net_active_window(state, client);
	}

	client->flags |= CLIENT_HIDDEN;
	icccm_set_wm_state(state, client, IconicState);
}

client_t *
client_init(state_t *state, Window window, Bool initial)
{
	client_t *client;
	ignored_t *ignored;
	screen_t *screen;
	XWindowAttributes attributes;

	if (window == None) {
		return NULL;
	}

	if (!XGetWindowAttributes(state->display, window, &attributes)) {
		return NULL;
	}

	if (initial) {
		if (attributes.override_redirect) {
			return NULL;
		}

		if (attributes.map_state != IsViewable) {
			return NULL;
		}
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
	client->geometry_saved = client->geometry;

	screen = screen_for_client(state, client);
	if (!screen) {
		free(client);
		return NULL;
	}

	client_update_class(state, client);
	client_update_wm_name(state, client);
	client_update_size_hints(state, client);
	client_update_wm_hints(state, client);

	icccm_restore_wm_protocols(state, client);

	if (attributes.map_state != IsViewable) {
		if (client->initial_state) {
			icccm_set_wm_state(state, client, client->initial_state);
		}
	}

	screen_adopt(state, screen, client);

	ewmh_restore_net_wm_state(state, client);

	TAILQ_FOREACH(ignored, &state->config->ignored, entry) {
		if (!strcmp(ignored->class_name, client->class_name)) {
			client->flags |= CLIENT_IGNORE;
			client->border_width = 0;
		}
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
		ewmh_set_net_wm_state(state, client);
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

client_t *
client_next(client_t *client)
{
	client_t *next;

	while (((next = TAILQ_NEXT(client, entry)) != NULL) && (next->flags & CLIENT_IGNORE));
	if (!next) {
		next = TAILQ_FIRST(&client->group->clients);
		while ((next->flags & CLIENT_IGNORE) && ((next = TAILQ_NEXT(client, entry)) != NULL));
	}

	return next;
}

client_t *
client_previous(client_t *client)
{
	client_t *previous;

	while (((previous = TAILQ_PREV(client, client_q, entry)) != NULL) && (previous->flags & CLIENT_IGNORE));
	if (!previous) {
		previous = TAILQ_LAST(&client->group->clients, client_q);
		while ((previous->flags & CLIENT_IGNORE) && ((previous = TAILQ_PREV(client, client_q, entry)) != NULL));
	}

	return previous;
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
	*/

	if (client->flags & CLIENT_ACTIVE) {
		ewmh_set_net_active_window(state, client);
	}

	group = client->group;
	group_unassign(client);
	client_free(client);

	TAILQ_FOREACH_REVERSE(client, &group->clients, client_q, entry) {
		if (!(client->flags & CLIENT_IGNORE)) {
			client_raise(state, client);
			client_activate(state, client);
			return;
		}
	}

	group = TAILQ_LAST(&group->desktop->groups, group_q);
	if (group) {
		TAILQ_FOREACH(client, &group->clients, entry) {
			client_raise(state, client);
		}

		TAILQ_FOREACH_REVERSE(client, &group->clients, client_q, entry) {
			if (!(client->flags & CLIENT_IGNORE)) {
				client_activate(state, client);
				break;
			}
		}
	}
}

void
client_restore(state_t *state, client_t *client)
{
	if ((client->geometry_saved.width > 0) && (client->geometry_saved.height > 0)) {
		client->flags &= ~CLIENT_HMAXIMIZED;
		client->flags &= ~CLIENT_VMAXIMIZED;

		client->geometry = client->geometry_saved;

		client_move_resize(state, client, False);
		ewmh_set_net_wm_state(state, client);
	}
}

void
client_show(state_t *state, client_t *client)
{
	XMapWindow(state->display, client->window);

	client->flags &= ~CLIENT_HIDDEN;
	icccm_set_wm_state(state, client, NormalState);
	client_draw_border(state, client);
}

void
client_toggle_freeze(state_t *state, client_t *client)
{
	if (client->flags & CLIENT_FULLSCREEN) {
		return;
	}

	client->flags ^= CLIENT_FREEZE;
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_fullscreen(state_t *state, client_t *client)
{
	if ((client->flags & CLIENT_FREEZE) && !(client->flags & CLIENT_FULLSCREEN)) {
		return;
	}

	if (client->flags & CLIENT_FULLSCREEN) {
		if (!(client->flags & CLIENT_IGNORE)) {
			client->border_width = state->config->border_width;
		}

		client->geometry = client->geometry_saved;
		client->flags &= ~(CLIENT_FULLSCREEN | CLIENT_FREEZE);
	} else {
		client->geometry_saved = client->geometry;
		client->border_width = 0;
		client->geometry = client->group->desktop->screen->geometry;
		client->flags |= (CLIENT_FULLSCREEN | CLIENT_FREEZE);
	}

	client_move_resize(state, client, False);
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_hidden(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_HIDDEN;
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_hmaximize(state_t *state, client_t *client)
{
	screen_t *screen;

	if (client->flags & CLIENT_FREEZE) {
		return;
	}

	screen = client->group->desktop->screen;
	client->geometry.x = screen->geometry.x;
	client->geometry.width = screen->geometry.width - 2 * client->border_width;

	client_move_resize(state, client, False);
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_maximize(state_t *state, client_t *client)
{
	screen_t *screen;

	if (client->flags & CLIENT_FREEZE) {
		return;
	}

	screen = client->group->desktop->screen;

	if ((client->flags & CLIENT_MAXFLAGS) == CLIENT_MAXIMIZED) {
		if ((client->geometry_saved.width > 0) && (client->geometry_saved.height > 0)) {
			client->geometry = client->geometry_saved;
		}
		client->flags &= ~CLIENT_MAXIMIZED;
	} else {
		client->geometry_saved = client->geometry;

		client->geometry.x = screen->geometry.x;
		client->geometry.y = screen->geometry.y;
		client->geometry.width = screen->geometry.width - 2 * client->border_width;
		client->geometry.height = screen->geometry.height - 2 * client->border_width;

		client->flags |= CLIENT_MAXIMIZED;
	}

	client_move_resize(state, client, False);
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_skip_pager(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_SKIP_PAGER;
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_skip_taskbar(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_SKIP_TASKBAR;
	ewmh_set_net_wm_state(state, client);
}

void
client_toggle_sticky(state_t *state, client_t *client)
{
	client->flags ^= CLIENT_STICKY;
	ewmh_set_net_wm_state(state, client);
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

	screen = client->group->desktop->screen;
	client->geometry.y = screen->geometry.y;
	client->geometry.width = screen->geometry.height - 2 * client->border_width;

	client_move_resize(state, client, False);
	ewmh_set_net_wm_state(state, client);
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

	client->icon = hints->icon_pixmap;
	client->mask = hints->icon_mask;

	XFree(hints);
}

void
client_update_wm_name(state_t *state, client_t *client)
{
	XTextProperty text;

	/*
	if (XGetTextProperty(state->display, client->window, &text, state->icccm[_NET_WM_NAME]) != Success) {
		if (!XGetWMName(state->display, client->window, &text)) {
			return;
		}
	}

	if (text.nitems == 0) {
	*/
		if (!XGetWMName(state->display, client->window, &text)) {
			return;
		}
		/*
	}
	*/

	client->name = strdup((char *)text.value);
	XFree(text.value);
}
