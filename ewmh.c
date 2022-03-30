#include <X11/Xatom.h>

#include "client.h"
#include "desktop.h"
#include "ewmh.h"
#include "group.h"
#include "screen.h"
#include "state.h"
#include "xutils.h"

void
ewmh_free(ewmh_t *ewmh)
{
	if (!ewmh) {
		return;
	}

	free(ewmh);
}

Bool
ewmh_get_net_wm_desktop(state_t *state, client_t *client, long *index)
{
	long *result;

	if (x_get_property(
				state->display,
				client->window,
				state->ewmh->atoms[_NET_WM_DESKTOP],
				XA_CARDINAL,
				1,
				(unsigned char **)&result) <= 0) {
		return False;
	}

	*index = *result;
	XFree((char *)result);

	return True;
}

Atom *
ewmh_get_net_wm_state(state_t *state, client_t *client, int *count)
{
	Atom *result;
	unsigned char *output;

	*count = x_get_property(state->display, client->window, state->ewmh->atoms[_NET_WM_STATE], XA_ATOM, 64L, &output);
	if (*count <= 0) {
		return NULL;
	}

	result = calloc(*count, sizeof(Atom));
	(void)memcpy(result, output, *count * sizeof(Atom));
	XFree(output);

	return result;
}

Bool
ewmh_get_net_wm_strut(state_t *state, client_t *client)
{
	int count;
	screen_t *screen;
	unsigned char *output;

	count = x_get_property(
			state->display,
			client->window,
			state->ewmh->atoms[_NET_WM_STRUT],
			XA_CARDINAL,
			32L,
			&output);
	if (count < 4) {
		return False;
	}

	screen = client->group->desktop->screen;

	client->strut.left = ((long *)output)[0];
	client->strut.right = ((long *)output)[1];
	client->strut.top = ((long *)output)[2];
	client->strut.bottom = ((long *)output)[3];

	client->strut.left_start_y = 0;
	client->strut.left_end_y = client->strut.left == 0 ? 0 : screen->geometry.height - 1;
	client->strut.right_start_y = 0;
	client->strut.right_end_y = client->strut.right == 0 ? 0 : screen->geometry.height - 1;
	client->strut.top_start_x = 0;
	client->strut.top_end_x = client->strut.top == 0 ? 0 : screen->geometry.width - 1;
	client->strut.bottom_start_x = 0;
	client->strut.bottom_end_x = client->strut.bottom == 0 ? 0 : screen->geometry.width - 1;

	return True;
}

Bool
ewmh_get_net_wm_strut_partial(state_t *state, client_t *client)
{
	int count;
	unsigned char *output;

	count = x_get_property(
			state->display,
			client->window,
			state->ewmh->atoms[_NET_WM_STRUT_PARTIAL],
			XA_CARDINAL,
			32L,
			&output);
	if (count < 12) {
		return False;
	}

	client->strut.left = ((long *)output)[0];
	client->strut.right = ((long *)output)[1];
	client->strut.top = ((long *)output)[2];
	client->strut.bottom = ((long *)output)[3];

	client->strut.left_start_y = ((long *)output)[4];
	client->strut.left_end_y = ((long *)output)[5];
	client->strut.right_start_y = ((long *)output)[6];
	client->strut.right_end_y = ((long *)output)[7];
	client->strut.top_start_x = ((long *)output)[8];
	client->strut.top_end_x = ((long *)output)[9];
	client->strut.bottom_start_x = ((long *)output)[10];
	client->strut.bottom_end_x = ((long *)output)[11];

	return True;
}

