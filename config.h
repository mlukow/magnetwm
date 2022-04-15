#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <X11/Xlib.h>

#include "queue.h"

struct state_t;

TAILQ_HEAD(command_q, command_t);
TAILQ_HEAD(binding_q, binding_t);
TAILQ_HEAD(ignored_q, ignored_t);

typedef enum {
	BINDING_CONTEXT_CLIENT,
	BINDING_CONTEXT_SCREEN,
	BINDING_CONTEXT_GLOBAL
} binding_context_t;

typedef enum {
	COLOR_BORDER_ACTIVE,
	COLOR_BORDER_INACTIVE,
	COLOR_BORDER_URGENT,
	COLOR_MENU_BACKGROUND,
	COLOR_MENU_FOREGROUND,
	COLOR_MENU_PROMPT,
	COLOR_MENU_SELECTION_BACKGROUND,
	COLOR_MENU_SELECTION_FOREGROUND,
	COLOR_MENU_SEPARATOR,
	COLOR_NITEMS
} color_t;

typedef enum {
	LABEL_APPLICATIONS,
	LABEL_RUN,
	LABEL_WINDOWS,

	LABEL_WINDOW_ACTIVE,
	LABEL_WINDOW_INACTIVE,
	LABEL_WINDOW_HIDDEN,

	LABEL_NITEMS
} label_t;

typedef struct command_t {
	TAILQ_ENTRY(command_t) entry;

	char *name;
	char *path;
} command_t;

typedef enum {
	FONT_MENU_INPUT,
	FONT_MENU_ITEM,
	FONT_MENU_ITEM_DETAIL,
	FONT_NITEMS
} font_t;

typedef enum {
	WINDOW_PLACEMENT_CASCADE,
	WINDOW_PLACEMENT_POINTER
} window_placement_t;

typedef struct binding_t {
	TAILQ_ENTRY(binding_t) entry;

	char *name;
	unsigned int modifier;
	long button;
	long flag;
	binding_context_t context;
	void (*function)(struct state_t *, void *, long);
} binding_t;

typedef struct ignored_t {
	TAILQ_ENTRY(ignored_t) entry;

	char *class_name;
} ignored_t;

typedef struct config_t {
	char *wm_name;

	struct command_q commands;
	struct binding_q keybindings;
	struct binding_q mousebindings;
	struct ignored_q ignored;

	char *colors[COLOR_NITEMS];
	char *fonts[FONT_NITEMS];
	char *labels[LABEL_NITEMS];

	double transition_duration;
	int border_width;
	window_placement_t window_placement;
} config_t;

void config_free(config_t *);
config_t *config_init();

void config_bind_command(config_t *, char *, char *);
Bool config_bind_key(config_t *, char *, char *);
Bool config_bind_mouse(config_t *, char *, char *);
void config_ignore(config_t *, char *);

#endif /* __CONFIG_H__ */
