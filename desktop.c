#include <stdlib.h>
#include <string.h>

#include "desktop.h"
#include "group.h"

void
desktop_free(desktop_t *desktop)
{
	group_t *group;

	if (!desktop) {
		return;
	}

	while ((group = TAILQ_FIRST(&desktop->groups)) != NULL) {
		TAILQ_REMOVE(&desktop->groups, group, entry);
		group_free(group);
	}

	free(desktop->name);
	free(desktop);
}

desktop_t *
desktop_init(char *name)
{
	desktop_t *desktop;

	desktop = calloc(1, sizeof(desktop_t));
	desktop->name = strdup(name);

	TAILQ_INIT(&desktop->groups);

	return desktop;
}
