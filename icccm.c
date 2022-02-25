#include <X11/Xatom.h>

#include "client.h"
#include "desktop.h"
#include "icccm.h"
#include "state.h"
#include "xutils.h"

void
icccm_delete_window(state_t *state, client_t *client)
{
	x_send_message(state->display, client->window, state->icccm->atoms[WM_PROTOCOLS], state->icccm->atoms[WM_DELETE_WINDOW], CurrentTime);
}

void
icccm_free(icccm_t *icccm)
{
	if (!icccm) {
		return;
	}

	free(icccm);
}

icccm_t *
icccm_init(state_t *state)
{
	char *names[] = {
		"WM_STATE",
		"WM_DELETE_WINDOW",
		"WM_TAKE_FOCUS",
		"WM_PROTOCOLS",
		"UTF8_STRING",
		"WM_CHANGE_STATE",
	};
	icccm_t *icccm;

	icccm = calloc(1, sizeof(icccm_t));
	if (!XInternAtoms(state->display, names, ICCCM_NITEMS, False, icccm->atoms)) {
		free(icccm);
		return NULL;
	}

	return icccm;
}

void
icccm_handle_property(state_t *state, client_t *client, Atom type)
{
	switch (type) {
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
			if (type == state->icccm->atoms[WM_PROTOCOLS]) {
				icccm_restore_wm_protocols(state, client);
			}

			break;
	}
}

void
icccm_restore_wm_protocols(state_t *state, client_t *client)
{
	Atom *protocols;
	int count, i;

	if (!XGetWMProtocols(state->display, client->window, &protocols, &count)) {
		return;
	}

	for (i = 0; i < count; i++) {
		if (protocols[i] == state->icccm->atoms[WM_DELETE_WINDOW]) {
			client->flags |= CLIENT_WM_DELETE_WINDOW;
		} else if (protocols[i] == state->icccm->atoms[WM_TAKE_FOCUS]) {
			client->flags |= CLIENT_WM_TAKE_FOCUS;
		}
	}
}

void
icccm_set_wm_state(state_t *state, client_t *client, long wm_state)
{
	long data[] = { wm_state, None };

	XChangeProperty(
			state->display,
			client->window,
			state->icccm->atoms[WM_STATE],
			state->icccm->atoms[WM_STATE],
			32,
			PropModeReplace,
			(unsigned char *)data,
			2);
}

void
icccm_take_focus(state_t *state, client_t *client)
{
	x_send_message(state->display, client->window, state->icccm->atoms[WM_PROTOCOLS], state->icccm->atoms[WM_TAKE_FOCUS], CurrentTime);
}

