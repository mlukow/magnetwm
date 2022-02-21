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

int menu_calculate_entry(menu_t *, int, int, Bool);
void menu_draw_horizontal(menu_t *);
void menu_draw_selection_horizontal(menu_t *, int);
void menu_draw_selection_vertical(menu_t *, int);
void menu_draw_vertical(menu_t *);
int menu_filter_add(menu_t *, char *);
int menu_filter_complete(menu_t *);
int menu_filter_delete(menu_t *);
void menu_filter_update(menu_t *);
int menu_handle_key(menu_t *, XKeyEvent *, Bool);
Bool menu_handle_move(menu_t *, int, int, Bool);
int menu_handle_release(menu_t *, int, int);
int menu_move_down(menu_t *);
int menu_move_left(menu_t *);
int menu_move_right(menu_t *);
int menu_move_up(menu_t *);

void
menu_add(menu_t *menu, void *context, Bool sorted, char *text, Pixmap icon, Pixmap mask)
{
	int cmp;
	menu_item_t *current, *item;

	item = malloc(sizeof(menu_item_t));
	item->context = context;
	item->text = strdup(text);
	item->icon = icon;
	item->mask = mask;

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
menu_calculate_entry(menu_t *menu, int x, int y, Bool cycle)
{
	if ((x < menu->geometry.x) || (x > menu->geometry.x + menu->geometry.width)) {
		return -1;
	}

	if ((y < menu->geometry.y) || (y > menu->geometry.y + menu->geometry.height)) {
		return -1;
	}

	if (cycle) {
		x -= menu->geometry.x + menu->border_width;
		if ((x < 0) || (x > menu->geometry.width)) {
			return -1;
		}

		return x / menu->geometry.height;
	}

	if ((y < menu->geometry.y + menu->offset + menu->padding) || (y >= menu->geometry.y + menu->geometry.height - menu->padding)) {
		return -1;
	}

	y -= menu->geometry.y + menu->offset + menu->padding + menu->border_width;

	return y / (menu->state->fonts[FONT_MENU_ITEM]->height + 1);
}

void *
menu_cycle(menu_t *menu)
{
	Bool processing = True;
	int focusrevert, i = 0;
	menu_item_t *item;
	Window focus;
	XEvent event;

	TAILQ_FOREACH(item, &menu->items, item) {
		TAILQ_INSERT_TAIL(&menu->results, item, result);
		menu->count++;
	}

	if (menu->count < 2) {
		return NULL;
	}

	menu->selected_item = 1;
	menu->selected_visible = 1;
	menu->geometry.height = 180;
	menu->geometry.width = menu->count * menu->geometry.height;
	menu->geometry.x = menu->screen->geometry.x + (menu->screen->geometry.width - menu->geometry.width) / 2;
	menu->geometry.y = menu->screen->geometry.y + (menu->screen->geometry.height - menu->geometry.height) / 2;

	menu->visible = TAILQ_FIRST(&menu->results);

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

	XMoveResizeWindow(
			menu->state->display,
			menu->window,
			menu->geometry.x,
			menu->geometry.y,
			menu->geometry.width,
			menu->geometry.height);

	menu->window_picture = XRenderCreatePicture(
			menu->state->display,
			menu->window,
			XRenderFindVisualFormat(menu->state->display, menu->state->visual),
			0,
			NULL);

	menu_draw_horizontal(menu);

	while (processing) {
		XWindowEvent(menu->state->display, menu->window, MENUMASK, &event);
		switch (event.type) {
			case KeyPress:
				i = menu_handle_key(menu, &event.xkey, True);
				if (i == 1) {
					menu_draw_horizontal(menu);
				} else if ((i == -1) || ((i == 2) && (menu->count > 0))) {
					processing = False;
				}

				break;
			case KeyRelease:
				if ((event.xkey.keycode == 0x40) && (event.xkey.state & Mod1Mask)) {
					processing = False;
				}

				break;
			case Expose:
				menu_draw_horizontal(menu);
				break;
			case MotionNotify:
				if (menu_handle_move(menu, event.xbutton.x_root, event.xbutton.y_root, True)) {
					menu_draw_horizontal(menu);
				}
				break;
			case ButtonPress:
				if (event.xbutton.button == Button4) {
					menu_move_right(menu);
					menu_draw_horizontal(menu);
				} else if (event.xbutton.button == Button5) {
					menu_move_left(menu);
					menu_draw_horizontal(menu);
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

	if (i == -1) {
		return NULL;
	}

	item = menu->visible;
	for (i = 0; i < menu->selected_visible; i++) {
		item = TAILQ_NEXT(item, result);
	}

	return (item == NULL) ? NULL : item->context;
}

void
menu_draw_horizontal(menu_t *menu)
{
	double size;
	int i = 0, x, y;
	menu_item_t *item;
	Picture icon_picture, mask_picture;
	unsigned int border_width, depth, height, width;
	Window root;
	XTransform transform;

	size = 0.5 * menu->geometry.height;

	memset(&transform, 0, sizeof(XTransform));
	transform.matrix[2][2] = XDoubleToFixed(1);

	XClearWindow(menu->state->display, menu->window);

	item = menu->visible;
	while ((i < menu->limit) && item) {
		XGetGeometry(menu->state->display, item->icon, &root, &x, &y, &width, &height, &border_width, &depth);
		transform.matrix[0][0] = XDoubleToFixed((double)width / size);
		transform.matrix[1][1] = XDoubleToFixed((double)height / size);

		icon_picture = XRenderCreatePicture(
				menu->state->display,
				item->icon,
				XRenderFindVisualFormat(menu->state->display, menu->state->visual),
				0,
				NULL);
		XRenderSetPictureTransform(menu->state->display, icon_picture, &transform);

		mask_picture = XRenderCreatePicture(
				menu->state->display,
				item->mask,
				XRenderFindStandardFormat(menu->state->display, PictStandardA1),
				0,
				NULL);
		XRenderSetPictureTransform(menu->state->display, mask_picture, &transform);

		XRenderComposite(
				menu->state->display,
				PictOpOver,
				icon_picture,
				mask_picture,
				menu->window_picture,
				x,
				y,
				x,
				y,
				i * menu->geometry.height + (menu->geometry.height - size) / 2,
				(menu->geometry.height - size) / 2,
				size,
				size);

		XRenderFreePicture(menu->state->display, icon_picture);
		XRenderFreePicture(menu->state->display, mask_picture);

		item = TAILQ_NEXT(item, result);
		i++;
	}

	if (i > 0) {
		menu_draw_selection_horizontal(menu, menu->selected_visible);
	}
}

void
menu_draw_selection_horizontal(menu_t *menu, int entry)
{
	double size;
	int i, x, y;
	menu_item_t *item = menu->visible;
	Picture icon_picture, mask_picture;
	unsigned int border_width, depth, height, width;
	Window root;
	XftFont *font;
	XGlyphInfo extents;
	XTransform transform;

	for (i = 0; i < entry; i++) {
		item = TAILQ_NEXT(item, result);
	}

	if (!item) {
		return;
	}

	size = 0.5 * menu->geometry.height;

	memset(&transform, 0, sizeof(XTransform));
	transform.matrix[2][2] = XDoubleToFixed(1);

	font = menu->state->fonts[FONT_MENU_ITEM];

	XGetGeometry(menu->state->display, item->icon, &root, &x, &y, &width, &height, &border_width, &depth);
	transform.matrix[0][0] = XDoubleToFixed((double)width / size);
	transform.matrix[1][1] = XDoubleToFixed((double)height / size);

	icon_picture = XRenderCreatePicture(
			menu->state->display,
			item->icon,
			XRenderFindVisualFormat(menu->state->display, menu->state->visual),
			0,
			NULL);
	XRenderSetPictureTransform(menu->state->display, icon_picture, &transform);

	mask_picture = XRenderCreatePicture(
			menu->state->display,
			item->mask,
			XRenderFindStandardFormat(menu->state->display, PictStandardA1),
			0,
			NULL);
	XRenderSetPictureTransform(menu->state->display, mask_picture, &transform);

	XftDrawRect(
			menu->draw,
			&menu->state->colors[COLOR_MENU_SELECTION_BACKGROUND],
			entry * menu->geometry.height + 2 * menu->padding,
			2 * menu->padding,
			menu->geometry.height - 4 * menu->padding,
			menu->geometry.height - 4 * menu->padding);

	XRenderComposite(
			menu->state->display,
			PictOpOver,
			icon_picture,
			mask_picture,
			menu->window_picture,
			x,
			y,
			x,
			y,
			MAX(entry * menu->geometry.height + (menu->geometry.height - size) / 2, 0),
			(menu->geometry.height - size) / 2,
			size,
			size);

	XRenderFreePicture(menu->state->display, icon_picture);
	XRenderFreePicture(menu->state->display, mask_picture);

	XftTextExtentsUtf8(menu->state->display, font, (const FcChar8 *)item->text, strlen(item->text), &extents);
	XftDrawStringUtf8(
			menu->draw,
			&menu->state->colors[COLOR_MENU_SELECTION_FOREGROUND],
			font,
			entry * menu->geometry.height + (menu->geometry.height - extents.xOff) / 2,
			menu->geometry.height - (2 * menu->padding - font->ascent) / 2,
			(const FcChar8 *)item->text,
			strlen(item->text));
}

void
menu_draw_selection_vertical(menu_t *menu, int entry)
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
			menu->offset + menu->padding + entry * (font->height + 1),
			menu->geometry.width,
			font->height + 1);

	XftDrawStringUtf8(
			menu->draw,
			&menu->state->colors[COLOR_MENU_SELECTION_FOREGROUND],
			font,
			menu->padding,
			menu->offset + menu->padding + entry * (font->height + 1) + font->ascent + 1,
			(const FcChar8 *)item->text,
			strlen(item->text));
}

void
menu_draw_vertical(menu_t *menu)
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

		XftDrawRect(
				menu->draw,
				&menu->state->colors[COLOR_MENU_SEPARATOR],
				0,
				menu->offset - 1,
				menu->geometry.width,
				1);
	}

	item = menu->visible;
	font = menu->state->fonts[FONT_MENU_ITEM];
	while ((i < menu->limit) && item) {
		y = menu->offset + menu->padding + i * (font->height + 1) + font->ascent + 1;
		XftDrawStringUtf8(
				menu->draw,
				&menu->state->colors[COLOR_MENU_FOREGROUND],
				font,
				menu->padding,
				y,
				(const FcChar8 *)item->text,
				strlen(item->text));
		item = TAILQ_NEXT(item, result);
		i++;
	}

	if (i > 0) {
		menu_draw_selection_vertical(menu, menu->selected_visible);
	}
}

