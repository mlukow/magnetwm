#define _GNU_SOURCE

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
		KeyReleaseMask		|	\
		ExposureMask)
#define MENUGRABMASK (			\
		ButtonPressMask		|	\
		ButtonReleaseMask	|	\
		PointerMotionMask	|	\
		ButtonMotionMask	|	\
		StructureNotifyMask)

int menu_calculate_entry(menu_t *, int, int);
void menu_draw(menu_t *);
void menu_draw_selection(menu_t *, int);
int menu_filter_add(menu_t *, char *);
int menu_filter_complete(menu_t *);
int menu_filter_delete(menu_t *);
void menu_filter_update(menu_t *);
int menu_handle_key(menu_t *, XKeyEvent *);
Bool menu_handle_move(menu_t *, int, int);
int menu_handle_release(menu_t *, int, int);
int menu_move_down(menu_t *);
int menu_move_left(menu_t *);
int menu_move_right(menu_t *);
int menu_move_up(menu_t *);

void
menu_add(menu_t *menu, void *context, Bool sorted, char *text, char *detail)
{
	int cmp;
	menu_item_t *current, *item;
	XGlyphInfo extents;

	XftTextExtentsUtf8(
			menu->state->display,
			menu->state->fonts[FONT_MENU_ITEM],
			(const FcChar8 *)text,
			strlen(text),
			&extents);

	item = malloc(sizeof(menu_item_t));
	item->context = context;
	item->text = strdup(text);
	item->text_width = extents.width;

	if (detail) {
		XftTextExtentsUtf8(
				menu->state->display,
				menu->state->fonts[FONT_MENU_ITEM_DETAIL],
				(const FcChar8 *)detail,
				strlen(detail),
				&extents);
		item->detail = strdup(detail);
		item->detail_width = extents.width;
	} else {
		item->detail = NULL;
		item->detail_width = 0;
	}

	if (sorted) {
		TAILQ_FOREACH(current, &menu->items, item) {
			cmp = strcmp(item->text, current->text);
			if (cmp == 0) {
				return;
			}

			if (cmp < 0) {
				TAILQ_INSERT_BEFORE(current, item, item);
				return;
			}
		}
	}

	TAILQ_INSERT_TAIL(&menu->items, item, item);
}

int
menu_calculate_entry(menu_t *menu, int x, int y)
{
	if ((x < menu->geometry.x) || (x > menu->geometry.x + menu->geometry.width)) {
		return -1;
	}

	if ((y < menu->geometry.y) || (y > menu->geometry.y + menu->geometry.height)) {
		return -1;
	}

	if ((y < menu->geometry.y + menu->offset + menu->padding) || (y >= menu->geometry.y + menu->geometry.height - menu->padding)) {
		return -1;
	}

	y -= menu->geometry.y + menu->offset + menu->padding + menu->border_width;

	return y / (menu->state->fonts[FONT_MENU_ITEM]->height + 1);
}

