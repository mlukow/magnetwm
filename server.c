#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "client.h"
#include "config.h"
#include "desktop.h"
#include "ewmh.h"
#include "group.h"
#include "queue.h"
#include "screen.h"
#include "server.h"
#include "state.h"
#include "xutils.h"

#define SOCKET_PATTERN "/tmp/magnetwm%s_%i_%i-socket"

void server_bind_key(state_t *, char **);
void server_bind_mouse(state_t *, char **);
void server_border_width(state_t *, char **);
void server_color(state_t *, char **);
void server_command(state_t *, char **);
void server_font(state_t *, char **);
void server_ignore(state_t *, char **);
void server_wm_name(state_t *, char **);

static const struct {
	char *name;
	int num_args;
	void (*function)(state_t *, char **);
} name_to_func[] = {
#define FUNC_CMD(n, c, f) #n, c, server_ ## f
	{ FUNC_CMD(bind-key, 2, bind_key) },
	{ FUNC_CMD(bind-mouse, 2, bind_mouse) },
	{ FUNC_CMD(border-width, 1, border_width) },
	{ FUNC_CMD(color, 2, color) },
	{ FUNC_CMD(command, 2, command) },
	{ FUNC_CMD(font, 2, font) },
	{ FUNC_CMD(ignore, 1, ignore) },
	{ FUNC_CMD(wm-name, 1, wm_name) },
#undef FUNC_CMD
};

void
server_bind_key(state_t *state, char **argv)
{
	binding_t *binding;
	int i;

	TAILQ_FOREACH(binding, &state->config->keybindings, entry) {
		if (!strcmp(binding->name, argv[1])) {
			XUngrabKey(
					state->display,
					XKeysymToKeycode(state->display, binding->button),
					binding->modifier,
					state->root);
			break;
		}
	}

	if (config_bind_key(state->config, argv[0], argv[1])) {
		TAILQ_FOREACH(binding, &state->config->keybindings, entry) {
			if (!strcmp(binding->name, argv[1])) {
				XGrabKey(
						state->display,
						XKeysymToKeycode(state->display, binding->button),
						binding->modifier,
						state->root,
						True,
						GrabModeAsync,
						GrabModeAsync);
			}
		}
	}
}

void
server_bind_mouse(state_t *state, char **argv)
{
	config_bind_mouse(state->config, argv[0], argv[1]);
}

void
server_border_width(state_t *state, char **argv)
{
	client_t *client;
	group_t *group;
	int border_width, i;
	screen_t *screen;

	border_width = atoi(argv[0]);
	state->config->border_width = border_width;

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				TAILQ_FOREACH(client, &group->clients, entry) {
					if (!(client->flags & CLIENT_IGNORE) && !(client->flags & CLIENT_FULLSCREEN)) {
						client->border_width = border_width;
						client_draw_border(state, client);
					}
				}
			}
		}
	}
}

void
server_color(state_t *state, char **argv)
{
	client_t *client;
	group_t *group;
	int i, result;
	screen_t *screen;
	struct colors {
		char *name;
		color_t color;
	} colors[] = {
		{ "border-active", COLOR_BORDER_ACTIVE },
		{ "border-inactive", COLOR_BORDER_INACTIVE },
		{ "border-urgent", COLOR_BORDER_URGENT },
		{ "menu-background", COLOR_MENU_BACKGROUND },
		{ "menu-foreground", COLOR_MENU_FOREGROUND },
		{ "menu-prompt", COLOR_MENU_PROMPT },
		{ "menu-selection-background", COLOR_MENU_SELECTION_BACKGROUND },
		{ "menu-selection-foreground", COLOR_MENU_SELECTION_FOREGROUND },
		{ "menu-separator", COLOR_MENU_SEPARATOR }
	};
	XftColor color;

	for (i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
		if (!strcmp(colors[i].name, argv[0])) {
			result = XftColorAllocName(
					state->display,
					state->visual,
					state->colormap,
					argv[1],
					&color);
			if (result) {
				free(state->config->colors[colors[i].color]);
				state->config->colors[colors[i].color] = strdup(argv[1]);

				XftColorFree(state->display, state->visual, state->colormap, &state->colors[colors[i].color]);
				state->colors[colors[i].color] = color;

				TAILQ_FOREACH(screen, &state->screens, entry) {
					for (i = 0; i < screen->desktop_count; i++) {
						TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
							TAILQ_FOREACH(client, &group->clients, entry) {
								client_draw_border(state, client);
							}
						}
					}
				}
			} else {
				fprintf(stderr, "Invalid color: %s\n", argv[1]);
			}

			return;
		}
	}

	fprintf(stderr, "Unknown color: %s\n", argv[0]);
}

void
server_command(state_t *state, char **argv)
{
	config_bind_command(state->config, argv[0], argv[1]);
}