void
ewmh_get_wm_window_type(state_t *state, client_t *client)
{
	Atom atom;
	int count;
	unsigned char *output;

	count = x_get_property(
			state->display,
			client->window,
			state->ewmh->atoms[_NET_WM_WINDOW_TYPE],
			XA_ATOM,
			64L,
			&output);
	if (count < 1) {
		atom = state->ewmh->atoms[_NET_WM_WINDOW_TYPE_NORMAL];
	} else {
		atom = ((Atom *)output)[0];
		free(output);
	}

	if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_DESKTOP]) {
		client->type = CLIENT_TYPE_DESKTOP;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_DOCK]) {
		client->type = CLIENT_TYPE_DOCK;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_TOOLBAR]) {
		client->type = CLIENT_TYPE_TOOLBAR;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_MENU]) {
		client->type = CLIENT_TYPE_MENU;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_UTILITY]) {
		client->type = CLIENT_TYPE_UTILITY;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_SPLASH]) {
		client->type = CLIENT_TYPE_SPLASH;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_DIALOG]) {
		client->type = CLIENT_TYPE_DIALOG;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_DROPDOWN_MENU]) {
		client->type = CLIENT_TYPE_DROPDOWN_MENU;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_POPUP_MENU]) {
		client->type = CLIENT_TYPE_POPUP_MENU;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_TOOLTIP]) {
		client->type = CLIENT_TYPE_TOOLTIP;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_NOTIFICATION]) {
		client->type = CLIENT_TYPE_NOTIFICATION;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_COMBO]) {
		client->type = CLIENT_TYPE_COMBO;
	} else if (atom == state->ewmh->atoms[_NET_WM_WINDOW_TYPE_DND]) {
		client->type = CLIENT_TYPE_DND;
	} else {
		client->type = CLIENT_TYPE_NORMAL;
	}
}

void
ewmh_handle_net_wm_state_message(state_t *state, client_t *client, int action, Atom first, Atom second)
{
	unsigned int i;
	struct handlers {
		Atom atom;
		int flag;
		void (*toggle)(state_t *, client_t *);
	} handlers[] = {
		{ _NET_WM_STATE_STICKY, CLIENT_STICKY, client_toggle_sticky },
		{ _NET_WM_STATE_MAXIMIZED_VERT, CLIENT_VMAXIMIZED, client_toggle_vmaximize },
		{ _NET_WM_STATE_MAXIMIZED_HORZ, CLIENT_HMAXIMIZED, client_toggle_hmaximize },
		{ _NET_WM_STATE_HIDDEN, CLIENT_HIDDEN, client_toggle_hidden },
		{ _NET_WM_STATE_FULLSCREEN, CLIENT_FULLSCREEN, client_toggle_fullscreen },
		{ _NET_WM_STATE_DEMANDS_ATTENTION, CLIENT_URGENCY, client_toggle_urgent },
		{ _NET_WM_STATE_SKIP_PAGER, CLIENT_SKIP_PAGER, client_toggle_skip_pager },
		{ _NET_WM_STATE_SKIP_TASKBAR, CLIENT_SKIP_TASKBAR, client_toggle_skip_taskbar },
	};

	for (i = 0; i < 8; i++) {
		if ((first != state->ewmh->atoms[handlers[i].atom]) && (second != state->ewmh->atoms[handlers[i].atom])) {
			continue;
		}

		switch (action) {
			case _NET_WM_STATE_ADD:
				if (!(client->flags & handlers[i].flag)) {
					handlers[i].toggle(state, client);
				}

				break;
			case _NET_WM_STATE_REMOVE:
				if (client->flags & handlers[i].flag) {
					handlers[i].toggle(state, client);
				}

				break;
			case _NET_WM_STATE_TOGGLE:
				handlers[i].toggle(state, client);
				break;
		}
	}
}

void
ewmh_handle_property(state_t *state, client_t *client, Atom type)
{
	if (type == state->ewmh->atoms[_NET_WM_NAME]) {
		client_update_wm_name(state, client);
	} else if (type == state->ewmh->atoms[_NET_WM_STRUT]) {
		ewmh_get_net_wm_strut(state, client);
	} else if (type == state->ewmh->atoms[_NET_WM_STRUT_PARTIAL]) {
		ewmh_get_net_wm_strut_partial(state, client);
	}
}

