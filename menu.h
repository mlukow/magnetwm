#ifndef __MENU_H__
#define __MENU_H__

#include <X11/Xft/Xft.h>

#include "queue.h"

typedef struct menu_t menu_t;

struct screen_t;
struct state_t;

TAILQ_HEAD(menu_t, menu_item_t);

typedef struct menu_item_t {
	TAILQ_ENTRY(menu_item_t) entry;
	TAILQ_ENTRY(menu_item_t) result;
	void *context;
} menu_item_t;

typedef struct menu_context_t {
	struct screen_t *screen;
	struct state_t *state;

	Window window;
	XftDraw *draw;

	int x;
	int y;
	int width;
	int offset;

	int padding;
	char *prompt;
	char *filter;
	int filter_length;
	int border_width;

	int count;
	int limit;
	int selected_item;
	int selected_previous;
	int selected_visible;
	menu_item_t *visible;

	char *(*print)(void *);
} menu_context_t;

void menu_add(menu_t *, void *, int, char *(*)(void *));
void *menu_filter(struct state_t *, struct screen_t *, menu_t *, char *, char *(*)(void *));

#endif /* __MENU_H__ */
