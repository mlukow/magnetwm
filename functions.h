#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

struct state_t;

typedef enum direction_t {
	DIRECTION_UP = (1 << 0),
	DIRECTION_UP_THIRD = (1 << 1),
	DIRECTION_DOWN = (1 << 2),
	DIRECTION_DOWN_THIRD = (1 << 3),
	DIRECTION_LEFT = (1 << 4),
	DIRECTION_LEFT_THIRD = (1 << 5),
	DIRECTION_RIGHT = (1 << 6),
	DIRECTION_RIGHT_THIRD = (1 << 7),
} direction_t;

void function_group_cycle(struct state_t *, void *, long);
void function_menu_command(struct state_t *, void *, long);
void function_menu_exec(struct state_t *, void *, long);
void function_terminal(struct state_t *, void *, long);
void function_window_center(struct state_t *, void *, long);
void function_window_cycle(struct state_t *, void *, long);
void function_window_fullscreen(struct state_t *, void *, long);
void function_window_maximize(struct state_t *, void *, long);
void function_window_move(struct state_t *, void *, long);
void function_window_resize(struct state_t *, void *, long);
void function_window_restore(struct state_t *, void *, long);
void function_window_tile(struct state_t *, void *, long);
void function_wm_state(struct state_t *, void *, long);

#endif /* __FUNCTIONS_H__ */
