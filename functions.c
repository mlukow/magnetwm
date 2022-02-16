#include <dirent.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "config.h"
#include "functions.h"
#include "group.h"
#include "menu.h"
#include "queue.h"
#include "screen.h"
#include "state.h"
#include "utils.h"

#define UP 0x01
#define DOWN 0x02
#define LEFT 0x04
#define RIGHT 0x08

char *format_command(void *);
char *format_group(void *);
char *format_path(void *);

char *
format_command(void *context)
{
	return ((command_t *)context)->name;
}

char *
format_group(void *context)
{
	return ((group_t *)context)->name;
}

char *
format_path(void *context)
{
	return (char *)context;
}

void
function_menu_command(state_t *state, void *context, long flag)
{
	command_t *command;
	menu_t items;
	screen_t *screen = (screen_t *)context;

	TAILQ_INIT(&items);

	TAILQ_FOREACH(command, &state->config->commands, entry) {
		menu_add(&items, command, 1, format_command);
	}

	command = (command_t *)menu_filter(state, screen, &items, NULL, format_command);

	if (command) {
		xspawn(command->path);
	}
}

void
function_menu_exec(state_t *state, void *context, long flag)
{
	char *path, *paths, tpath[PATH_MAX];
	DIR *dirp;
	int i;
	menu_t items;
	screen_t *screen = (screen_t *)context;
	struct dirent *dp;
	struct stat sb;

	TAILQ_INIT(&items);

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
				menu_add(&items, strdup(dp->d_name), 1, format_path);
			}
		}

		closedir(dirp);
	}

	path = (char *)menu_filter(state, screen, &items, "Run", format_path);

	if (path) {
		xspawn(path);
	}

	free(paths);
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
	screen_t *screen;

	screen = screen_for_client(state, client);

	client->geometry.x = (screen->geometry.width - client->geometry.width) / 2 - client->border_width;
	client->geometry.y = (screen->geometry.height - client->geometry.height) / 2 - client->border_width;

	client_move_resize(state, client, False);
}

void
function_window_move(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	int move = 1, result, x, y;
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
	XFlush(state->display);
}

void
function_window_resize(struct state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	Cursor cursor;
	int resize = 1, result, x, y;
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

	while (resize) {
		XMaskEvent(state->display, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &event);
		switch (event.type) {
			case MotionNotify:
				if (event.xmotion.time - time <= 1000 / 60) {
					continue;
				}

				time = event.xmotion.time;

				if (x < client->geometry.x + client->geometry.width / 2) {
					client->geometry.x += event.xmotion.x_root - x;
					client->geometry.width -= event.xmotion.x_root - x;
				} else {
					client->geometry.width += event.xmotion.x_root - x;
				}

				if (y < client->geometry.y + client->geometry.height / 2) {
					client->geometry.y += event.xmotion.y_root - y;
					client->geometry.height -= event.xmotion.y_root - y;
				} else {
					client->geometry.height += event.xmotion.y_root - y;
				}

				x = event.xmotion.x_root;
				y = event.xmotion.y_root;

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
function_window_tile(state_t *state, void *context, long flag)
{
	client_t *client = (client_t *)context;
	screen_t *screen;

	screen = screen_for_client(state, client);

	if (flag & UP) {
		client->geometry.y = 0;
		client->geometry.height = screen->geometry.height / 2 - 2 * client->border_width;
	} else if (flag & DOWN) {
		client->geometry.y = screen->geometry.height / 2;
		client->geometry.height = screen->geometry.height / 2 - 2 * client->border_width;
	} else {
		client->geometry.y = 0;
		client->geometry.height = screen->geometry.height - 2 * client->border_width;
	}

	if (flag & LEFT) {
		client->geometry.x = 0;
		client->geometry.width = screen->geometry.width / 2 - 2 * client->border_width;
	} else if (flag & RIGHT) {
		client->geometry.x = screen->geometry.width / 2;
		client->geometry.width = screen->geometry.width / 2 - 2 * client->border_width;
	} else {
		client->geometry.x = 0;
		client->geometry.width = screen->geometry.width - 2 * client->border_width;
	}

	client_move_resize(state, client, False);
}