void *
menu_filter(menu_t *menu)
{
	Bool processing = True;
	int focusrevert, i;
	menu_item_t *item;
	Window focus;
	XEvent event;

	TAILQ_FOREACH(item, &menu->items, item) {
		TAILQ_INSERT_TAIL(&menu->results, item, result);
		menu->count++;
	}

	if (menu->count == 0) {
		return NULL;
	}

	menu->geometry.width = MIN(0.3 * menu->screen->geometry.width, 650);
	if (menu->prompt) {
		menu->offset = 2 * menu->padding + menu->state->fonts[FONT_MENU_INPUT]->height + 1;
	}

	menu->geometry.x = menu->screen->geometry.x + (menu->screen->geometry.width - menu->geometry.width) / 2;
	menu->geometry.y = menu->screen->geometry.y + 0.2 * menu->screen->geometry.height;

	menu->visible = TAILQ_FIRST(&menu->results);

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

	while (processing) {
		XWindowEvent(menu->state->display, menu->window, MENUMASK, &event);
		switch (event.type) {
			case KeyPress:
				i = menu_handle_key(menu, &event.xkey, False);
				if (i == 1) {
					menu_draw_vertical(menu);
				} else if ((i == -1) || ((i == 2) && (menu->count > 0))) {
					processing = False;
				}

				break;
			case Expose:
				menu_draw_vertical(menu);
				break;
			case MotionNotify:
				if (menu_handle_move(menu, event.xbutton.x_root, event.xbutton.y_root, False)) {
					menu_draw_vertical(menu);
				}
				break;
			case ButtonPress:
				if (event.xbutton.button == Button4) {
					menu_move_up(menu);
					menu_draw_vertical(menu);
				} else if (event.xbutton.button == Button5) {
					menu_move_down(menu);
					menu_draw_vertical(menu);
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
menu_handle_key(menu_t *menu, XKeyEvent *event, Bool cycle)
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
			return cycle ? menu_move_right(menu) : menu_filter_complete(menu);
		case XK_ISO_Left_Tab:
			return cycle ? menu_move_left(menu) : 0;
		case XK_b:
			if (cycle && (event->state & ControlMask)) {
				return menu_move_left(menu);
			}

			break;
		case XK_Left:
			return cycle ? menu_move_left(menu) : 0;
		case XK_Up:
			return cycle ? 0 : menu_move_up(menu);
		case XK_p:
			if (!cycle && (event->state & ControlMask)) {
				return menu_move_up(menu);
			}

			break;
		case XK_f:
			if (cycle && (event->state & ControlMask)) {
				return menu_move_right(menu);
			}

			break;
		case XK_Right:
			return cycle ? menu_move_right(menu) : 0;
		case XK_Down:
			return cycle ? 0 : menu_move_down(menu);
		case XK_n:
			if (!cycle && (event->state & ControlMask)) {
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
menu_handle_move(menu_t *menu, int x, int y, Bool cycle)
{
	menu->selected_previous = menu->selected_visible;
	menu->selected_visible = menu_calculate_entry(menu, x, y, cycle);

	if (menu->selected_visible == menu->selected_previous) {
		return False;
	}

	if (menu->selected_visible == -1) {
		menu->selected_visible = menu->selected_previous;
		return False;
	}

	if (cycle) {
		menu_draw_selection_horizontal(menu, menu->selected_visible);
	} else {
		menu_draw_selection_vertical(menu, menu->selected_visible);
	}

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
menu_init(state_t *state, screen_t *screen, char *prompt)
{
	menu_t *menu;
	XClassHint *hint;
	XGCValues values;

	menu = calloc(1, sizeof(menu_t));
	memset(menu, 0, sizeof(menu_t));

	TAILQ_INIT(&menu->results);
	TAILQ_INIT(&menu->items);

	menu->screen = screen;
	menu->state = state;
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
	menu->gc = XCreateGC(state->display, menu->window, 0, &values);
	menu->window_picture = None;
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

	hint = XAllocClassHint();
	hint->res_name = strdup("magnetwm");
	hint->res_class = strdup("magnetwm");
	XSetClassHint(state->display, menu->window, hint);
	XFree(hint);

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
