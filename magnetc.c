#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "xutils.h"

int
main(int argc, char **argv)
{
	char  buf[BUFSIZ], *host;
	int dn, fd, i, size, sn;
	struct sockaddr_un address;

	if (argc < 2) {
		fprintf(stderr, "No arguments given.\n");
		return EXIT_FAILURE;
	}

	if (!x_parse_display(NULL, &host, &dn, &sn)) {
		fprintf(stderr, "Could not parse display.\n");
		return EXIT_FAILURE;
	}

	address.sun_family = AF_UNIX;
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		fprintf(stderr, "Could not create socket.\n");
		return EXIT_FAILURE;
	}

	snprintf(address.sun_path, sizeof(address.sun_path), "/tmp/magnetwm%s_%i_%i-socket", host, dn, sn);
	free(host);

	if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
		fprintf(stderr, "Could not connect to socket.\n");
		close(fd);
		return EXIT_FAILURE;
	}

	for (i = 1; i < argc; i++) {
		if (send(fd, argv[i], strlen(argv[i]), 0) == -1) {
			fprintf(stderr, "Could not send data.\n");
			break;
		}

		if (((size = recv(fd, buf, BUFSIZ, 0)) <= 0) || strncmp(buf, "OK", size)) {
			fprintf(stderr, "Received invalid response.\n");
			break;
		}
	}

	close(fd);

	return EXIT_SUCCESS;
}
