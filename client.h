#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <X11/Xlib.h>

#include "ewmh.h"
#include "queue.h"
#include "xutils.h"

struct group_t;
struct state_t;

typedef enum client_flags_t {
	CLIENT_HIDDEN = 0x0001,
	CLIENT_IGNORE = 0x0002,
	CLIENT_VMAXIMIZED = 0x0004,
	CLIENT_HMAXIMIZED = 0x0008,
	CLIENT_FREEZE = 0x0010,
	CLIENT_GROUP = 0x0020,
	CLIENT_UNGROUP = 0x0040,
	CLIENT_INPUT = 0x0080,
	CLIENT_WM_DELETE_WINDOW = 0x0100,
	CLIENT_WM_TAKE_FOCUS = 0x0200,
	CLIENT_URGENCY = 0x0400,
	CLIENT_FULLSCREEN = 0x0800,
	CLIENT_STICKY = 0x1000,
	CLIENT_ACTIVE = 0x2000,
	CLIENT_SKIP_PAGER = 0x4000,
	CLIENT_SKIP_TASKBAR = 0x8000,
	CLIENT_SKIP_CYCLE = (CLIENT_HIDDEN | CLIENT_IGNORE | CLIENT_SKIP_TASKBAR | CLIENT_SKIP_PAGER),
	CLIENT_HIGHLIGHT = (CLIENT_GROUP | CLIENT_UNGROUP),
	CLIENT_MAXFLAGS = (CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED),
	CLIENT_MAXIMIZED = (CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
} client_flags_t;

typedef enum client_type_t {
	CLIENT_TYPE_DESKTOP,
	CLIENT_TYPE_DOCK,
	CLIENT_TYPE_TOOLBAR,
	CLIENT_TYPE_MENU,
	CLIENT_TYPE_UTILITY,
	CLIENT_TYPE_SPLASH,
	CLIENT_TYPE_DIALOG,
	CLIENT_TYPE_DROPDOWN_MENU,
	CLIENT_TYPE_POPUP_MENU,
	CLIENT_TYPE_TOOLTIP,
	CLIENT_TYPE_NOTIFICATION,
	CLIENT_TYPE_COMBO,
	CLIENT_TYPE_DND,
	CLIENT_TYPE_NORMAL
} client_type_t;

typedef struct client_t {
	TAILQ_ENTRY(client_t) entry;

	Window window;

	struct group_t *group;

	char *name;
	char *class_name;
	char *instance_name;

	struct {
		long flags;
		int desired_width;
		int desired_height;
		int min_width;
		int min_height;
		int max_width;
		int max_height;
		int width_increment;
		int height_increment;
		float min_aspect_ratio;
		float max_aspect_ratio;
	} hints;

	unsigned long flags;
	long initial_state;
	unsigned int border_width;
	Bool mapped;
	client_type_t type;

	geometry_t geometry;
	geometry_t geometry_saved;
	strut_t strut;
} client_t;

void client_activate(struct state_t *, client_t *, Bool);
void client_close(struct state_t *, client_t *);
void client_configure(struct state_t *, client_t *);
void client_deactivate(struct state_t *, client_t *);
void client_draw_border(struct state_t *, client_t *);
client_t *client_find(struct state_t *, Window);
client_t *client_find_active(struct state_t *);
void client_free(client_t *);
void client_hide(struct state_t *, client_t *);
client_t *client_init(struct state_t *, Window, Bool);
void client_lower(struct state_t *, client_t *);
void client_map(struct state_t *, client_t *);
void client_move_resize(struct state_t *, client_t *, geometry_t, Bool, Bool);
client_t *client_next(client_t *);
client_t *client_previous(client_t *);
void client_raise(struct state_t *, client_t *);
void client_remove(struct state_t *, client_t *);
void client_restore(struct state_t *, client_t *);
void client_show(struct state_t *, client_t *);
void client_toggle_freeze(struct state_t *, client_t *);
void client_toggle_fullscreen(struct state_t *, client_t *);
void client_toggle_hidden(struct state_t *, client_t *);
void client_toggle_hmaximize(struct state_t *, client_t *);
void client_toggle_maximize(struct state_t *, client_t *);
void client_toggle_skip_pager(struct state_t *, client_t *);
void client_toggle_skip_taskbar(struct state_t *, client_t *);
void client_toggle_sticky(struct state_t *, client_t *);
void client_toggle_urgent(struct state_t *, client_t *);
void client_toggle_vmaximize(struct state_t *, client_t *);
void client_unmap(struct state_t *, client_t *);
void client_update_size_hints(struct state_t *, client_t *);
void client_update_wm_hints(struct state_t *, client_t *);
void client_update_wm_name(struct state_t *, client_t *);
void client_update_wm_protocols(struct state_t *, client_t *);

#endif /* __CLIENT_H__ */
