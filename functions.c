#include <dirent.h>
#include <paths.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <X11/XKBlib.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "functions.h"
#include "group.h"
#include "menu.h"
#include "queue.h"
#include "screen.h"
#include "state.h"
#include "utils.h"
#include "xutils.h"

extern sig_atomic_t wm_state;

void
function_group_cycle_callback(state_t *state, void *context)
{
	client_t *client;
	group_t *current, *group;

	group = (group_t *)context;

	TAILQ_FOREACH(current, &group->desktop->groups, entry) {
		if (current == group) {
			TAILQ_FOREACH(client, &current->clients, entry) {
				client->flags |= CLIENT_MARK;
				client_draw_border(state, client);
			}
		} else {
			TAILQ_FOREACH(client, &current->clients, entry) {
				client->flags &= ~CLIENT_MARK;
				client_draw_border(state, client);
			}
		}
	}
}

void
function_group_cycle(state_t *state, void *context, long flag)
{
	char detail[BUFSIZ];
	client_t *client;
	desktop_t *desktop;
	group_t *current, *group;
	int hidden, visible;
	menu_t *menu;
	screen_t *screen = (screen_t *)context;

	desktop = screen->desktops[screen->desktop_index];
	menu = menu_init(state, screen, NULL, True, function_group_cycle_callback);

	TAILQ_FOREACH_REVERSE(group, &desktop->groups, group_q, entry) {
		if (!group_can_activate(group)) {
			continue;
		}

		hidden = visible = 0;
		TAILQ_FOREACH(client, &group->clients, entry) {
			if (client->flags & CLIENT_HIDDEN) {
				hidden++;
			} else {
				visible++;
			}
		}

		if (hidden + visible > 0) {
			if (hidden > 0) {
				if (visible > 0) {
					sprintf(detail, "(%d visible, %d hidden)", hidden, visible);
				} else {
					sprintf(detail, "(%d hidden)", hidden);
				}
			} else {
				sprintf(detail, "(%d visible)", visible);
			}
			menu_add(menu, group, 0, group->name, detail);
		}
	}

	group = (group_t *)menu_filter(menu);

	TAILQ_FOREACH(current, &desktop->groups, entry) {
		TAILQ_FOREACH(client, &current->clients, entry) {
			client->flags &= ~CLIENT_MARK;
			client_draw_border(state, client);
		}
	}

	if (group) {
		current = TAILQ_LAST(&desktop->groups, group_q);
		if (current != group) {
			group_activate(state, group);
		}
	}

	menu_free(menu);
}

void
function_menu_command(state_t *state, void *context, long flag)
{
	command_t *command;
	menu_t *menu;
	screen_t *screen = (screen_t *)context;

	menu = menu_init(state, screen, state->config->labels[LABEL_APPLICATIONS], False, NULL);

	TAILQ_FOREACH(command, &state->config->commands, entry) {
		menu_add(menu, command, 1, command->name, NULL);
	}

	command = (command_t *)menu_filter(menu);

	if (command) {
		xspawn(command->path);
	}

	menu_free(menu);
}

void
function_menu_exec(state_t *state, void *context, long flag)
{
	char *path, *paths, tpath[PATH_MAX];
	DIR *dirp;
	int i;
	menu_t *menu;
	screen_t *screen = (screen_t *)context;
	struct dirent *dp;
	struct stat sb;

	menu = menu_init(state, screen, state->config->labels[LABEL_RUN], False, NULL);

	paths = getenv("PATH");
	if (!paths) {
		paths = _PATH_DEFPATH;
	}

	paths = strdup(paths);

	while ((path = strsep(&paths, ":")) != NULL) {
		dirp = opendir(path);
		if (!dirp) {
			continue;
		}

		while ((dp = readdir(dirp)) != NULL) {
			if (dp->d_type != DT_REG && dp->d_type != DT_LNK) {
				continue;
			}

			memset(tpath, '\0', PATH_MAX);
			i = snprintf(tpath, PATH_MAX, "%s/%s", path, dp->d_name);
			if ((i == -1) || (i >= PATH_MAX)) {
				continue;
			}

			if (lstat(tpath, &sb) == -1) {
				continue;
			}

			if (!S_ISREG(sb.st_mode) && !S_ISLNK(sb.st_mode)) {
				continue;
			}

			if (access(tpath, X_OK) == 0) {
				menu_add(menu, dp, 1, dp->d_name, NULL);
			}
		}

		closedir(dirp);
	}

	dp = (struct dirent *)menu_filter(menu);

	if (dp) {
		xspawn(dp->d_name);
	}

	menu_free(menu);
	free(paths);
}

void
function_menu_windows_callback(state_t *state, void *context)
{
	client_t *client, *current;

	client = (client_t *)context;

	TAILQ_FOREACH(current, &client->group->clients, entry) {
		if (current == client) {
			current->flags |= CLIENT_MARK;
		} else {
			current->flags &= ~CLIENT_MARK;
		}

		client_draw_border(state, current);
	}
}

