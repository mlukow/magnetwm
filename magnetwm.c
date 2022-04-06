#include <errno.h>
#include <locale.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "event.h"
#include "server.h"
#include "state.h"
#include "utils.h"

#define IDLE 0
#define RUNNING 1
#define RESTART 2
#define QUIT 3

volatile sig_atomic_t wm_state = IDLE;

void
signal_handler(int signum)
{
    int status;
    pid_t pid;

    switch (signum) {
        case SIGCHLD:
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0 || (pid < 0 && errno == EINTR));
            break;
        case SIGHUP:
            status = RESTART;
            break;
        case SIGINT:
        case SIGTERM:
            wm_state = QUIT;
            break;
    }
}

int
main(int argc, char **argv)
{
	char buf[BUFSIZ];
	fd_set descriptors;
	int bytes;
    state_t *state;
	struct passwd *pw;

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
		fprintf(stderr, "No locale support");
	}

	mbtowc(NULL, NULL, MB_CUR_MAX);

    state = state_init(NULL);
    if (!state) {
        return EXIT_FAILURE;
	}

    if ((signal(SIGCHLD, signal_handler) == SIG_ERR) ||
        (signal(SIGHUP, signal_handler) == SIG_ERR) ||
        (signal(SIGINT, signal_handler) == SIG_ERR) ||
        (signal(SIGTERM, signal_handler) == SIG_ERR)) {
        fprintf(stderr, "Could not register signal handlers\n");
	}

    wm_state = RUNNING;

    while (wm_state == RUNNING) {
		state_flush(state);

		FD_ZERO(&descriptors);
		FD_SET(state->server->fd, &descriptors);
		FD_SET(state->fd, &descriptors);

		if (select(MAX(state->fd, state->server->fd) + 1, &descriptors, NULL, NULL, NULL) <= 0) {
			continue;
		}

		if (FD_ISSET(state->server->fd, &descriptors)) {
			server_process(state, state->server);
		}

		if (FD_ISSET(state->fd, &descriptors)) {
			event_process(state);
		}
    }

    state_free(state);

    if (wm_state == RESTART) {
		sprintf(buf, "/proc/%d/exe", getpid());
		bytes = readlink(buf, buf, BUFSIZ);
		if (bytes > 0) {
			buf[bytes] = '\0';
	        xexec(buf);
		}
	}

    return EXIT_SUCCESS;
}
