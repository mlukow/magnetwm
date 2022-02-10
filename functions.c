#include <string.h>

#include "client.h"
#include "config.h"
#include "functions.h"
#include "queue.h"
#include "screen.h"
#include "state.h"
#include "utils.h"

#define UP 0x01
#define DOWN 0x02
#define LEFT 0x04
#define RIGHT 0x08

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
