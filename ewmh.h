#ifndef __EWMH_H__
#define __EWMH_H__

#include <X11/Xlib.h>

enum _ewmh_t {
	_NET_ACTIVE_WINDOW,
	_NET_CLIENT_LIST,
	_NET_CLIENT_LIST_STACKING,
	_NET_CLOSE_WINDOW,
	_NET_CURRENT_DESKTOP,
	_NET_DESKTOP_GEOMETRY,
	_NET_DESKTOP_NAMES,
	_NET_DESKTOP_VIEWPORT,
	_NET_NUMBER_OF_DESKTOPS,
	_NET_SHOWING_DESKTOP,
	_NET_SUPPORTED,
	_NET_SUPPORTING_WM_CHECK,
	_NET_WM_DESKTOP,
	_NET_WM_NAME,

	_NET_WM_STATE,
	_NET_WM_STATE_DEMANDS_ATTENTION,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_SKIP_PAGER,
	_NET_WM_STATE_SKIP_TASKBAR,
	_NET_WM_STATE_STICKY,

	_NET_WM_WINDOW_TYPE,
	_NET_WM_WINDOW_TYPE_DOCK,
	_NET_WM_WINDOW_TYPE_DESKTOP,
	_NET_WM_WINDOW_TYPE_NOTIFICATION,
	_NET_WM_WINDOW_TYPE_DIALOG,
	_NET_WM_WINDOW_TYPE_UTILITY,
	_NET_WM_WINDOW_TYPE_TOOLBAR,

	EWMH_NITEMS
};

enum _net_wm_state_t {
    _NET_WM_STATE_REMOVE,
    _NET_WM_STATE_ADD,
    _NET_WM_STATE_TOGGLE
};

typedef struct ewmh_t {
	Atom atoms[EWMH_NITEMS];
} ewmh_t;

struct client_t;
struct state_t;

void ewmh_free(ewmh_t *);
Bool ewmh_get_net_wm_desktop(struct state_t *, struct client_t *, long *);
Atom *ewmh_get_net_wm_state(struct state_t *, struct client_t *, int *);
void ewmh_handle_net_wm_state_message(struct state_t *, struct client_t *, int, Atom, Atom);
ewmh_t *ewmh_init(struct state_t *);
void ewmh_restore_net_wm_state(struct state_t *, struct client_t *);
void ewmh_set_net_active_window(struct state_t *, struct client_t *);
void ewmh_set_net_client_list(struct state_t *);
void ewmh_set_net_client_list_stacking(struct state_t *);
void ewmh_set_net_current_desktop(struct state_t *);
void ewmh_set_net_current_desktop_index(struct state_t *, long);
void ewmh_set_net_desktop_geometry(struct state_t *);
void ewmh_set_net_desktop_names(struct state_t *);
void ewmh_set_net_desktop_viewport(struct state_t *);
void ewmh_set_net_number_of_desktops(struct state_t *);
void ewmh_set_net_showing_desktop(struct state_t *, Bool);
void ewmh_set_net_supported(struct state_t *);
void ewmh_set_net_wm_state(struct state_t *, struct client_t *);
void ewmh_set_net_workarea(struct state_t *);

/*
void xu_ewmh_net_supported_wm_check(struct screen_ctx *);
void xu_ewmh_net_virtual_roots(struct screen_ctx *);
void xu_ewmh_set_net_wm_desktop(struct client_ctx *);
*/

#endif /* __EWMH_H__ */
