#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <X11/extensions/Xrandr.h>

#include "queue.h"
#include "xutils.h"

struct client_t;
struct desktop_t;
struct state_t;

typedef struct screen_t {
	TAILQ_ENTRY(screen_t) entry;

	char *name;
	RRCrtc id;
	Bool wired;
	unsigned long mm_width;
	unsigned long mm_height;
	
	geometry_t geometry;

	struct desktop_t **desktops;
	int desktop_count;
	int desktop_index;
} screen_t;

void screen_adopt(struct state_t *, screen_t *, struct client_t *);
screen_t *screen_find_by_name(struct state_t *, char *);
screen_t *screen_for_client(struct state_t *, struct client_t *);
screen_t *screen_for_point(struct state_t *, int, int);
screen_t *screen_find_unwired(struct state_t *);
void screen_free(screen_t *);
screen_t *screen_init(struct state_t *, char *, RRCrtc, geometry_t, unsigned long, unsigned long);
void screen_update_geometry(struct state_t *, screen_t *, geometry_t);

#endif /* __SCREEN_H__ */
