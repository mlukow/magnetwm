#include <stdlib.h>
#include <unistd.h>

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

Bool
x_parse_display(char *name, char **host, int *displayp, int *screenp)
{
	char *colon, *dot, *end;
	int len, display, screen;

	if (!name || !*name) {
		name = getenv("DISPLAY");
	}

	if (!name) {
		return False;
	}

	colon = strrchr(name, ':');
	if (!colon) {
		return False;
	}

	len = colon - name;
	++colon;
	display = strtoul(colon, &dot, 10);

	if (dot == colon) {
		return False;
	}

	if(*dot == '\0') {
		screen = 0;
	} else {
		if (*dot != '.') {
			return False;
		}

		++dot;
		screen = strtoul(dot, &end, 10);
		if ((end == dot) || (*end != '\0')) {
			return False;
		}
	}

	*host = malloc(len + 1);
	if(!*host) {
		return False;
	}

	memcpy(*host, name, len);
	(*host)[len] = '\0';
	*displayp = display;
	if(screenp) {
		*screenp = screen;
	}

	return True;
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
