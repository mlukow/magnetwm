#include <string.h>

#include "config.h"
#include "functions.h"
#include "queue.h"
#include "state.h"
#include "utils.h"

void
function_terminal(struct state_t *state, void *context, long flag)
{
	command_t *command;

	TAILQ_FOREACH(command, &state->config->commands, entry) {
		if (!strcmp(command->name, "terminal")) {
			xspawn(command->path);
		}
	}
}

void
function_window_center(struct state_t *state, void *context, long flag)
{
	printf("centering window\n");
}
