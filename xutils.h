#ifndef __XUTILS_H__
#define __XUTILS_H__

#include <X11/Xlib.h>

struct state_t;

typedef struct geometry_t {
	int x;
	int y;
	unsigned int width;
	unsigned int height;
} geometry_t;

void x_animate(Display *, Window, geometry_t, geometry_t, double);
Bool x_contains_point(geometry_t, int, int);
int x_distance(geometry_t, int, int);
Bool x_get_pointer(Display *, Window, int *, int *);
int x_get_property(Display *, Window, Atom, Atom, long, unsigned char **);
void x_send_message(Display *, Window, Atom, Atom, Time);

/*
void x_ewmh_set_client_list(struct state_t *);
void x_ewmh_set_client_list_stacking(struct state_t *);
*/

#endif /* __XUTILS_H__ */
