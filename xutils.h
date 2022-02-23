#ifndef __XUTILS_H__
#define __XUTILS_H__

#include <X11/Xlib.h>

struct state_t;

typedef enum ewmh_t {
	// standard properties
	WM_STATE,
	WM_DELETE_WINDOW,
	WM_TAKE_FOCUS,
	WM_PROTOCOLS,
	UTF8_STRING,
	WM_CHANGE_STATE,

	// EWMH root properties
	_NET_SUPPORTED,
	_NET_CLIENT_LIST,
	_NET_CLIENT_LIST_STACKING,
	_NET_NUMBER_OF_DESKTOPS,
	_NET_DESKTOP_GEOMETRY,
	_NET_DESKTOP_VIEWPORT,
	_NET_CURRENT_DESKTOP,
	_NET_DESKTOP_NAMES,
	_NET_ACTIVE_WINDOW,
	_NET_WORKAREA,
	_NET_SUPPORTING_WM_CHECK,
	_NET_VIRTUAL_ROOTS,
	_NET_DESKTOP_LAYOUT,
	_NET_SHOWING_DESKTOP,

	// EWMH window properties
	_NET_WM_NAME,
	_NET_WM_VISIBLE_NAME,
	_NET_WM_ICON_NAME,
	_NET_WM_VISIBLE_ICON_NAME,
	_NET_WM_DESKTOP,
	_NET_CLOSE_WINDOW,
	_NET_WM_WINDOW_TYPE,
	_NET_WM_STATE,
	_NET_WM_ALLOWED_ACTIONS,
	_NET_WM_STRUT,
	_NET_WM_STRUT_PARTIAL,
	_NET_WM_ICON_GEOMETRY,
	_NET_WM_ICON,
	_NET_WM_PID,
	_NET_WM_HANDLED_ICONS,
	_NET_WM_USER_TIME,
	_NET_WM_USER_TIME_WINDOW,
	_NET_FRAME_EXTENTS,
	_NET_WM_OPAQUE_REGION,
	_NET_WM_BYPASS_COMPOSITOR,

	// states
	_NET_WM_STATE_STICKY,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_DEMANDS_ATTENTION,
	_NET_WM_STATE_SKIP_PAGER,
	_NET_WM_STATE_SKIP_TASKBAR,

	EWMH_NITEMS
} ewmh_t;

typedef enum net_wm_state_t {
    _NET_WM_STATE_REMOVE,
    _NET_WM_STATE_ADD,
    _NET_WM_STATE_TOGGLE
} net_wm_state_t;

typedef struct geometry_t {
	int x;
	int y;
	unsigned int width;
	unsigned int height;
} geometry_t;

void x_animate(Display *, Window, geometry_t, geometry_t, double);
Bool x_contains_point(geometry_t, int, int);
int x_distance(geometry_t, int, int);
Bool x_get_pointer(Display *, Window, int *, int *);
int x_get_property(Display *, Window, Atom, Atom, long, unsigned char **);

/*
void x_ewmh_set_client_list(struct state_t *);
void x_ewmh_set_client_list_stacking(struct state_t *);
*/

#endif /* __XUTILS_H__ */