void
server_font(state_t *state, char **argv)
{
	int i;
	struct fonts {
		char *name;
		font_t font;
	} fonts[] = {
		{ "menu-input", FONT_MENU_INPUT },
		{ "menu-item", FONT_MENU_ITEM },
		{ "menu-item-detail", FONT_MENU_ITEM_DETAIL }
	};
	XftFont *font;

	for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
		if (!strcmp(fonts[i].name, argv[0])) {
			font = XftFontOpenXlfd(state->display, state->primary_screen, argv[1]);

			if (!font) {
				font = XftFontOpenName(state->display, state->primary_screen, argv[1]);
			}

			if (font) {
				free(state->config->fonts[fonts[i].font]);
				state->config->fonts[fonts[i].font] = strdup(argv[1]);
				state->fonts[fonts[i].font] = font;
			} else {
				fprintf(stderr, "Invalid font: %s\n", argv[1]);
			}
		}
	}
}

void
server_free(server_t *server)
{
	char buf[BUFSIZ], *host;
	int dn, sn;

	if (!server) {
		return;
	}

	close(server->fd);

	if (x_parse_display(NULL, &host, &dn, &sn)) {
		snprintf(buf, BUFSIZ, SOCKET_PATTERN, host, dn, sn);
		unlink(buf);
		free(host);
	}

	free(server);
}

void
server_ignore(state_t *state, char **argv)
{
	client_t *client;
	group_t *group;
	int i;
	screen_t *screen;

	config_ignore(state->config, argv[0]);

	TAILQ_FOREACH(screen, &state->screens, entry) {
		for (i = 0; i < screen->desktop_count; i++) {
			TAILQ_FOREACH(group, &screen->desktops[i]->groups, entry) {
				if (!strcmp(group->name, argv[0])) {
					TAILQ_FOREACH(client, &group->clients, entry) {
						client->flags |= CLIENT_IGNORE;
						client->border_width = 0;
						client_configure(state, client);
						client_draw_border(state, client);
					}
				}
			}
		}
	}
}

server_t *
server_init()
{
	char *host;
	int dn, fd, sn;
	server_t *server;
	struct sockaddr_un address;

	if (x_parse_display(NULL, &host, &dn, &sn) <= 0) {
		fprintf(stderr, "Could not parse display.\n");
		return NULL;
	}

	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, sizeof(address.sun_path), SOCKET_PATTERN, host, dn, sn);
	free(host);

	if (access(address.sun_path, F_OK) == 0) {
		unlink(address.sun_path);
	}

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		fprintf(stderr, "Could not open socket.\n");
		return NULL;
	}

	if (bind(fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
		fprintf(stderr, "Could not bind a name to the socket.\n");
		close(fd);
		return NULL;
	}

	if (listen(fd, SOMAXCONN) == -1) {
		fprintf(stderr, "Could not listen to the socket.\n");
		close(fd);
		return NULL;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC | fcntl(fd, F_GETFD));

	server = calloc(1, sizeof(server_t));
	server->fd = fd;

	return server;
}

void
server_process(state_t *state, server_t *server)
{
	char **argv, buf[BUFSIZ];
	int argc, client_fd, i, size;
	void (*function)(state_t *, char **) = NULL;

	client_fd = accept(server->fd, NULL, 0);
	if (client_fd < 0) {
		return;
	}

	if ((size = recv(client_fd, buf, BUFSIZ - 1, 0)) <= 0) {
		fprintf(stderr, "Could not read from client socket.\n");
		close(client_fd);
		return;
	}

	if (send(client_fd, "OK", 2, 0) == -1) {
		fprintf(stderr, "Could not send response.\n");
		close(client_fd);
		return;
	}

	buf[size] = '\0';

	for (i = 0; i < sizeof(name_to_func) / sizeof(name_to_func[0]); i++) {
		if (!strncmp(name_to_func[i].name, buf, size)) {
			argc = name_to_func[i].num_args;
			function = name_to_func[i].function;
			break;
		}
	}

	if (!function) {
		fprintf(stderr, "Could not find function %s\n", buf);
		close(client_fd);
		return;
	}

	argv = calloc(argc, sizeof(char *));

	for (i = 0; i < argc; i++) {
		if ((size = recv(client_fd, buf, BUFSIZ - 1, 0)) <= 0) {
			fprintf(stderr, "Invalid number of arguments: %d\n", i);
			close(client_fd);
			return;
		}

		if (send(client_fd, "OK", 2, 0) == -1) {
			fprintf(stderr, "Could not send response.\n");
			close(client_fd);
			return;
		}

		argv[i] = strndup(buf, size);
	}

	close(client_fd);

	function(state, argv);

	for (i = 0; i < argc; i++) {
		free(argv[i]);
	}

	free(argv);
}

void
server_wm_name(state_t *state, char **argv)
{
	free(state->config->wm_name);
	state->config->wm_name = strdup(argv[0]);
	ewmh_set_net_wm_name(state);
	x_set_class_hint(state->display, state->root, state->config->wm_name);
}
