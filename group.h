#ifndef __GROUP_H__
#define __GROUP_H__

#include "queue.h"

struct client_t;
struct desktop_t;

TAILQ_HEAD(client_q, client_t);

typedef struct group_t {
	TAILQ_ENTRY(group_t) entry;

	char *name;

	struct client_q clients;
} group_t;

group_t *group_assign(struct desktop_t *, struct client_t *);
void group_free(group_t *);
void group_unassign(struct desktop_t *, struct client_t *);

#endif /* __GROUP_H__ */