void
menu_draw(menu_t *menu)
{
	int i = 0, y;
	menu_item_t *item;
	XftFont *font;

	menu->geometry.height = menu->offset + MIN(menu->limit, menu->count) * (menu->state->fonts[FONT_MENU_ITEM]->height + 1);
	if (menu->count > 0) {
		menu->geometry.height += 2 * menu->padding;
	}

	XClearWindow(menu->state->display, menu->window);
	XMoveResizeWindow(
			menu->state->display,
			menu->window,
			menu->geometry.x,
			menu->geometry.y,
			menu->geometry.width,
			menu->geometry.height);

	if (menu->prompt) {
		if (menu->filter_length == 0) {
			XftDrawStringUtf8(
					menu->draw,
					&menu->state->colors[COLOR_MENU_PROMPT],
					menu->state->fonts[FONT_MENU_INPUT],
					menu->padding,
					menu->padding + menu->state->fonts[FONT_MENU_INPUT]->ascent,
					(const FcChar8 *)menu->prompt,
					strlen(menu->prompt));
		} else {
			XftDrawStringUtf8(
					menu->draw,
					&menu->state->colors[COLOR_MENU_FOREGROUND],
					menu->state->fonts[FONT_MENU_INPUT],
					menu->padding,
					menu->padding + menu->state->fonts[FONT_MENU_INPUT]->ascent,
					(const FcChar8 *)menu->filter,
					menu->filter_length);
		}

		if (menu->count > 0) {
			XftDrawRect(
					menu->draw,
					&menu->state->colors[COLOR_MENU_SEPARATOR],
					0,
					menu->offset - 1,
					menu->geometry.width,
					1);
		}
	}

	item = menu->visible;
	font = menu->state->fonts[FONT_MENU_ITEM];
	while ((i < menu->limit) && item) {
		y = menu->offset + menu->padding + i * (font->height + 1) + font->ascent;
		XftDrawStringUtf8(
				menu->draw,
				&menu->state->colors[COLOR_MENU_FOREGROUND],
				font,
				menu->padding,
				y,
				(const FcChar8 *)item->text,
				strlen(item->text));

		if (item->detail) {
			XftDrawStringUtf8(
					menu->draw,
					&menu->state->colors[COLOR_MENU_FOREGROUND],
					menu->state->fonts[FONT_MENU_ITEM_DETAIL],
					menu->geometry.width - menu->padding - item->detail_width,
					y,
					(const FcChar8 *)item->detail,
					strlen(item->detail));
		}

		item = TAILQ_NEXT(item, result);
		i++;
	}

	if (i > 0) {
		menu_draw_selection(menu, menu->selected_visible);
	}
}

void
menu_draw_selection(menu_t *menu, int entry)
{
	int i;
	menu_item_t *item = menu->visible;
	XftFont *font;

	for (i = 0; i < entry; i++) {
		item = TAILQ_NEXT(item, result);
	}

	if (!item) {
		return;
	}

	font = menu->state->fonts[FONT_MENU_ITEM];

	XftDrawRect(
			menu->draw,
			&menu->state->colors[COLOR_MENU_SELECTION_BACKGROUND],
			0,
			menu->offset + menu->padding + entry * (font->height + 1) + 1,
			menu->geometry.width,
			font->height + 1);

	XftDrawStringUtf8(
			menu->draw,
			&menu->state->colors[COLOR_MENU_SELECTION_FOREGROUND],
			font,
			menu->padding,
			menu->offset + menu->padding + entry * (font->height + 1) + font->ascent,
			(const FcChar8 *)item->text,
			strlen(item->text));

	if (item->detail) {
		XftDrawStringUtf8(
				menu->draw,
				&menu->state->colors[COLOR_MENU_FOREGROUND],
				menu->state->fonts[FONT_MENU_ITEM_DETAIL],
				menu->geometry.width - menu->padding - item->detail_width,
				menu->offset + menu->padding + entry * (font->height + 1) + font->ascent,
				(const FcChar8 *)item->detail,
				strlen(item->detail));
	}

	if (menu->callback) {
		menu->callback(menu->state, item->context);
	}
}