ewmh_t *
ewmh_init(state_t *state)
{
	char *names[] = {
		"_NET_ACTIVE_WINDOW",
		"_NET_CLIENT_LIST",
		"_NET_CLIENT_LIST_STACKING",
		"_NET_CLOSE_WINDOW",
		"_NET_CURRENT_DESKTOP",
		"_NET_DESKTOP_GEOMETRY",
		"_NET_DESKTOP_NAMES",
		"_NET_DESKTOP_VIEWPORT",
		"_NET_FRAME_EXTENTS",
		"_NET_NUMBER_OF_DESKTOPS",
		"_NET_SHOWING_DESKTOP",
		"_NET_SUPPORTED",
		"_NET_SUPPORTING_WM_CHECK",
		"_NET_WM_DESKTOP",
		"_NET_WM_ICON",
		"_NET_WM_NAME",
		"_NET_WM_STATE",
		"_NET_WM_STATE_DEMANDS_ATTENTION",
		"_NET_WM_STATE_FULLSCREEN",
		"_NET_WM_STATE_HIDDEN",
		"_NET_WM_STATE_MAXIMIZED_HORZ",
		"_NET_WM_STATE_MAXIMIZED_VERT",
		"_NET_WM_STATE_SKIP_PAGER",
		"_NET_WM_STATE_SKIP_TASKBAR",
		"_NET_WM_STATE_STICKY",
		"_NET_WM_STRUT",
		"_NET_WM_STRUT_PARTIAL",
		"_NET_WM_WINDOW_TYPE",
		"_NET_WM_WINDOW_TYPE_DESKTOP",
		"_NET_WM_WINDOW_TYPE_DOCK",
		"_NET_WM_WINDOW_TYPE_TOOLBAR",
		"_NET_WM_WINDOW_TYPE_MENU",
		"_NET_WM_WINDOW_TYPE_UTILITY",
		"_NET_WM_WINDOW_TYPE_SPLASH",
		"_NET_WM_WINDOW_TYPE_DIALOG",
		"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
		"_NET_WM_WINDOW_TYPE_POPUP_MENU",
		"_NET_WM_WINDOW_TYPE_TOOLTIP",
		"_NET_WM_WINDOW_TYPE_NOTIFICATION",
		"_NET_WM_WINDOW_TYPE_COMBO",
		"_NET_WM_WINDOW_TYPE_DND",
		"_NET_WM_WINDOW_TYPE_NORMAL",
	};
	ewmh_t *ewmh;

	ewmh = calloc(1, sizeof(ewmh_t));
	if (!XInternAtoms(state->display, names, EWMH_NITEMS, False, ewmh->atoms)) {
		free(ewmh);
		return NULL;
	}

	return ewmh;
}

void
ewmh_restore_net_wm_state(state_t *state, client_t *client)
{
	Atom *atoms;
	int count, i;

	atoms = ewmh_get_net_wm_state(state, client, &count);
	for (i = 0; i < count; i++) {
		if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_STICKY]) {
			client_toggle_sticky(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_MAXIMIZED_VERT]) {
			client_toggle_vmaximize(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_MAXIMIZED_HORZ]) {
			client_toggle_hmaximize(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_HIDDEN]) {
			client_toggle_hidden(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_FULLSCREEN]) {
			client_toggle_fullscreen(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_DEMANDS_ATTENTION]) {
			client_toggle_urgent(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_SKIP_PAGER]) {
			client_toggle_skip_pager(state, client);
		} else if (atoms[i] == state->ewmh->atoms[_NET_WM_STATE_SKIP_TASKBAR]) {
			client_toggle_skip_taskbar(state, client);
		}
	}

	free(atoms);
}

void
ewmh_set_net_active_window(state_t *state, client_t *client)
{
	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_ACTIVE_WINDOW],
			XA_WINDOW,
			32,
			PropModeReplace, 
			(unsigned char *)&client->window,
			1);
}

