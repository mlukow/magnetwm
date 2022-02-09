#ifndef __STATE_H__
#define __STATE_H__

#include <X11/Xlib.h>

#include "queue.h"

struct config_t;
struct screen_t;

TAILQ_HEAD(screen_q, screen_t);

typedef struct state_t {
	Display *display;
	Window root;
	int fd;
	int primary_screen;
	int xrandr_event_base;

	Atom *atoms;

	struct config_t *config;

	struct screen_q screens;
} state_t;

void state_free(state_t *);
state_t *state_init(char *, char *);

#endif /* __STATE_H__ */
