#ifndef __ICCCM_H__
#define __ICCCM_H__

#include <X11/Xlib.h>

enum _icccm_t {
    WM_STATE,
    WM_DELETE_WINDOW,
    WM_TAKE_FOCUS,
    WM_PROTOCOLS,
    UTF8_STRING,
    WM_CHANGE_STATE,
    ICCCM_NITEMS
};

typedef struct icccm_t {
	Atom atoms[ICCCM_NITEMS];
} icccm_t;

struct client_t;
struct state_t;

void icccm_free(icccm_t *);
icccm_t *icccm_init(struct state_t *);

void icccm_delete_window(struct state_t *, struct client_t *);
Bool icccm_handle_property(struct state_t *, struct client_t *, Atom);
void icccm_restore_wm_protocols(struct state_t *, struct client_t *);
void icccm_set_wm_state(struct state_t *, struct client_t *, long);
void icccm_take_focus(struct state_t *, struct client_t *);

#endif /* __ICCCM_H__ */
