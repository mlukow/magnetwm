#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "desktop.h"
#include "group.h"

group_t *
group_assign(desktop_t *desktop, client_t *client)
{
	group_t *group;

	TAILQ_FOREACH(group, &desktop->groups, entry) {
		if (strcmp(group->name, client->class_name) == 0) {
			TAILQ_INSERT_TAIL(&group->clients, client, entry);
			return group;
		}
	}

	group = calloc(1, sizeof(group_t));
	group->name = strdup(client->class_name);
	TAILQ_INIT(&group->clients);
	TAILQ_INSERT_TAIL(&group->clients, client, entry);

	TAILQ_INSERT_TAIL(&desktop->groups, group, entry);

	return group;
}

void
group_free(group_t *group)
{
	client_t *client;

	if (!group) {
		return;
	}

	while ((client = TAILQ_FIRST(&group->clients)) != NULL) {
		TAILQ_REMOVE(&group->clients, client, entry);
		client_free(client);
	}

	free(group->name);
}

void
group_unassign(desktop_t *desktop, client_t *client)
{
	client_t *current_client;
	group_t *current_group;

	TAILQ_FOREACH(current_group, &desktop->groups, entry) {
		TAILQ_FOREACH(current_client, &current_group->clients, entry) {
			if (current_client == client) {
				TAILQ_REMOVE(&current_group->clients, client, entry);
				if (TAILQ_EMPTY(&current_group->clients)) {
					TAILQ_REMOVE(&desktop->groups, current_group, entry);
					group_free(current_group);
				}
			}
		}
	}
}
