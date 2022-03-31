#include <stdlib.h>
#include <unistd.h>

#include "client.h"
#include "group.h"
#include "queue.h"
#include "state.h"
#include "xutils.h"

void
x_animate(Display *display, Window window, geometry_t from, geometry_t to, double duration)
{
	double time;
	double dheight, dwidth, dx, dy, height, i, steps, width, x, y;

	steps = 60.0 * duration;
	time = 1000000.0 * duration / steps;

	x = from.x;
	y = from.y;
	width = from.width;
	height = from.height;
	dx = ((double)to.x - (double)from.x) / steps;
	dy = ((double)to.y - (double)from.y) / steps;
	dwidth = ((double)to.width - (double)from.width) / steps;
	dheight = ((double)to.height - (double)from.height) / steps;

	for (i = 0; i < steps; i++) {
		x += dx;
		y += dy;
		width += dwidth;
		height += dheight;
		XMoveResizeWindow(display, window, x, y, width, height);
		XFlush(display);
		usleep(time);
	}

	XMoveResizeWindow(display, window, to.x, to.y, to.width, to.height);
}

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

Bool
x_get_text_property(Display *display, Window window, Atom atom, char **output)
{
	Bool result = False;
	char **list;
	int nitems;
	XTextProperty base, item;

	*output = NULL;

	if ((XGetTextProperty(display, window, &base, atom) == 0) || (base.nitems == 0)) {
		return False;
	}

	if (Xutf8TextPropertyToTextList(display, &base, &list, &nitems) == Success) {
		if (nitems > 1) {
			if (Xutf8TextListToTextProperty(display, list, nitems, XUTF8StringStyle, &item) == Success) {
				*output = strdup((char *)item.value);
				XFree(item.value);
				result = True;
			}
		} else if (nitems == 1) {
			*output = strdup(*list);
			result = True;
		}

		XFreeStringList(list);
	}

	XFree(base.value);

	return result;
}

void
x_send_message(Display *display, Window window, Atom type, Atom data, Time time)
{
	XClientMessageEvent event;

	(void)memset(&event, 0, sizeof(XClientMessageEvent));
	event.type = ClientMessage;
	event.window = window;
	event.message_type = type;
	event.format = 32;
	event.data.l[0] = data;
	event.data.l[1] = time;

	XSendEvent(display, window, False, NoEventMask, (XEvent *)&event);
}

void
x_set_class_hint(Display *display, Window window, char *name)
{
	XClassHint *hint;

	hint = XAllocClassHint();
	hint->res_name = strdup(name);
	hint->res_class = strdup(name);XSetClassHint(display, window, hint);
	XFree(hint);
}
