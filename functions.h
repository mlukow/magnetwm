#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

struct state_t;

void function_terminal(struct state_t *, void *, long);
void function_window_center(struct state_t *, void *, long);
void function_window_move(struct state_t *, void *, long);
void function_window_resize(struct state_t *, void *, long);
void function_window_tile(struct state_t *, void *, long);

#endif /* __FUNCTIONS_H__ */
