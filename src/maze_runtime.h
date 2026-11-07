#ifndef MAZE_RUNTIME_H
#define MAZE_RUNTIME_H

#include "maze_contract.h"

/* All APIs below must run on the main/LVGL owner thread. No I2C here.
 * render receives borrowed data for the duration of the call only.
 * Do not call detach/actions recursively from render. */
typedef void (*maze_render_t)(void *context, const maze_snapshot_t *snapshot,
                               const maze_level_t *level);
typedef enum
{
    MAZE_RUNTIME_NOT_INITIALIZED,
    MAZE_RUNTIME_READY,
    MAZE_RUNTIME_SELFTEST_FAILED,
    MAZE_RUNTIME_LEVEL_FAILED,
    MAZE_RUNTIME_QUEUE_FAILED,
    MAZE_RUNTIME_TIMER_FAILED
} maze_runtime_init_status_t;
/* Main/LVGL thread only, including before init. No resources or model changes.
 * READY means initialized, not calibrated; action/snapshot results still apply. */
maze_runtime_init_status_t maze_runtime_init_status(void);
maze_result_t maze_runtime_init(bool selftests_passed);
/* UI thread, only before first run and before attach. For menu-create failure,
 * not normal page navigation. Releases startup resources, returns to NOT_INITIALIZED. */
maze_result_t maze_runtime_cancel_startup(void);
maze_result_t maze_runtime_attach(maze_render_t render, void *context);
maze_result_t maze_runtime_detach(void);
maze_result_t maze_runtime_snapshot(maze_snapshot_t *out);
maze_result_t maze_runtime_next_request(uint32_t *id);
/* Compatible with maze_action_sink_t; context must be NULL. Always reread
 * snapshot after a call, including error returns (pause may settle time). */
maze_result_t maze_runtime_action(void *context, const maze_action_t *action);

#endif