void
ewmh_set_net_client_list(state_t *state)
{
	client_t *client;
	group_t *group;
	int i, windows_count = 0;
	screen_t *screen;
	Window *windows;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				TAILQ_FOREACH(client, &group->clients, entry) {
					if (windows_count == 0) {
						windows = calloc(1, sizeof(Window));
					} else {
						windows = realloc(windows, (windows_count + 1) * sizeof(Window));
					}

					windows[windows_count++] = client->window;
				}
			}
		}
	}

	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_CLIENT_LIST],
			XA_WINDOW,
			32,
			PropModeReplace,
			(unsigned char *)windows,
			windows_count);

	if (windows_count > 0) {
		free(windows);
	}
}

void
ewmh_set_net_client_list_stacking(state_t *state)
{
	// TODO: implement me
}

void
ewmh_set_net_current_desktop(state_t *state)
{
	long index = 0;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (screen->active) {
			index += screen->desktop_index;
			break;
		} else {
			index += screen->desktop_count;
		}
	}

	ewmh_set_net_current_desktop_index(state, index);
}

void
ewmh_set_net_current_desktop_index(state_t *state, long index)
{
	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_CURRENT_DESKTOP],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)&index,
			1);
}

void
ewmh_set_net_desktop_geometry(state_t *state)
{
	int i, geometries_count = 0;
	long *geometries;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (geometries_count == 0) {
			geometries = calloc(2 * screen->desktop_count, sizeof(long));
		} else {
			geometries = realloc(geometries, (geometries_count + 2 * screen->desktop_count) * sizeof(long));
		}   

		for (i = 0; i < screen->desktop_count; i++) {
			geometries[geometries_count + 2 * i] = screen->geometry.width;
			geometries[geometries_count + 2 * i + 1] = screen->geometry.height;
		}   

		geometries_count += 2 * screen->desktop_count;
	}   

	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_DESKTOP_VIEWPORT],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)geometries,
			geometries_count);

	if (geometries_count > 0) {
		free(geometries); 
	}
}

void
ewmh_set_net_desktop_names(state_t *state)
{
	char *names = NULL;
	int i, names_count = 0;
	screen_t *screen;
	ssize_t name_length;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			name_length = strlen(screen->desktops[i]->name);
			if (names_count == 0) {
				names = calloc(name_length + 1, sizeof(char));
			} else {
				names = realloc(names, (names_count + name_length + 1) * sizeof(char));
			}   

			memcpy(names + names_count, screen->desktops[i]->name, name_length);
			names_count += name_length; 
			names[names_count++] = '\0';
		}   
	}   

	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_DESKTOP_NAMES],
			XA_STRING,
			8,
			PropModeReplace,
			(unsigned char *)names,
			names_count); 

	if (names_count > 0) {
		free(names); 
	}
}

void
ewmh_set_net_desktop_viewport(state_t *state)
{
	int i, viewports_count = 0;
	long *viewports;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (viewports_count == 0) {
			viewports = calloc(2 * screen->desktop_count, sizeof(long));
		} else {
			viewports = realloc(viewports, (viewports_count + 2 * screen->desktop_count) * sizeof(long));
		}

		for (i = 0; i < screen->desktop_count; i++) {
			viewports[viewports_count + 2 * i] = screen->geometry.x;
			viewports[viewports_count + 2 * i + 1] = screen->geometry.y;
		}

		viewports_count += 2 * screen->desktop_count;
	}

	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_DESKTOP_VIEWPORT],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)viewports,
			viewports_count);

	if (viewports_count > 0) {
		free(viewports);
	}
}

void
ewmh_set_net_frame_extents(state_t *state, client_t *client)
{
	long extents[] = { client->border_width, client->border_width, client->border_width, client->border_width };

	XChangeProperty(
			state->display,
			client->window,
			state->ewmh->atoms[_NET_FRAME_EXTENTS],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)extents,
			4);
}

void
ewmh_set_net_number_of_desktops(state_t *state)
{
	int count = 0;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		count += screen->desktop_count;
	}   

	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_NUMBER_OF_DESKTOPS],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)&count,
			1);
}

void
ewmh_set_net_showing_desktop(state_t *state, Bool showing_desktop)
{
	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_SHOWING_DESKTOP],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)&showing_desktop,
			1);
}

