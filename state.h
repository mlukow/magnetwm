#ifndef __STATE_H__
#define __STATE_H__

#include <X11/cursorfont.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include "queue.h"

struct config_t;
struct ewmh_t;
struct icccm_t;
struct screen_t;

TAILQ_HEAD(screen_q, screen_t);

typedef enum cursor_t {
	CURSOR_NORMAL,
	CURSOR_MOVE,
	CURSOR_RESIZE_BOTTOM_LEFT,
	CURSOR_RESIZE_BOTTOM_RIGHT,
	CURSOR_RESIZE_TOP_LEFT,
	CURSOR_RESIZE_TOP_RIGHT,
	CURSOR_NITEMS
} cursor_t;

typedef struct state_t {
	Colormap colormap;
	Display *display;
	Visual *visual;
	Window root;
	int fd;
	int primary_screen;
	int xrandr_event_base;

	XftColor *colors;
	XftFont **fonts;
	Cursor cursors[CURSOR_NITEMS];

	struct config_t *config;
	struct ewmh_t *ewmh;
	struct icccm_t *icccm;
	struct screen_q screens;
} state_t;

void state_flush(state_t *);
void state_free(state_t *);
state_t *state_init(char *);

#endif /* __STATE_H__ */
