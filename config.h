#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "queue.h"

struct state_t;

TAILQ_HEAD(command_q, command_t);
TAILQ_HEAD(binding_q, binding_t);

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
	COLOR_NITEMS
} color_t;

typedef enum {
	FONT_MENU_INPUT,
	FONT_MENU_ITEM,
	FONT_NITEMS
} font_t;

typedef struct binding_t {
	TAILQ_ENTRY(binding_t) entry;

	unsigned int modifier;
	int button;
	long flag;
	binding_context_t context;
	void (*function)(struct state_t *, void *, long);
} binding_t;

typedef struct command_t {
	TAILQ_ENTRY(command_t) entry;

	char *name;
	char *path;
} command_t;

typedef struct config_t {
	struct command_q commands;
	struct binding_q keybindings;
	struct binding_q mousebindings;

	char *colors[COLOR_NITEMS];
	char *fonts[FONT_NITEMS];

	int border_width;
} config_t;

void config_free(config_t *);
config_t *config_init(char *);

#endif /* __CONFIG_H__ */
