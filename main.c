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
	char buf[BUFSIZ], *home, *file;
	int bytes;
    state_t *state;
	struct passwd *pw;
    struct pollfd pfd[1];

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
		fprintf(stderr, "No locale support");
	}

	mbtowc(NULL, NULL, MB_CUR_MAX);

	home = getenv("HOME");
	if ((home == NULL) || (*home == '\0')) {
		pw = getpwuid(getuid());
		if (pw && pw->pw_dir && (*pw->pw_dir != '\0')) {
			home = pw->pw_dir;
		} else {
			home = "/";
		}
	}

	xasprintf(&file, "%s/%s", home, ".magnetwmrc");

    state = state_init(NULL, file);
    if (!state) {
        return EXIT_FAILURE;
	}

    if ((signal(SIGCHLD, signal_handler) == SIG_ERR) ||
        (signal(SIGHUP, signal_handler) == SIG_ERR) ||
        (signal(SIGINT, signal_handler) == SIG_ERR) ||
        (signal(SIGTERM, signal_handler) == SIG_ERR)) {
        fprintf(stderr, "Could not register signal handlers\n");
	}

    memset(&pfd, 0, sizeof(pfd));
    pfd[0].fd = state->fd;
    pfd[0].events = POLLIN;

    wm_state = RUNNING;

    while (wm_state == RUNNING) {
		event_process(state);

        if (poll(pfd, 1, -1) == -1) {
            if (errno != EINTR) {
                fprintf(stderr, "Could not poll\n");
			}
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