void
ewmh_set_net_supported(state_t *state)
{
	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_SUPPORTED],
			XA_ATOM,
			32,
			PropModeReplace,
			(unsigned char *)&state->ewmh->atoms,
			EWMH_NITEMS);
}

void
ewmh_set_net_wm_state(state_t *state, client_t *client)
{
	Atom *input, *output;
	int count, i, j;

	input = ewmh_get_net_wm_state(state, client, &count);
	output = calloc(count + 9, sizeof(Atom));

	for (i = j = 0; i < count; i++) {
		if ((input[i] != state->ewmh->atoms[_NET_WM_STATE_STICKY]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_MAXIMIZED_VERT]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_MAXIMIZED_HORZ]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_HIDDEN]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_FULLSCREEN]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_DEMANDS_ATTENTION]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_SKIP_PAGER]) &&
				(input[i] != state->ewmh->atoms[_NET_WM_STATE_SKIP_TASKBAR])) {
			output[j++] = input[i];
		}
	}

	free(input);

	if (client->flags & CLIENT_STICKY) {
		output[j++] = state->ewmh->atoms[_NET_WM_STATE_STICKY];
	}

	if (client->flags & CLIENT_HIDDEN) {
		output[j++] = state->ewmh->atoms[_NET_WM_STATE_HIDDEN];
	}

	if (client->flags & CLIENT_FULLSCREEN) {
		output[j++] = state->ewmh->atoms[_NET_WM_STATE_FULLSCREEN];
	} else {
		if (client->flags & CLIENT_VMAXIMIZED) {
			output[j++] = state->ewmh->atoms[_NET_WM_STATE_MAXIMIZED_VERT];
		}

		if (client->flags & CLIENT_HMAXIMIZED) {
			output[j++] = state->ewmh->atoms[_NET_WM_STATE_MAXIMIZED_HORZ];
		}
	}

	if (client->flags & CLIENT_URGENCY) {
		output[j++] = state->ewmh->atoms[_NET_WM_STATE_DEMANDS_ATTENTION];
	}

	if (client->flags & CLIENT_SKIP_PAGER) {
		output[j++] = state->ewmh->atoms[_NET_WM_STATE_SKIP_PAGER];
	}

	if (client->flags & CLIENT_SKIP_TASKBAR) {
		output[j++] = state->ewmh->atoms[_NET_WM_STATE_SKIP_TASKBAR];
	}

	if (j > 0) {
		XChangeProperty(
				state->display,
				client->window,
				state->ewmh->atoms[_NET_WM_STATE],
				XA_ATOM,
				32,
				PropModeReplace,
				(unsigned char *)output,
				j);
	} else {
		XDeleteProperty(state->display, client->window, state->ewmh->atoms[_NET_WM_STATE]);
	}

	free(output);
}

void
ewmh_set_net_workarea(state_t *state)
{
	int i, workareas_count = 0;
	long *worksareas;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (workareas_count == 0) {
			worksareas = calloc(4 * screen->desktop_count, sizeof(long));
		} else {
			worksareas = realloc(worksareas, (workareas_count + 4 * screen->desktop_count) * sizeof(long));
		}

		for (i = 0; i < screen->desktop_count; i++) {
			worksareas[workareas_count + 4 * i] = screen->geometry.x;
			worksareas[workareas_count + 4 * i + 1] = screen->geometry.y;
			worksareas[workareas_count + 4 * i + 2] = screen->geometry.width;
			worksareas[workareas_count + 4 * i + 3] = screen->geometry.height;
		}

		workareas_count += 4 * screen->desktop_count;
	}

	XChangeProperty(
			state->display,
			state->root,
			state->ewmh->atoms[_NET_DESKTOP_VIEWPORT],
			XA_CARDINAL,
			32,
			PropModeReplace,
			(unsigned char *)worksareas,
			workareas_count);

	if (workareas_count > 0) {
		free(worksareas);
	}
}