void
function_menu_windows(state_t *state, void *context, long flag)
{
	client_t *client, *current;
	group_t *group;
	menu_t *menu;
	screen_t *screen;

	client = (client_t *)context;
	group = client->group;

	screen = client->group->desktop->screen;
	menu = menu_init(state, screen, state->config->labels[LABEL_WINDOWS], False, function_menu_windows_callback);

	TAILQ_FOREACH_REVERSE(client, &client->group->clients, client_q, entry) {
		if (client->type != CLIENT_TYPE_NORMAL) {
			continue;
		}
		
		if (client->flags & CLIENT_HIDDEN) {
			menu_add(menu, client, 0, client->name, state->config->labels[LABEL_WINDOW_HIDDEN]);
		} else if (client->flags & CLIENT_ACTIVE) {
			menu_add(menu, client, 0, client->name, state->config->labels[LABEL_WINDOW_ACTIVE]);
		} else {
			menu_add(menu, client, 0, client->name, state->config->labels[LABEL_WINDOW_INACTIVE]);
		}
	}

	client = (client_t *)menu_filter(menu);

	TAILQ_FOREACH(current, &group->clients, entry) {
		current->flags &= ~CLIENT_MARK;
		client_draw_border(state, current);
	}

	if (client) {
		if (client->flags & CLIENT_HIDDEN) {
			client_show(state, client);
		}

		client_raise(state, client);
		client_activate(state, client, True);
	}

	menu_free(menu);
}

void
function_terminal(struct state_t *state, void *context, long flag)
{
	command_t *command;

	TAILQ_FOREACH(command, &state->config->commands, entry) {
		if (!strcmp(command->name, "terminal")) {
			xspawn(command->path);
		}
	}
}

void
function_window_center(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	geometry_t screen_area;
	screen_t *screen;

	screen = client->group->desktop->screen;
	screen_area = screen_available_area(screen);

	client->geometry_saved = client->geometry;

	client->geometry.x = screen_area.x + (screen_area.width - client->geometry.width) / 2 - client->border_width;
	client->geometry.y = screen_area.y + (screen_area.height - client->geometry.height) / 2 - client->border_width;

	client_move_resize(state, client, True);
}

void
function_window_close(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;

	client_close(state, client);
}

void
function_window_cycle(struct state_t *state, void *context, long flag)
{
	Bool processing = True;
	client_t *client = (client_t *)context;
	KeySym keysym;
	XEvent event;

	if (flag == 0) {
		client = client_next(client);
	} else {
		client = client_previous(client);
	}

	client_raise(state, client);
	client_activate(state, client, False);
}

void
function_window_fullscreen(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;

	client_toggle_fullscreen(state, client);
}

void
function_window_hide(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;

	client_hide(state, client);
}

void
function_window_maximize(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;

	if (client->flags & CLIENT_FREEZE) {
		return;
	}

	client_toggle_maximize(state, client);
}

void
function_window_move(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	geometry_t screen_area;
	int move = 1, result, x, y;
	screen_t *screen;
	Time time = 0;
	XEvent event;

	if (!x_get_pointer(state->display, client->window, &x, &y)) {
		return;
	}

	result = XGrabPointer(
			state->display,
			client->window,
			False,
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			state->cursors[CURSOR_MOVE],
			CurrentTime);
	if (result != GrabSuccess) {
		return;
	}

	client->geometry_saved = client->geometry;

	screen = client->group->desktop->screen;
	screen_area = screen_available_area(screen);

	while (move) {
		XMaskEvent(state->display, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &event);
		switch (event.type) {
			case MotionNotify:
				if (event.xmotion.time - time <= 1000 / 60) {
					continue;
				}

				time = event.xmotion.time;

				client->geometry.x += event.xmotion.x_root - x;
				client->geometry.y += event.xmotion.y_root - y;
				if (client->geometry.y < screen_area.y) {
					client->geometry.y = screen_area.y;
				}

				x = event.xmotion.x_root;
				y = event.xmotion.y_root;

				client_move_resize(state, client, True);

				break;
			case ButtonRelease:
				move = 0;
				break;
		}
	}

	XUngrabPointer(state->display, CurrentTime);

	screen = screen_for_client(state, client);
	if (screen != client->group->desktop->screen) {
		screen_adopt(state, screen, client);
	}

	XFlush(state->display);
}

