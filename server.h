#ifndef __SERVER_H__
#define __SERVER_H__

typedef struct server_t {
	int fd;
} server_t;

struct state_t;

void server_free(server_t *);
server_t *server_init();
void server_process(struct state_t *, server_t *);

#endif /* __SERVER_H__ */
