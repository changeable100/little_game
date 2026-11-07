#ifndef MAZE_UI_H
#define MAZE_UI_H

#include "maze_contract.h"

/* P0/P1: runtime request allocation exists; production UI binding is P2 work.
 * UI errors are separate from maze_result_t; never pass them to the model. */
enum maze_ui_result {
    MAZE_UI_SUCCESS = 0,
    MAZE_UI_ERR_EXISTS = -100,
    MAZE_UI_ERR_NOT_CONNECTED = -101,
    MAZE_UI_ERR_DISPLAY = -102
};

/* All calls belong to the LVGL/model owning thread.
 * P1 implements ONLY simulated rendering: sink must be NULL. Non-NULL
 * returns NOT_CONNECTED without creating objects or retaining context.
 * P2 will bind the existing runtime request allocator and snapshot callbacks;
 * sink/context must then outlive the UI, with detach before object deletion.
 * No game instance, clock, input sampling or timer is owned by this module. */
int maze_ui_create(maze_action_sink_t sink, void *context);

/* P1 renders six states of a valid 16x16 level, with distinct error reasons.
 * Unknown enums, invalid maps/coordinates show a safe message and hide ball.
 * Inputs are read only during this call; no caller pointers are retained.
 * Invalid data hides the ball. Repeated calls allocate no objects/timers. */
void maze_ui_render(const maze_snapshot_t *snapshot,
                    const maze_level_t *level);

/* Idempotent. A must stop/detach its refresh reference before calling this
 * in a future production integration. Only this module's objects are freed. */
void maze_ui_destroy(void);

/* Preview entry, supplies explicit fixed simulated data; no model actions.
 * BACK closes the preview, revealing the standalone game menu underneath.
 * Archived Lab host tests may also open it without running the game menu. */
void maze_ui_preview_open(void);

#endif
