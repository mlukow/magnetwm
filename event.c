#include <stdio.h>

#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "ewmh.h"
#include "group.h"
#include "icccm.h"
#include "screen.h"
#include "state.h"

void event_handle_button_press(state_t *, XButtonPressedEvent *);
void event_handle_client_message(state_t *, XClientMessageEvent *);
void event_handle_configure_request(state_t *, XConfigureRequestEvent *);
void event_handle_destroy_notify(state_t *, XDestroyWindowEvent *);
void event_handle_key_press(state_t *, XKeyEvent *);
void event_handle_leave_notify(state_t *, XCrossingEvent *);
void event_handle_map_request(state_t *, XMapRequestEvent *);
void event_handle_property_notify(state_t *, XPropertyEvent *);
void event_handle_reparent_notify(state_t *, XReparentEvent *);

void
event_handle_button_press(state_t *state, XButtonPressedEvent *event)
{
	client_t *client;
	binding_t *binding;
	screen_t *screen;

	screen = screen_for_point(state, event->x_root, event->y_root);
	screen_activate(state, screen);

	client = client_find(state, event->subwindow);
	if (client) {
		client_raise(state, client);

		if (!(client->flags & CLIENT_ACTIVE) && !(client->flags & CLIENT_IGNORE)) {
			client_activate(state, client, True);
		}
	} else {
		client = client_find_active(state);
		if (client) {
			client_deactivate(state, client);
			client = NULL;
		}
	}

	event->state &= ~(LockMask | Mod2Mask | 0x2000);

	TAILQ_FOREACH(binding, &state->config->mousebindings, entry) {
		if ((event->state == binding->modifier) && (binding->button == event->button)) {
			if ((binding->context == BINDING_CONTEXT_CLIENT) && client) {
				binding->function(state, client, binding->flag);
			} else if ((binding->context == BINDING_CONTEXT_SCREEN) && screen) {
				binding->function(state, screen, binding->flag);
			} else if (binding->context == BINDING_CONTEXT_GLOBAL) {
				binding->function(state, screen, binding->flag);
			}
		}
	}

	XAllowEvents(state->display, ReplayPointer, event->time);
	XSync(state->display, 0);
}

void
event_handle_client_message(state_t *state, XClientMessageEvent *event)
{
	Atom first, second;
	client_t *client;
	int action;
	screen_t *screen;

	action = event->data.l[0];
	first = event->data.l[1];
	second = event->data.l[2];

	if (event->message_type == state->icccm->atoms[WM_CHANGE_STATE]) {
		if (action == IconicState) {
			client = client_find(state, event->window);
			if (client) {
				client_hide(state, client);
			}
		}
	} else if (event->message_type == state->ewmh->atoms[_NET_CLOSE_WINDOW]) {
		client = client_find(state, event->window);
		if (client) {
			client_close(state, client);
		}
	} else if (event->message_type == state->ewmh->atoms[_NET_ACTIVE_WINDOW]) {
		client = client_find(state, event->window);
		if (client) {
			client_show(state, client);
		}
	} else if (event->message_type == state->ewmh->atoms[_NET_WM_DESKTOP]) {
		printf("change desktop\n");
	} else if (event->message_type == state->ewmh->atoms[_NET_WM_STATE]) {
		client = client_find(state, event->window);
		if (client) {
			ewmh_handle_net_wm_state_message(state, client, action, first, second);
		}
	} else if (event->message_type == state->ewmh->atoms[_NET_CURRENT_DESKTOP]) {
		desktop_switch_to_index(state, event->data.l[0]);
	}
}

void
event_handle_configure_request(state_t *state, XConfigureRequestEvent *event)
{
	client_t *client;
	XWindowChanges changes;

	client = client_find(state, event->window);
	if (client) {
		if (event->value_mask & CWX) {
			client->geometry.x = event->x;
		}

		if (event->value_mask & CWY) {
			client->geometry.y = event->y;
		}

		if (event->value_mask & CWWidth) {
			client->geometry.width = event->width;
		}

		if (event->value_mask & CWHeight) {
			client->geometry.height = event->height;
		}

		if (event->value_mask & CWBorderWidth) {
			client->border_width = event->border_width;
		}
	}

	changes.x = event->x;
	changes.y = event->y;
	changes.width = event->width;
	changes.height = event->height;
	changes.border_width = event->border_width;
	changes.sibling = event->above;
	changes.stack_mode = event->detail;

	XConfigureWindow(state->display, event->window, event->value_mask, &changes);
}

void
event_handle_destroy_notify(state_t *state, XDestroyWindowEvent *event)
{
	client_t *client;

	client = client_find(state, event->window);
	if (!client) {
		return;
	}

	client_remove(state, client);
}

