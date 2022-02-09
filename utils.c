#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

#ifndef HAVE_STRTONUM
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define INVALID 1
#define TOOSMALL 2
#define TOOLARGE 3

long long
strtonum(char *numstr, long long minval, long long maxval, char **errstrp)
{
	long long ll = 0;
	int error = 0;
	char *ep;
	struct errval {
		char *errstr;
		int err;
	} ev[4] = {
		{ NULL, 0 },
		{ "invalid", EINVAL },
		{ "too small", ERANGE },
		{ "too large", ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval) {
		error = INVALID;
	} else {
		ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0') {
			error = INVALID;
		} else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval) {
			error = TOOSMALL;
		} else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval) {
			error = TOOLARGE;
		}
	}
	if (errstrp != NULL) {
		*errstrp = ev[error].errstr;
	}

	errno = ev[error].err;
	if (error) {
		ll = 0;
	}

	return ll;
}
#endif /* HAVE_STRTONUM */

int
xasprintf(char **ret, char *fmt, ...)
{
	int i;
	va_list ap;

	va_start(ap, fmt);
	i = xvasprintf(ret, fmt, ap);
	va_end(ap);

	return i;
}

void
xexec(char *argstr)
{
	char *args[20], **ap = args;
	char **end = &args[18], *tmp;

	while (ap < end && (*ap = strsep(&argstr, " \t")) != NULL) {
		if (**ap == '\0') {
			continue;
		}

		ap++;
		if (argstr != NULL) {
			/* deal with quoted strings */
			switch(argstr[0]) {
				case '"':
				case '\'':
					if ((tmp = strchr(argstr + 1, argstr[0])) != NULL) {
						*(tmp++) = '\0';
						*(ap++) = ++argstr;
						argstr = tmp;
					}
					break;
				default:
					break;
			}
		}
	}
	*ap = NULL;

	(void)setsid();
	(void)execvp(args[0], args);
}

void
xspawn(char *cmd)
{
	switch (fork()) {
		case 0:
			xexec(cmd);
			exit(1);
		case -1:
			fprintf(stderr, "could not fork\n");
			break;
		default:
			break;
	}
}

int
xvasprintf(char **ret, char *fmt, va_list ap)
{
	int i;

	i = vasprintf(ret, fmt, ap);
	if (i == -1) {
		fprintf(stderr, "vasprintf");
	}

	return i;
}
