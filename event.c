#include <stdio.h>

#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "group.h"
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

		if (!(client->flags & CLIENT_ACTIVE)) {
			client_activate(state, client);
		}
	} else {
		client = client_find_active(state);
		if (client) {
			client_deactivate(state, client);
		}
	}

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

	action = event->data.l[0];
	first = event->data.l[1];
	second = event->data.l[2];

	if (event->message_type == state->atoms[WM_CHANGE_STATE]) {
		if (action == IconicState) {
			client = client_find(state, event->window);
			if (!client) {
				return;
			}

			client_hide(state, client);
		}
	} else if (event->message_type == state->atoms[_NET_CLOSE_WINDOW]) {
		client = client_find(state, event->window);
		if (!client) {
			return;
		}

		client_close(state, client);
	} else if (event->message_type == state->atoms[_NET_ACTIVE_WINDOW]) {
		client = client_find(state, event->window);
		if (!client) {
			return;
		}

		client_show(state, client);
	} else if (event->message_type == state->atoms[_NET_WM_DESKTOP]) {
		printf("change desktop\n");
	} else if (event->message_type == state->atoms[_NET_WM_STATE]) {
		client = client_find(state, event->window);
		if (!client) {
			return;
		}

		if ((first == state->atoms[_NET_WM_STATE_STICKY]) ||
				(second == state->atoms[_NET_WM_STATE_STICKY])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_STICKY)) {
					client_toggle_sticky(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_STICKY) {
					client_toggle_sticky(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_sticky(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_MAXIMIZED_VERT]) ||
				(second == state->atoms[_NET_WM_STATE_MAXIMIZED_VERT])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_VMAXIMIZED)) {
					client_toggle_vmaximize(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_VMAXIMIZED) {
					client_toggle_vmaximize(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_vmaximize(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_MAXIMIZED_HORZ]) ||
				(second == state->atoms[_NET_WM_STATE_MAXIMIZED_HORZ])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_HMAXIMIZED)) {
					client_toggle_hmaximize(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_HMAXIMIZED) {
					client_toggle_hmaximize(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_hmaximize(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_HIDDEN]) ||
				(second == state->atoms[_NET_WM_STATE_HIDDEN])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_HIDDEN)) {
					client_toggle_hidden(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_HIDDEN) {
					client_toggle_hidden(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_hidden(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_FULLSCREEN]) ||
				(second == state->atoms[_NET_WM_STATE_FULLSCREEN])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_FULLSCREEN)) {
					client_toggle_fullscreen(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_FULLSCREEN) {
					client_toggle_fullscreen(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_fullscreen(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_DEMANDS_ATTENTION]) ||
				(second == state->atoms[_NET_WM_STATE_DEMANDS_ATTENTION])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_URGENCY)) {
					client_toggle_urgent(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_URGENCY) {
					client_toggle_urgent(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_urgent(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_SKIP_PAGER]) ||
				(second == state->atoms[_NET_WM_STATE_SKIP_PAGER])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_SKIP_PAGER)) {
					client_toggle_skip_pager(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_SKIP_PAGER) {
					client_toggle_skip_pager(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_skip_pager(state, client);
			}
		} else if ((first == state->atoms[_NET_WM_STATE_SKIP_TASKBAR]) ||
				(second == state->atoms[_NET_WM_STATE_SKIP_TASKBAR])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_SKIP_TASKBAR)) {
					client_toggle_skip_taskbar(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_SKIP_TASKBAR) {
					client_toggle_skip_taskbar(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_skip_taskbar(state, client);
			}
		} else if ((first == state->atoms[_CWM_WM_STATE_FREEZE]) ||
				(second == state->atoms[_CWM_WM_STATE_FREEZE])) {
			if (action == _NET_WM_STATE_ADD) {
				if (!(client->flags & CLIENT_FREEZE)) {
					client_toggle_freeze(state, client);
				}
			} else if (action == _NET_WM_STATE_REMOVE) {
				if (client->flags & CLIENT_FREEZE) {
					client_toggle_freeze(state, client);
				}
			} else if (action == _NET_WM_STATE_TOGGLE) {
				client_toggle_freeze(state, client);
			}
		}
	} else if (event->message_type == state->atoms[_NET_CURRENT_DESKTOP]) {
		printf("change desktop to %ld\n", event->data.l[0]);
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
	KeySym keysym;
	screen_t *screen;

	client = client_find_active(state);
	if (client) {
		screen = client->group->desktop->screen;
	} else {
		screen = screen_find_active(state);
	}

	keysym = XkbKeycodeToKeysym(state->display, event->keycode, 0, 0);

	TAILQ_FOREACH(binding, &state->config->keybindings, entry) {
		if ((event->state == binding->modifier) && (binding->button == keysym)) {
			if ((binding->context == BINDING_CONTEXT_CLIENT) && client) {
				binding->function(state, client, binding->flag);
			} else if ((binding->context == BINDING_CONTEXT_SCREEN) && screen) {
				binding->function(state, screen, binding->flag);
			} else if (binding->context == BINDING_CONTEXT_GLOBAL) {
				binding->function(state, screen, binding->flag);
			}
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

	client_show(state, client);
	client_activate(state, client);
}

void
event_handle_property_notify(state_t *state, XPropertyEvent *event)
{
	client_t *client;

	client = client_find(state, event->window);
	if (!client) {
		return;
	}

	switch (event->atom) {
		case XA_WM_NORMAL_HINTS:
			client_update_size_hints(state, client);
			break;
		case XA_WM_NAME:
			client_update_wm_name(state, client);
			break;
		case XA_WM_HINTS:
			client_update_wm_hints(state, client);
			break;
		case XA_WM_TRANSIENT_FOR:
			printf("got transient for\n");
			break;
		default:
			if (event->atom == state->atoms[WM_PROTOCOLS]) {
				client_update_wm_protocols(state, client);
			}
			/*
			for (int i = 0; i < EWMH_NITEMS; i++) {
				if (event->atom == state->atoms[i]) {
					printf("found %d\n", i);
				}
			}
			*/
			break;
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
			case MotionNotify:
				printf("motion notify\n");
				break;
				/*
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
