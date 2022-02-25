#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "desktop.h"
#include "ewmh.h"
#include "group.h"
#include "screen.h"
#include "state.h"

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

void
desktop_switch_to_index(state_t *state, unsigned int index)
{
	client_t *client;
	group_t *group;
	int client_count, desktop_count = 0, i;
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (screen->active) {
			TAILQ_FOREACH(group, &screen->desktops[screen->desktop_index]->groups, entry) {
				group_hide(state, group);
			}

			TAILQ_FOREACH(group, &screen->desktops[index - desktop_count]->groups, entry) {
				group_show(state, group);
			}

			TAILQ_FOREACH_REVERSE(group, &screen->desktops[index - desktop_count]->groups, group_q, entry) {
				client_count = 0;
				TAILQ_FOREACH(client, &group->clients, entry) {
					if (!(client->flags & CLIENT_IGNORE)) {
						client_count++;
					}
				}

				if (client_count > 0) {
					group_activate(state, group);
					break;
				}
			}

			screen->desktop_index = index - desktop_count;

			ewmh_set_net_current_desktop_index(state, index);

			return;
		}

		desktop_count += screen->desktop_count;
	}
}
