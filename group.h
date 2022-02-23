#ifndef __GROUP_H__
#define __GROUP_H__

#include <X11/Xlib.h>

#include "queue.h"

struct client_t;
struct desktop_t;
struct state_t;

TAILQ_HEAD(client_q, client_t);

typedef struct group_t {
	TAILQ_ENTRY(group_t) entry;

	Pixmap icon;
	Pixmap mask;

	char *name;

	struct desktop_t *desktop;
	struct client_q clients;
} group_t;

void group_activate(struct state_t *, group_t *);
group_t *group_assign(struct desktop_t *, struct client_t *);
void group_deactivate(struct state_t *, group_t *);
void group_free(group_t *);
void group_hide(struct state_t *, group_t *);
void group_show(struct state_t *, group_t *);
void group_unassign(struct client_t *);

#endif /* __GROUP_H__ */
