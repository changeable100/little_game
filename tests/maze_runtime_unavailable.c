/* Pure UI suite ONLY: deliberately unavailable runtime, not connection proof. */
#include "maze_runtime.h"
maze_runtime_init_status_t maze_runtime_init_status(void){return MAZE_RUNTIME_NOT_INITIALIZED;}
maze_result_t maze_runtime_attach(maze_render_t r,void *c){(void)r;(void)c;return MAZE_WRONG_STATE;}
maze_result_t maze_runtime_detach(void){return MAZE_WRONG_STATE;}
maze_result_t maze_runtime_snapshot(maze_snapshot_t *s){(void)s;return MAZE_WRONG_STATE;}
maze_result_t maze_runtime_next_request(uint32_t *r){(void)r;return MAZE_WRONG_STATE;}
maze_result_t maze_runtime_action(void *c,const maze_action_t *a){(void)c;(void)a;return MAZE_WRONG_STATE;}
