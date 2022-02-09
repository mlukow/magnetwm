#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <X11/Xlib.h>

#include "queue.h"
#include "xutils.h"

struct group_t;
struct state_t;

typedef struct client_t {
	TAILQ_ENTRY(client_t) entry;

	Window window;

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

	long flags;
	unsigned int border_width;
	geometry_t geometry;
	Bool active;
	Bool delete_window;
	Bool take_focus;
} client_t;

client_t *client_active(struct state_t *);
void client_draw_border(struct state_t *, client_t *);
client_t *client_find(struct state_t *, Window);
void client_free(client_t *);
client_t *client_init(struct state_t *, Window);
void client_lower(struct state_t *, client_t *);
void client_move_resize(struct state_t *, client_t *);
void client_raise(struct state_t *, client_t *);
void client_remove(struct state_t *, client_t *);
void client_set_active(struct state_t *, client_t *);
void client_update_size_hints(struct state_t *, client_t *);
void client_update_wm_hints(struct state_t *, client_t *);
void client_update_wm_name(struct state_t *, client_t *);
void client_update_wm_protocols(struct state_t *, client_t *);

#endif /* __CLIENT_H__ */
