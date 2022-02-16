#include <ctype.h>

#include <X11/keysymdef.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>

#include "config.h"
#include "menu.h"
#include "screen.h"
#include "state.h"
#include "utils.h"
#include "xutils.h"

#define MENUMASK (				\
		ButtonPressMask		|	\
		ButtonReleaseMask	|	\
		PointerMotionMask	|	\
		ButtonMotionMask	|	\
		KeyPressMask		|	\
		ExposureMask)
#define MENUGRABMASK (			\
		ButtonPressMask		|	\
		ButtonReleaseMask	|	\
		PointerMotionMask	|	\
		ButtonMotionMask	|	\
		StructureNotifyMask)

int menu_calculate_entry(menu_context_t *, int, int);
void menu_draw(state_t *, menu_context_t *, menu_t *, menu_t *);
void menu_draw_entry(menu_context_t *, menu_t *, int, int);
int menu_filter_add(menu_context_t *, menu_t *, menu_t *, char *);
int menu_filter_complete(menu_context_t *, menu_t *);
int menu_filter_delete(menu_context_t *, menu_t *, menu_t *);
void menu_filter_update(menu_context_t *, menu_t *);
int menu_handle_key(state_t *, menu_context_t *, menu_t *, menu_t *, XKeyEvent *);
void menu_handle_move(menu_context_t *, menu_t *, int, int);
int menu_handle_release(menu_context_t *, int, int);
int menu_move_down(menu_context_t *);
int menu_move_up(menu_context_t *);

void
menu_add(menu_t *items, void *context, int sorted, char *(*print)(void *))
{
	char *item_text, *current_text;
	int cmp;
	menu_item_t *current, *item;

	item = malloc(sizeof(menu_item_t));
	item->context = context;

	if (sorted) {
		item_text = (*print)(context);

		TAILQ_FOREACH(current, items, entry) {
			current_text = (*print)(current->context);
			cmp = strcmp(item_text, current_text);
			if (cmp == 0) {
				return;
			}

			if (cmp < 0) {
				TAILQ_INSERT_BEFORE(current, item, entry);
				return;
			}
		}
	}

	TAILQ_INSERT_TAIL(items, item, entry);
}

int
menu_calculate_entry(menu_context_t *context, int x, int y)
{
	if ((x < context->geometry.x) || (x > context->geometry.x + context->geometry.width)) {
		return -1;
	}

	if ((y < context->geometry.y + context->offset + context->padding) ||
		(y >= context->geometry.y + context->geometry.height - context->padding)) {
		return -1;
	}

	y -= context->geometry.y + context->offset + context->padding + context->border_width;

	return y / (context->state->fonts[FONT_MENU_ITEM]->height + 1);
}

void
menu_draw(state_t *state, menu_context_t *context, menu_t *items, menu_t *results)
{
	char *text;
	int i = 0, y;
	menu_item_t *item;
	XftFont *font;

	context->geometry.height = context->offset + MIN(context->limit, context->count) * (context->state->fonts[FONT_MENU_ITEM]->height + 1);
	if (context->count > 0) {
		context->geometry.height += 2 * context->padding;
	}

	XClearWindow(state->display, context->window);
	XMoveResizeWindow(
			state->display,
			context->window,
			context->geometry.x,
			context->geometry.y,
			context->geometry.width,
			context->geometry.height);

	if (context->prompt) {
		if (context->filter_length == 0) {
			XftDrawStringUtf8(
					context->draw,
					&context->state->colors[COLOR_MENU_PROMPT],
					context->state->fonts[FONT_MENU_INPUT],
					context->padding,
					context->padding + context->state->fonts[FONT_MENU_INPUT]->ascent,
					(const FcChar8 *)context->prompt,
					strlen(context->prompt));
		} else {
			XftDrawStringUtf8(
					context->draw,
					&context->state->colors[COLOR_MENU_FOREGROUND],
					context->state->fonts[FONT_MENU_INPUT],
					context->padding,
					context->padding + context->state->fonts[FONT_MENU_INPUT]->ascent,
					(const FcChar8 *)context->filter,
					context->filter_length);
		}

		XftDrawRect(
				context->draw,
				&context->state->colors[COLOR_MENU_SEPARATOR],
				0,
				context->offset - 1,
				context->geometry.width,
				1);
	}

	item = context->visible;
	font = context->state->fonts[FONT_MENU_ITEM];
	while ((i < context->limit) && item) {
		text = (*context->print)(item->context);
		y = context->offset + context->padding + i * (font->height + 1) + font->ascent + 1;
		XftDrawStringUtf8(
				context->draw,
				&context->state->colors[COLOR_MENU_FOREGROUND],
				font,
				context->padding,
				y,
				(const FcChar8 *)text,
				strlen(text));
		item = TAILQ_NEXT(item, result);
		i++;
	}

	if (i > 0) {
		menu_draw_entry(context, results, context->selected_visible, 1);
	}
}