void *
menu_filter(menu_t *menu)
{
	Bool processing = True;
	int focusrevert, i, width_detail = 0, width_text = 0;
	menu_item_t *item;
	Window focus;
	XEvent event;
	XGlyphInfo extents;

	TAILQ_FOREACH(item, &menu->items, item) {
		TAILQ_INSERT_TAIL(&menu->results, item, result);
		menu->count++;

		if (item->text_width > width_text) {
			width_text = item->text_width;
		}

		if (item->detail_width > width_detail) {
			width_detail = item->detail_width;
		}
	}

	menu->geometry.width = width_text;
	if (width_detail > 0) {
		menu->geometry.width += menu->padding + width_detail;
	}

	if (menu->prompt) {
		XftTextExtentsUtf8(
				menu->state->display,
				menu->state->fonts[FONT_MENU_INPUT],
				(const FcChar8 *)menu->prompt,
				strlen(menu->prompt),
				&extents);
		if (extents.width > menu->geometry.width) {
			menu->geometry.width = extents.width;
		}
	}

	if (menu->cycle) {
		if (menu->count < 2) {
			return NULL;
		}
	} else if (menu->count == 0) {
		return NULL;
	}

	menu->geometry.width += 2 * menu->padding;
	if (menu->prompt) {
		menu->offset = 2 * menu->padding + menu->state->fonts[FONT_MENU_INPUT]->height + 1;
	}

	menu->geometry.x = menu->screen->geometry.x + (menu->screen->geometry.width - menu->geometry.width) / 2;
	menu->geometry.y = menu->screen->geometry.y + 0.2 * menu->screen->geometry.height;

	menu->visible = TAILQ_FIRST(&menu->results);
	if (menu->cycle) {
		menu->selected_item = 1;
		menu->selected_visible = 1;
	}

	XSelectInput(menu->state->display, menu->window, MENUMASK);
	XMapRaised(menu->state->display, menu->window);

	if (XGrabPointer(
				menu->state->display,
				menu->window,
				False,
				MENUGRABMASK,
				GrabModeAsync,
				GrabModeAsync,
				None,
				None,
				CurrentTime) != GrabSuccess) {
		XftDrawDestroy(menu->draw);
		XDestroyWindow(menu->state->display, menu->window);
		return NULL;
	}

	XGetInputFocus(menu->state->display, &focus, &focusrevert);
	XSetInputFocus(menu->state->display, menu->window, RevertToPointerRoot, CurrentTime);

	XGrabKeyboard(menu->state->display, menu->window, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	menu_draw(menu);

	while (processing) {
		XWindowEvent(menu->state->display, menu->window, MENUMASK, &event);
		switch (event.type) {
			case KeyPress:
				i = menu_handle_key(menu, &event.xkey);
				if (i == 1) {
					menu_draw(menu);
				} else if ((i == -1) || ((i == 2) && (menu->count > 0))) {
					processing = False;
				}

				break;
			case KeyRelease:
				if (menu->cycle) {
					if ((event.xkey.keycode == 0x40) && (event.xkey.state & Mod1Mask)) {
						processing = False;
					}
				}

				break;
			case MotionNotify:
				if (menu_handle_move(menu, event.xbutton.x_root, event.xbutton.y_root)) {
					menu_draw(menu);
				}
				break;
			case ButtonPress:
				if (event.xbutton.button == Button4) {
					menu_move_up(menu);
					menu_draw(menu);
				} else if (event.xbutton.button == Button5) {
					menu_move_down(menu);
					menu_draw(menu);
				}

				break;
			case ButtonRelease:
				if (event.xbutton.button == Button1) {
					i = menu_handle_release(menu, event.xbutton.x_root, event.xbutton.y_root);
					processing = False;
				}

				break;
		}
	}

	XftDrawDestroy(menu->draw);
	XDestroyWindow(menu->state->display, menu->window);

	XSetInputFocus(menu->state->display, focus, focusrevert, CurrentTime);

	XUngrabKeyboard(menu->state->display, CurrentTime);
	XUngrabPointer(menu->state->display, CurrentTime);

	if (menu->filter) {
		free(menu->filter);
	}

	if (i == -1) {
		return NULL;
	}

	item = menu->visible;
	for (i = 0; i < menu->selected_visible; i++) {
		item = TAILQ_NEXT(item, result);
	}

	return (item == NULL) ? NULL : item->context;
}

int
menu_filter_add(menu_t *menu, char *suffix)
{
	int len;
	menu_item_t *item, *next;

	len = strlen(suffix);
	if (menu->filter_length == 0) {
		menu->filter = malloc(len + 1);
	} else {
		menu->filter = realloc(menu->filter, menu->filter_length + len + 1);
	}

	memcpy(menu->filter + menu->filter_length, suffix, len);
	menu->filter_length += len;
	menu->filter[menu->filter_length] = '\0';

	item = TAILQ_FIRST(&menu->results);
	menu->count = 0;
	while (item) {
		next = TAILQ_NEXT(item, result);

		if (strcasestr(item->text, menu->filter)) {
			menu->count++;
		} else {
			TAILQ_REMOVE(&menu->results, item, result);
		}

		item = next;
	}

	menu_filter_update(menu);

	return 1;
}

int
menu_filter_complete(menu_t *menu)
{
	int i;
	menu_item_t *item;

	item = TAILQ_FIRST(&menu->results);
	if (!item) {
		return 0;
	}

	if (menu->filter_length > 0) {
		free(menu->filter);
	}

	menu->filter = strdup(item->text);
	menu->filter_length = strlen(item->text);
	while ((item = TAILQ_NEXT(item, result)) != NULL) {
		i = 0;
		while (tolower(menu->filter[i]) == tolower(item->text[i])) {
			i++;
		}

		menu->filter[i] = '\0';
		menu->filter_length = i;
	}

	return 1;
}

int
menu_filter_delete(menu_t *menu)
{
	menu_item_t *item;

	while ((item = TAILQ_FIRST(&menu->results)) != NULL) {
		TAILQ_REMOVE(&menu->results, item, result);
	}

	menu->count = 0;
	TAILQ_FOREACH(item, &menu->items, item) {
		if (!menu->filter || strcasestr(item->text, menu->filter)) {
			TAILQ_INSERT_TAIL(&menu->results, item, result);
			menu->count++;
		}
	}

	menu_filter_update(menu);

	return 1;
}

void
menu_filter_update(menu_t *menu)
{
	menu->visible = TAILQ_FIRST(&menu->results);
	if (menu->visible) {
		menu->selected_item = 0;
		menu->selected_previous = 0;
		menu->selected_visible = 0;
	} else {
		menu->selected_item = -1;
		menu->selected_previous = -1;
		menu->selected_visible = -1;
	}
}

void
menu_free(menu_t *menu)
{
	menu_item_t *item;

	while ((item = TAILQ_FIRST(&menu->results)) != NULL) {
		TAILQ_REMOVE(&menu->results, item, result);
	}

	while ((item = TAILQ_FIRST(&menu->items)) != NULL) {
		free(item->text);
		TAILQ_REMOVE(&menu->items, item, item);
	}

	if (menu->prompt) {
		free(menu->prompt);
	}

	free(menu);
}

int
menu_handle_key(menu_t *menu, XKeyEvent *event)
{
	char c[32];
	int wide_length;
	KeySym keysym;
	wchar_t wc;

	memset(c, 0, 32);
	keysym = XkbKeycodeToKeysym(menu->state->display, event->keycode, 0, event->state & ShiftMask);

	switch (keysym) {
		case XK_BackSpace:
			if (!menu->prompt) {
				return 0;
			}

			if (menu->filter_length == 0) {
				return 0;
			}

			wide_length = 1;
			while (mbtowc(&wc, &menu->filter[menu->filter_length - wide_length], MB_CUR_MAX) == -1) {
				wide_length++;
			}

			menu->filter_length -= wide_length;

			if (menu->filter_length == 0) {
				free(menu->filter);
				menu->filter = NULL;
			} else {
				menu->filter = realloc(menu->filter, menu->filter_length + 1);
				menu->filter[menu->filter_length] = '\0';
			}

			return menu_filter_delete(menu);
		case XK_KP_Enter:
		case XK_Return:
			return 2;
		case XK_Tab:
			return menu->cycle ? menu_move_right(menu) : menu_filter_complete(menu);
		case XK_ISO_Left_Tab:
			return menu->cycle ? menu_move_left(menu) : 0;
		case XK_b:
			if (menu->cycle && (event->state & ControlMask)) {
				return menu_move_left(menu);
			}

			break;
		case XK_Left:
			return menu->cycle ? menu_move_left(menu) : 0;
		case XK_Up:
			return menu->cycle ? 0 : menu_move_up(menu);
		case XK_p:
			if (!menu->cycle && (event->state & ControlMask)) {
				return menu_move_up(menu);
			}

			break;
		case XK_f:
			if (menu->cycle && (event->state & ControlMask)) {
				return menu_move_right(menu);
			}

			break;
		case XK_Right:
			return menu->cycle ? menu_move_right(menu) : 0;
		case XK_Down:
			return menu->cycle ? 0 : menu_move_down(menu);
		case XK_n:
			if (!menu->cycle && (event->state & ControlMask)) {
				return menu_move_down(menu);
			}

			break;
		case XK_Escape:
			if (menu->filter_length == 0) {
				return -1;
			}

			free(menu->filter);
			menu->filter = NULL;
			menu->filter_length = 0;

			return menu_filter_delete(menu);
	}

	if (!menu->prompt) {
		return 0;
	}

	if (XLookupString(event, c, 32, &keysym, NULL) < 0) {
		return 0;
	}

	return menu_filter_add(menu, c);
}

Bool
menu_handle_move(menu_t *menu, int x, int y)
{
	menu->selected_previous = menu->selected_visible;
	menu->selected_visible = menu_calculate_entry(menu, x, y);

	if (menu->selected_visible == menu->selected_previous) {
		return False;
	}

	if (menu->selected_visible == -1) {
		menu->selected_visible = menu->selected_previous;
		return False;
	}

	menu_draw_selection(menu, menu->selected_visible);

	return True;
}

int
menu_handle_release(menu_t *menu, int x, int y)
{
	if ((x < menu->geometry.x) || (x > menu->geometry.x + menu->geometry.width)) {
		return -1;
	}

	if ((y < menu->geometry.y + menu->offset) || (y >= menu->geometry.y + menu->geometry.height - menu->padding)) {
		return -1;
	}

	return 2;
}

menu_t *
menu_init(state_t *state, screen_t *screen, char *prompt, Bool cycle, void (*callback)(state_t *, void *))
{
	menu_t *menu;
	XGCValues values;

	menu = calloc(1, sizeof(menu_t));
	memset(menu, 0, sizeof(menu_t));

	TAILQ_INIT(&menu->results);
	TAILQ_INIT(&menu->items);

	menu->screen = screen;
	menu->state = state;
	menu->callback = callback;
	menu->window = XCreateSimpleWindow(
			state->display,
			state->root,
			0,
			0,
			1,
			1,
			state->config->border_width,
			state->colors[COLOR_BORDER_ACTIVE].pixel,
			state->colors[COLOR_MENU_BACKGROUND].pixel);
	menu->draw = XftDrawCreate(
			state->display,
			menu->window,
			state->visual,
			state->colormap);
	if (prompt) {
		menu->prompt = strdup(prompt);
	} else {
		menu->prompt = NULL;
	}
	menu->selected_previous = -1;
	menu->offset = 0;
	menu->limit = 10; // state->config->menu_limit;
	menu->padding = 15; // state->config->menu_padding;
	menu->border_width = state->config->border_width;
	menu->cycle = cycle;

	x_set_class_hint(state->display, menu->window, state->config->wm_name);

	return menu;
}

int
menu_move_down(menu_t *menu)
{
	menu_item_t *item;

	if (menu->selected_item == menu->count - 1) {
		return 0;
	}

	if (menu->selected_visible == menu->limit - 1) {
		item = TAILQ_NEXT(menu->visible, result);
		if (!item) {
			return 0;
		}

		menu->visible = item;
		menu->selected_item++;
	} else {
		menu->selected_item++;
		menu->selected_visible++;
	}

	return 1;
}

int
menu_move_left(menu_t *menu)
{
	int result;

	result = menu_move_up(menu);
	if (result == 0) {
		//menu->visible = TAILQ_LAST(&menu->items, menu_item_q);
		menu->selected_item = menu->count - 1;
		menu->selected_visible = menu->count - 1;
		result = 1;
	}

	return result;
}

int
menu_move_right(menu_t *menu)
{
	int result;

	result = menu_move_down(menu);
	if (result == 0) {
		//menu->visible = TAILQ_FIRST(&menu->items);
		menu->selected_item = 0;
		menu->selected_visible = 0;
		result = 1;
	}

	return result;
}

int
menu_move_up(menu_t *menu)
{
	menu_item_t *item;

	if (menu->selected_item == 0) {
		return 0;
	}

	if (menu->selected_visible == 0) {
		item = TAILQ_PREV(menu->visible, menu_item_q, result);
		if (!item) {
			return 0;
		}

		menu->visible = item;
		menu->selected_item--;
	} else {
		menu->selected_item--;
		menu->selected_visible--;
	}

	return 1;
}
