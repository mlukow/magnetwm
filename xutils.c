#include <stdlib.h>

#include <X11/Xatom.h>

#include "client.h"
#include "group.h"
#include "queue.h"
#include "state.h"
#include "xutils.h"

Bool
x_contains_point(geometry_t geometry, int x, int y)
{
	return
		(x >= geometry.x) &&
		(x <= geometry.x + geometry.width) &&
		(y >= geometry.y) &&
		(y <= geometry.y + geometry.height);
}

int
x_distance(geometry_t geometry, int x, int y)
{
	return abs(geometry.x + geometry.width / 2 - x) + abs(geometry.y + geometry.height / 2 - y);
}

Bool
x_get_pointer(Display *display, Window root, int *x, int *y)
{
	int window_x, window_y;
	unsigned int mask;
	Window window;

	return XQueryPointer(display, root, &window, &window, x, y, &window_x, &window_y, &mask);
}

int
x_get_property(Display *display, Window window, Atom atom, Atom type, long length, unsigned char **output)
{
	Atom real_type;
	int format;
	unsigned long count, extra;

	if (XGetWindowProperty(display, window, atom, 0L, length, False, type, &real_type, &format, &count, &extra, output) != Success) {
		return -1;
	}

	if (!*output) {
		return -1;
	}

	if (count == 0) {
		XFree(*output);
	}

	return count;
}

/*
void
x_ewmh_set_client_list(state_t *state)
{
	client_t *client;
	group_t *group;
	int count = 0;
	Window *windows = NULL;

	TAILQ_FOREACH(group, &state->groups, entry) {
		TAILQ_FOREACH(client, &group->clients, entry) {
			if (count == 0) {
				windows = calloc(1, sizeof(Window));
			} else {
				windows = realloc(windows, (count + 1) * sizeof(Window));
			}
			windows[count++] = client->window;
		}
	}

	XChangeProperty(
			state->display,
			state->root,
			state->atoms[_NET_CLIENT_LIST],
			XA_WINDOW,
			32,
			PropModeReplace,
			(unsigned char *)windows,
			count);
}

void
x_ewmh_set_client_list_stacking(state_t *state)
{
	client_t *client;
	group_t *group;
	int count = 0;
	Window *windows = NULL;

	TAILQ_FOREACH_REVERSE(group, &state->groups, group_q, entry) {
		TAILQ_FOREACH_REVERSE(client, &group->clients, client_q, entry) {
			if (count == 0) {
				windows = calloc(1, sizeof(Window));
			} else {
				windows = realloc(windows, (count + 1) * sizeof(Window));
			}
			windows[count++] = client->window;
		}
	}

	XChangeProperty(
			state->display,
			state->root,
			state->atoms[_NET_CLIENT_LIST_STACKING],
			XA_WINDOW,
			32,
			PropModeReplace,
			(unsigned char *)windows,
			count);
}
*/