void
menu_draw_entry(menu_context_t *context, menu_t *results, int entry, int selected)
{
	char *text;
	int color, i;
	menu_item_t *item = context->visible;
	XftFont *font;

	for (i = 0; i < entry; i++)
		item = TAILQ_NEXT(item, result);

	if (!item)
		return;

	text = (*context->print)(item->context);
	font = context->state->fonts[FONT_MENU_ITEM];

	color = selected ? COLOR_MENU_SELECTION_BACKGROUND : COLOR_MENU_BACKGROUND;
	XftDrawRect(
			context->draw,
			&context->state->colors[color],
			0,
			context->offset + context->padding + entry * (font->height + 1),
			context->geometry.width, font->height + 1);

	color = selected ? COLOR_MENU_SELECTION_FOREGROUND : COLOR_MENU_FOREGROUND;
	XftDrawStringUtf8(
			context->draw,
			&context->state->colors[color],
			font,
			context->padding,
			context->offset + context->padding + entry * (font->height + 1) + font->ascent + 1,
			(const FcChar8 *)text,
			strlen(text));
}

void *
menu_filter(state_t *state, screen_t *screen, menu_t *items, char *prompt, char *(*print)(void *item))
{
	int focusrevert, i = 0, processing = 1;
	menu_context_t context;
	menu_t results;
	menu_item_t *item;
	Window focus;
	XClassHint *hint;
	XEvent event;

	memset(&context, 0, sizeof(menu_context_t));

	context.screen = screen;
	context.state = state;
	context.window = XCreateSimpleWindow(
			state->display,
			state->root,
			0,
			0,
			1,
			1,
			state->config->border_width,
			state->colors[COLOR_BORDER_ACTIVE].pixel,
			state->colors[COLOR_MENU_BACKGROUND].pixel);
	context.draw = XftDrawCreate(
			state->display,
			context.window,
			state->visual,
			state->colormap);
	context.prompt = prompt;
	context.print = print;
	context.selected_previous = -1;
	context.limit = 10; // state->config->menu_limit;
	context.padding = 15; // state->config->menu_padding;
	context.border_width = state->config->border_width;

	hint = XAllocClassHint();
	hint->res_name = strdup("torwm");
	hint->res_class = strdup("torwm");
	XSetClassHint(state->display, context.window, hint);
	XFree(hint);

	TAILQ_INIT(&results);

	context.geometry.width = MIN(0.3 * context.screen->geometry.width, 650);
	if (prompt) {
		context.offset = 2 * context.padding + context.state->fonts[FONT_MENU_INPUT]->height + 1;
	} else {
		context.offset = 0;
	}

	TAILQ_FOREACH(item, items, entry) {
		TAILQ_INSERT_TAIL(&results, item, result);
		context.count++;
	}

	if (context.count < 1) {
		return NULL;
	}

	context.geometry.x = (context.screen->geometry.width - context.geometry.width) / 2;
	context.geometry.y = 0.2 * context.screen->geometry.height;

	context.visible = TAILQ_FIRST(&results);

	XSelectInput(state->display, context.window, MENUMASK);
	XMapRaised(state->display, context.window);

	if (XGrabPointer(
				state->display,
				context.window,
				False,
				MENUGRABMASK,
				GrabModeAsync,
				GrabModeAsync,
				None,
				None,
				CurrentTime) != GrabSuccess) {
		XftDrawDestroy(context.draw);
		XDestroyWindow(state->display, context.window);
		return NULL;
	}

	XGetInputFocus(state->display, &focus, &focusrevert);
	XSetInputFocus(state->display, context.window, RevertToPointerRoot, CurrentTime);

	XGrabKeyboard(state->display, context.window, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	while (processing) {
		XWindowEvent(state->display, context.window, MENUMASK, &event);
		switch (event.type) {
			case KeyPress:
				i = menu_handle_key(state, &context, items, &results, &event.xkey);
				if (i == 1) {
					menu_draw(state, &context, items, &results);
				} else if ((i == -1) || ((i == 2) && (context.count > 0))) {
					processing = 0;
				}

				break;
			case Expose:
				menu_draw(state, &context, items, &results);
				break;
			case MotionNotify:
				menu_handle_move(&context, &results, event.xbutton.x_root, event.xbutton.y_root);
				break;
			case ButtonPress:
				if (event.xbutton.button == Button4) {
					menu_move_up(&context);
					menu_draw(state, &context, items, &results);
				} else if (event.xbutton.button == Button5) {
					menu_move_down(&context);
					menu_draw(state, &context, items, &results);
				}

				break;
			case ButtonRelease:
				if (event.xbutton.button == Button1) {
					i = menu_handle_release(&context, event.xbutton.x_root, event.xbutton.y_root);
					processing = 0;
				}

				break;
		}
	}

	XftDrawDestroy(context.draw);
	XDestroyWindow(state->display, context.window);

	XSetInputFocus(state->display, focus, focusrevert, CurrentTime);

	XUngrabKeyboard(state->display, CurrentTime);
	XUngrabPointer(state->display, CurrentTime);

	if (i == -1) {
		return NULL;
	}

	item = context.visible;
	for (i = 0; i < context.selected_visible; i++) {
		item = TAILQ_NEXT(item, result);
	}

	return (item == NULL) ? NULL : item->context;
}

int
menu_filter_add(menu_context_t *context, menu_t *items, menu_t *results, char *suffix)
{
	char *text;
	int len;
	menu_item_t *item, *next;

	len = strlen(suffix);
	if (context->filter_length == 0) {
		context->filter = malloc(len + 1);
	} else {
		context->filter = realloc(context->filter, context->filter_length + len + 1);
	}

	memcpy(context->filter + context->filter_length, suffix, len);
	context->filter_length += len;
	context->filter[context->filter_length] = '\0';

	item = TAILQ_FIRST(results);
	context->count = 0;
	while (item) {
		next = TAILQ_NEXT(item, result);
		text = (*context->print)(item->context);

		if (strcasestr(text, context->filter)) {
			context->count++;
		} else {
			TAILQ_REMOVE(results, item, result);
		}

		item = next;
	}

	menu_filter_update(context, results);

	return 1;
}

int
menu_filter_complete(menu_context_t *context, menu_t *results)
{
	char *text;
	int i;
	menu_item_t *item;

	item = TAILQ_FIRST(results);
	if (!item) {
		return 0;
	}

	text = (*context->print)(item->context);

	if (context->filter_length > 0) {
		free(context->filter);
	}

	context->filter = strdup(text);
	context->filter_length = strlen(text);
	while ((item = TAILQ_NEXT(item, result)) != NULL) {
		i = 0;
		text = (*context->print)(item->context);
		while (tolower(context->filter[i]) == tolower(text[i])) {
			i++;
		}

		context->filter[i] = '\0';
		context->filter_length = i;
	}

	return 1;
}

int
menu_filter_delete(menu_context_t *context, menu_t *items, menu_t *results)
{
	char *text;
	menu_item_t *item;

	while ((item = TAILQ_FIRST(results)) != NULL) {
		TAILQ_REMOVE(results, item, result);
	}

	context->count = 0;
	TAILQ_FOREACH(item, items, entry) {
		text = (*context->print)(item->context);

		if (!context->filter || strcasestr(text, context->filter)) {
			TAILQ_INSERT_TAIL(results, item, result);
			context->count++;
		}
	}

	menu_filter_update(context, results);

	return 1;
}

void
menu_filter_update(menu_context_t *context, menu_t *items)
{
	context->visible = TAILQ_FIRST(items);
	if (context->visible) {
		context->selected_item = 0;
		context->selected_previous = 0;
		context->selected_visible = 0;
	} else {
		context->selected_item = -1;
		context->selected_previous = -1;
		context->selected_visible = -1;
	}
}

int
menu_handle_key(state_t *state, menu_context_t *context, menu_t *items,
		menu_t *results, XKeyEvent *event)
{
	char c[32];
	int wide_length;
	KeySym keysym;
	wchar_t wc;

	memset(c, 0, 32);
	keysym = XkbKeycodeToKeysym(state->display, event->keycode, 0, event->state & ShiftMask);

	switch (keysym) {
		case XK_BackSpace:
			if (!context->prompt) {
				return 0;
			}

			if (context->filter_length == 0) {
				return 0;
			}

			wide_length = 1;
			while (mbtowc(&wc, &context->filter[context->filter_length - wide_length], MB_CUR_MAX) == -1) {
				wide_length++;
			}

			context->filter_length -= wide_length;

			if (context->filter_length == 0) {
				free(context->filter);
				context->filter = NULL;
			} else {
				context->filter = realloc(context->filter, context->filter_length + 1);
				context->filter[context->filter_length] = '\0';
			}

			return menu_filter_delete(context, items, results);
		case XK_KP_Enter:
		case XK_Return:
			return 2;
		case XK_Tab:
			return menu_filter_complete(context, results);
		case XK_Up:
			return menu_move_up(context);
		case XK_p:
			if (event->state & ControlMask)
				return menu_move_up(context);

			break;
		case XK_Down:
			return menu_move_down(context);
		case XK_n:
			if (event->state & ControlMask) {
				return menu_move_down(context);
			}

			break;
		case XK_Escape:
			if (context->filter_length == 0) {
				return -1;
			}

			free(context->filter);
			context->filter = NULL;
			context->filter_length = 0;

			return menu_filter_delete(context, items, results);
	}

	if (!context->prompt) {
		return 0;
	}

	if (XLookupString(event, c, 32, &keysym, NULL) < 0) {
		return 0;
	}

	return menu_filter_add(context, items, results, c);
}

void
menu_handle_move(menu_context_t *context, menu_t *results, int x, int y)
{
	context->selected_previous = context->selected_visible;
	context->selected_visible = menu_calculate_entry(context, x, y);

	if (context->selected_visible == context->selected_previous) {
		return;
	}

	if (context->selected_visible == -1) {
		context->selected_visible = context->selected_previous;
		return;
	}

	if (context->selected_previous != -1) {
		context->selected_item += context->selected_visible - context->selected_previous;
		menu_draw_entry(context, results, context->selected_previous, 0);
	}

	menu_draw_entry(context, results, context->selected_visible, 1);
}

int
menu_handle_release(menu_context_t *context, int x, int y)
{
	if ((x < context->geometry.x) || (x > context->geometry.x + context->geometry.width)) {
		return -1;
	}

	if ((y < context->geometry.y + context->offset) || (y >= context->geometry.y + context->geometry.height - context->padding)) {
		return -1;
	}

	return 2;
}

int
menu_move_down(menu_context_t *context)
{
	menu_item_t *item;

	if (context->selected_item == context->count - 1) {
		return 0;
	}

	if (context->selected_visible == context->limit - 1) {
		item = TAILQ_NEXT(context->visible, result);
		if (!item) {
			return 0;
		}

		context->visible = item;
		context->selected_item++;
	} else {
		context->selected_item++;
		context->selected_visible++;
	}

	return 1;
}

int
menu_move_up(menu_context_t *context)
{
	menu_item_t *item;

	if (context->selected_item == 0) {
		return 0;
	}

	if (context->selected_visible == 0) {
		item = TAILQ_PREV(context->visible, menu_t, result);
		if (!item) {
			return 0;
		}

		context->visible = item;
		context->selected_item--;
	} else {
		context->selected_item--;
		context->selected_visible--;
	}

	return 1;
}
