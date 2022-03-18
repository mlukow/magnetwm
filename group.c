#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "desktop.h"
#include "group.h"
#include "state.h"

void
group_activate(state_t *state, group_t *group)
{
	client_t *client;

	TAILQ_REMOVE(&group->desktop->groups, group, entry);
	TAILQ_INSERT_TAIL(&group->desktop->groups, group, entry);

	TAILQ_FOREACH(client, &group->clients, entry) {
		client_raise(state, client);
	}

	TAILQ_FOREACH_REVERSE(client, &group->clients, client_q, entry) {
		if (!(client->flags & CLIENT_IGNORE)) {
			client_activate(state, client, True);
			return;
		}
	}
}

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
	TAILQ_INIT(&group->clients);
	TAILQ_INSERT_TAIL(&group->clients, client, entry);
	client->group = group;

	TAILQ_INSERT_TAIL(&desktop->groups, group, entry);

	return group;
}

void
group_deactivate(state_t *state, group_t *group)
{
	client_t *client;

	TAILQ_FOREACH(client, &group->clients, entry) {
		client_deactivate(state, client);
	}
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
group_hide(state_t *state, group_t *group)
{
	client_t *client;

	TAILQ_FOREACH(client, &group->clients, entry) {
		if (!(client->flags & CLIENT_STICKY) && !(client->flags & CLIENT_HIDDEN)) {
			client_deactivate(state, client);
			client_hide(state, client);
		}
	}
}

void
group_show(state_t *state, group_t *group)
{
	client_t *client;

	TAILQ_FOREACH(client, &group->clients, entry) {
		if (client->flags & CLIENT_HIDDEN) {
			client_show(state, client);
		}
	}
}

void
group_unassign(client_t *client)
{
	if (!client->group) {
		return;
	}

	TAILQ_REMOVE(&client->group->clients, client, entry);
	if (TAILQ_EMPTY(&client->group->clients)) {
		TAILQ_REMOVE(&client->group->desktop->groups, client->group, entry);
		group_free(client->group);
	}

	client->group = NULL;
}