void
event_handle_key_press(state_t *state, XKeyEvent *event)
{
	binding_t *binding;
	client_t *client;
	KeySym keysym, skeysym;
	screen_t *screen;
	unsigned int modshift;

	client = client_find_active(state);
	if (client) {
		screen = client->group->desktop->screen;
	} else {
		screen = screen_find_active(state);
	}

	keysym = XkbKeycodeToKeysym(state->display, event->keycode, 0, 0);
	skeysym = XkbKeycodeToKeysym(state->display, event->keycode, 0, 1);

	event->state &= ~(LockMask | Mod2Mask | 0x2000);

	TAILQ_FOREACH(binding, &state->config->keybindings, entry) {
		if ((binding->button != keysym) && (binding->button == skeysym)) {
			modshift = ShiftMask;
		} else {
			modshift = 0;
		}

		if (event->state != (binding->modifier | modshift)) {
			continue;
		}

		if (binding->button != ((modshift == 0) ? keysym : skeysym)) {
			continue;
		}

		if ((binding->context == BINDING_CONTEXT_CLIENT) && client) {
			binding->function(state, client, binding->flag);
		} else if ((binding->context == BINDING_CONTEXT_SCREEN) && screen) {
			binding->function(state, screen, binding->flag);
		} else if (binding->context == BINDING_CONTEXT_GLOBAL) {
			binding->function(state, screen, binding->flag);
		}
	}

	XAllowEvents(state->display, ReplayKeyboard, event->time);
	XSync(state->display, 0);
}

void
event_handle_map_request(state_t *state, XMapRequestEvent *event)
{
	client_t *client;
	screen_t *screen;

	screen = TAILQ_LAST(&state->screens, screen_q);

	client = client_find(state, event->window);
	if (!client) {
		client = client_init(state, event->window, False);
		if (!client) {
			return;
		}

		screen_adopt(state, screen, client);
	}

	client->mapped = True;
	client_show(state, client);

	if (!(client->flags & CLIENT_IGNORE)) {
		client_activate(state, client, True);
	}

	TAILQ_REMOVE(&client->group->desktop->groups, client->group, entry);
	TAILQ_INSERT_TAIL(&client->group->desktop->groups, client->group, entry);
}

void
event_handle_property_notify(state_t *state, XPropertyEvent *event)
{
	client_t *client;

	client = client_find(state, event->window);
	if (!client) {
		return;
	}

	if (!icccm_handle_property(state, client, event->atom)) {
		ewmh_handle_property(state, client, event->atom);
	}
}

void
event_process(state_t *state)
{
	XEvent event;

	while (XPending(state->display)) {
		XNextEvent(state->display, &event);

		switch (event.type) {
			case KeyPress:
				event_handle_key_press(state, &event.xkey);
				break;
				/*
			case KeyRelease:
				printf("key release: %ld\n", event.xkey.time);
				break;
				*/
			case ButtonPress:
				event_handle_button_press(state, &event.xbutton);
				break;
			case ButtonRelease:
				printf("button release\n");
				break;
				/*
			case MotionNotify:
				printf("motion notify\n");
				break;
			case EnterNotify:
				event_handle_enter_notify(state, &event.xcrossing);
				break;
			case LeaveNotify:
				event_handle_leave_notify(state, &event.xcrossing);
				break;
				*/
			case FocusIn:
				printf("focus in\n");
				break;
			case FocusOut:
				printf("focus out\n");
				break;
			case KeymapNotify:
				printf("keymap notify\n");
				break;
			case Expose:
				printf("expose\n");
				break;
			case GraphicsExpose:
				printf("graphics expose\n");
				break;
				/*
			case NoExpose:
				printf("no expose\n");
				break;
				*/
			case VisibilityNotify:
				printf("visibility notify\n");
				break;
				/*
			case CreateNotify:
				event_handle_create_notify(state, &event.xcreatewindow);
				break;
				*/
			case DestroyNotify:
				event_handle_destroy_notify(state, &event.xdestroywindow);
				break;
				/*
			case UnmapNotify:
				printf("unmap notify 0x%lx\n", event.xunmap.window);
				break;
			case MapNotify:
				printf("map notify 0x%lx\n", event.xmap.window);
				break;
				*/
			case MapRequest:
				event_handle_map_request(state, &event.xmaprequest);
				break;
			case ReparentNotify:
				printf("reparent notify\n");
				break;
				/*
			case ConfigureNotify:
				printf("configure notify\n");
				break;
				*/
			case ConfigureRequest:
				event_handle_configure_request(state, (XConfigureRequestEvent *)&event.xconfigure);
				break;
			case GravityNotify:
				printf("gravity notify\n");
				break;
			case ResizeRequest:
				printf("resize request\n");
				break;
			case CirculateNotify:
				printf("circulate notify\n");
				break;
			case CirculateRequest:
				printf("circulate request\n");
				break;
			case PropertyNotify:
				event_handle_property_notify(state, &event.xproperty);
				break;
			case SelectionClear:
				printf("selection clear\n");
				break;
			case SelectionRequest:
				printf("selection request\n");
				break;
			case SelectionNotify:
				printf("selection notify\n");
				break;
			case ColormapNotify:
				printf("colormap notify\n");
				break;
			case ClientMessage:
				event_handle_client_message(state, (XClientMessageEvent *)&event.xclient);
				break;
			case MappingNotify:
				printf("mapping notify\n");
				break;
				/*
			default:
				printf("unknown event: 0x%x\n", event.type);
				*/
		}
	}
}
