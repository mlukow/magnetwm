#ifndef __MENU_H__
#define __MENU_H__

#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

#include "queue.h"
#include "xutils.h"

struct screen_t;
struct state_t;

TAILQ_HEAD(menu_item_q, menu_item_t);

typedef struct menu_item_t {
	TAILQ_ENTRY(menu_item_t) item;
	TAILQ_ENTRY(menu_item_t) result;
	void *context;

	char *text;
} menu_item_t;

typedef struct menu_t {
	struct screen_t *screen;
	struct state_t *state;

	Window window;
	XftDraw *draw;

	geometry_t geometry;
	int offset;

	Bool cycle;
	int padding;
	char *prompt;
	char *filter;
	int filter_length;
	int border_width;

	struct menu_item_q items;
	struct menu_item_q results;

	int count;
	int limit;
	int selected_item;
	int selected_previous;
	int selected_visible;
	menu_item_t *visible;
} menu_t;

void menu_add(menu_t *, void *, Bool, char *);
void *menu_filter(menu_t *);
void menu_free(menu_t *);
menu_t *menu_init(struct state_t *, struct screen_t *, char *, Bool);

#endif /* __MENU_H__ */
