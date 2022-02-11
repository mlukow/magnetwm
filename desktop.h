#ifndef __DESKTOP_H__
#define __DESKTOP_H__

#include "queue.h"

struct group_t;
struct screen_t;

TAILQ_HEAD(group_q, group_t);

typedef struct desktop_t {
	TAILQ_ENTRY(desktop_t) entry;

	char *name;

	struct group_q groups;
	struct screen_t *screen;
} desktop_t;

void desktop_free(desktop_t *);
desktop_t *desktop_init(char *);

#endif /* __DESKTOP_H__ */