void
function_window_move_to_screen(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	geometry_t screen_area_new, screen_area_old;
	screen_t *screen = NULL;

	if (flag == DIRECTION_UP) {
		screen = screen_find_above(state, client->group->desktop->screen);
	} else if (flag == DIRECTION_DOWN) {
		screen = screen_find_below(state, client->group->desktop->screen);
	} else if (flag == DIRECTION_LEFT) {
		screen = screen_find_left(state, client->group->desktop->screen);
	} else if (flag == DIRECTION_RIGHT) {
		screen = screen_find_right(state, client->group->desktop->screen);
	}

	if (screen) {
		screen_area_new = screen_available_area(screen);
		screen_area_old = screen_available_area(client->group->desktop->screen);

		client->geometry.x = screen_area_new.x + (client->geometry.x - screen_area_old.x);
		client->geometry.y = screen_area_new.y + (client->geometry.y - screen_area_old.y);

		client_move_resize(state, client, False);
		screen_adopt(state, screen, client);
	}
}

void
function_window_resize(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	Cursor cursor;
	int resize = 1, original_height, original_width, result, x, y;
	Time time = 0;
	XEvent event;

	if (!x_get_pointer(state->display, client->window, &x, &y)) {
		return;
	}

	if (x < client->geometry.x + client->geometry.width / 2) {
		if (y < client->geometry.y + client->geometry.height / 2) {
			cursor = state->cursors[CURSOR_RESIZE_TOP_LEFT];
		} else {
			cursor = state->cursors[CURSOR_RESIZE_BOTTOM_LEFT];
		}
	} else {
		if (y < client->geometry.y + client->geometry.height / 2) {
			cursor = state->cursors[CURSOR_RESIZE_TOP_RIGHT];
		} else {
			cursor = state->cursors[CURSOR_RESIZE_BOTTOM_RIGHT];
		}
	}

	result = XGrabPointer(
			state->display,
			client->window,
			False,
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			cursor,
			CurrentTime);
	if (result != GrabSuccess) {
		return;
	}

	original_width = client->geometry.width;
	original_height = client->geometry.height;
	
	client->geometry_saved = client->geometry;

	while (resize) {
		XMaskEvent(state->display, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &event);
		switch (event.type) {
			case MotionNotify:
				if (event.xmotion.time - time <= 1000 / 60) {
					continue;
				}

				time = event.xmotion.time;

				client->geometry.width = original_width + event.xmotion.x_root - x;
				client->geometry.height = original_height + event.xmotion.y_root - y;

				client_apply_size_hints(state, client);
				client_move_resize(state, client, True);

				break;
			case ButtonRelease:
				resize = 0;
				break;
		}
	}

	XUngrabPointer(state->display, CurrentTime);
	XFlush(state->display);
}

void
function_window_restore(state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;

	if ((client->geometry_saved.width == 0) || (client->geometry_saved.height == 0)) {
		return;
	}

	client_restore(state, client);
}

void
function_window_tile(state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	geometry_t screen_area;
	screen_t *screen;

	screen = client->group->desktop->screen;
	screen_area = screen_available_area(screen);

	client->geometry_saved = client->geometry;

	if (flag & DIRECTION_UP) {
		client->geometry.y = screen_area.y;
		client->geometry.height = screen_area.height / 2 - 2 * client->border_width;
	} else if (flag & DIRECTION_UP_THIRD) {
		client->geometry.y = screen_area.y;
		client->geometry.height = screen_area.height / 3 - 2 * client->border_width;
	} else if (flag & DIRECTION_DOWN) {
		client->geometry.y = screen_area.y + screen_area.height / 2;
		client->geometry.height = screen_area.height / 2 - 2 * client->border_width;
	} else if (flag & DIRECTION_DOWN_THIRD) {
		client->geometry.y = screen_area.y + screen_area.height - screen_area.height / 3;
		client->geometry.height = screen_area.height / 3 - 2 * client->border_width;
	} else {
		client->geometry.y = screen_area.y;
		client->geometry.height = screen_area.height - 2 * client->border_width;
	}

	if (flag & DIRECTION_LEFT) {
		client->geometry.x = screen_area.x;
		client->geometry.width = screen_area.width / 2 - 2 * client->border_width;
	} else if (flag & DIRECTION_LEFT_THIRD) {
		client->geometry.x = screen_area.x;
		client->geometry.width = screen_area.width / 3 - 2 * client->border_width;
	} else if (flag & DIRECTION_RIGHT) {
		client->geometry.x = screen_area.x + screen_area.width / 2;
		client->geometry.width = screen_area.width / 2 - 2 * client->border_width;
	} else if (flag & DIRECTION_RIGHT_THIRD) {
		client->geometry.x = screen_area.x + screen_area.width - screen_area.width / 3;
		client->geometry.width = screen_area.width / 3 - 2 * client->border_width;
	} else {
		client->geometry.x = screen_area.x;
		client->geometry.width = screen_area.width - 2 * client->border_width;
	}

	client_apply_size_hints(state, client);
	client_move_resize(state, client, True);
}

void
function_wm_state(state_t *state, void *context, long flag)
{
	wm_state = flag;
}
