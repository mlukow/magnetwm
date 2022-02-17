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
			client->group = group;
			return group;
		}
	}

	group = calloc(1, sizeof(group_t));
	group->name = strdup(client->class_name);
	group->icon = client->icon;
	group->mask = client->mask;
	TAILQ_INIT(&group->clients);
	TAILQ_INSERT_TAIL(&group->clients, client, entry);
	client->group = group;

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
group_unassign(client_t *client)
{
	TAILQ_REMOVE(&client->group->clients, client, entry);
	if (TAILQ_EMPTY(&client->group->clients)) {
		TAILQ_REMOVE(&client->group->desktop->groups, client->group, entry);
		group_free(client->group);
	}

	client->group = NULL;
}
