#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "desktop.h"
#include "group.h"
#include "screen.h"
#include "state.h"

void
screen_activate(state_t *state, screen_t *screen)
{
	screen_t *current;

	current = screen_find_active(state);
	if (current == screen) {
		return;
	}

	if (current) {
		current->active = False;
	}

	screen->active = True;
}

void
screen_adopt(state_t *state, screen_t *screen, client_t *client)
{
	Bool dirty = False;
	group_t *group;

	group_unassign(client);

	group = group_assign(screen->desktops[screen->desktop_index], client);
	group->desktop = screen->desktops[screen->desktop_index];

	if (client->geometry.x + client->geometry.width < screen->geometry.x) {
		client->geometry.x = screen->geometry.x;
		dirty = True;
	}

	if (client->geometry.x > screen->geometry.x + screen->geometry.width) {
		client->geometry.x = screen->geometry.x + screen->geometry.width - client->geometry.width;
		dirty = True;
	}

	if (client->geometry.y + client->geometry.height < screen->geometry.y) {
		client->geometry.y = screen->geometry.y;
	}

	if (client->geometry.y > screen->geometry.y + screen->geometry.height) {
		client->geometry.y = screen->geometry.y + screen->geometry.height - client->geometry.height;
		dirty = True;
	}

	if (dirty) {
		client_move_resize(state, client, False);
	}
}

screen_t *
screen_find_active(state_t *state)
{
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (screen->active) {
			return screen;
		}
	}

	return NULL;
}

screen_t *
screen_find_by_name(state_t *state, char *name)
{
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (!strcmp(screen->name, name)) {
			return screen;
		}
	}

	return NULL;
}

screen_t *
screen_find_above(state_t *state, screen_t *screen)
{
	screen_t *current;

	TAILQ_FOREACH(current, &state->screens, entry) {
		if (current->geometry.y + current->geometry.height == screen->geometry.y) {
			return current;
		}
	}

	return NULL;
}

screen_t *
screen_find_below(state_t *state, screen_t *screen)
{
	screen_t *current;

	TAILQ_FOREACH(current, &state->screens, entry) {
		if (current->geometry.y == screen->geometry.y + screen->geometry.height) {
			return current;
		}
	}

	return NULL;
}

screen_t *
screen_find_left(state_t *state, screen_t *screen)
{
	screen_t *current;

	TAILQ_FOREACH(current, &state->screens, entry) {
		if (current->geometry.x + current->geometry.width == screen->geometry.x) {
			return current;
		}
	}

	return NULL;
}

screen_t *
screen_find_right(state_t *state, screen_t *screen)
{
	screen_t *current;

	TAILQ_FOREACH(current, &state->screens, entry) {
		if (current->geometry.x == screen->geometry.x + screen->geometry.width) {
			return current;
		}
	}

	return NULL;
}

screen_t *
screen_find_unwired(state_t *state)
{
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (!screen->wired) {
			return screen;
		}
	}

	return NULL;
}

screen_t *
screen_for_client(state_t *state, client_t *client)
{
	int current_distance, nearest_distance, x, y;
	screen_t *current, *nearest;

	x = client->geometry.x + client->geometry.width / 2;
	y = client->geometry.y + client->geometry.height / 2;

	nearest = screen_for_point(state, x, y);
	if (!nearest) {
		nearest_distance = INT_MAX;
		TAILQ_FOREACH(current, &state->screens, entry) {
			current_distance = x_distance(current->geometry, x, y);
			if (current_distance < nearest_distance) {
				nearest_distance = current_distance;
				nearest = current;
			}
		}
	}

	return nearest;
}

screen_t *
screen_for_point(state_t *state, int x, int y)
{
	screen_t *screen;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		if (x_contains_point(screen->geometry, x, y)) {
			return screen;
		}
	}

	return NULL;
}

void
screen_free(screen_t *screen)
{
	int i;

	if (!screen) {
		return;
	}

	for (i = 0; i < screen->desktop_count; i++) {
		desktop_free(screen->desktops[i]);
	}

	free(screen->desktops);
	free(screen->name);
	free(screen);
}

screen_t *
screen_init(state_t *state, char *name, RRCrtc id, geometry_t geometry, unsigned long width, unsigned long height)
{
	screen_t *screen;

	screen = calloc(1, sizeof(screen_t));
	screen->name = strdup(name);
	screen->id = id;
	screen->mm_width = width;
	screen->mm_height = height;

	screen->desktop_count = 4;
	screen->desktop_index = 0;
	screen->desktops = calloc(screen->desktop_count, sizeof(desktop_t));
	screen->desktops[0] = desktop_init("Desktop 1");
	screen->desktops[1] = desktop_init("Desktop 2");
	screen->desktops[2] = desktop_init("Desktop 3");
	screen->desktops[3] = desktop_init("Desktop 4");
	screen->desktops[screen->desktop_index]->screen = screen;

	screen_update_geometry(state, screen, geometry);

	return screen;
}

void
screen_update_geometry(state_t *state, screen_t *screen, geometry_t geometry)
{
	screen->geometry = geometry;
}
