#ifndef MAZE_UI_H
#define MAZE_UI_H

#include "maze_contract.h"

/* B2-0 draft: A review and request-id adapter are pending.
 * UI errors are separate from maze_result_t; never pass them to the model. */
enum maze_ui_result {
    MAZE_UI_SUCCESS = 0,
    MAZE_UI_ERR_EXISTS = -100,
    MAZE_UI_ERR_NOT_CONNECTED = -101,
    MAZE_UI_ERR_DISPLAY = -102
};

/* All calls belong to the LVGL/model owning thread.
 * B2-1 implements ONLY a simulated preview: sink must be NULL. Non-NULL
 * returns NOT_CONNECTED without creating objects or retaining context.
 * Later production integration requires A's monotonic request-id provider
 * and refreshed-snapshot delivery; sink/context must then outlive the UI.
 * No game instance, clock, input sampling or timer is owned by this module. */
int maze_ui_create(maze_action_sink_t sink, void *context);

/* B2-1 accepts a READY snapshot of a valid 16x16 level. Other states show
 * an explicit unsupported-preview message, not a simulated running game.
 * Inputs are read only during this call; no caller pointers are retained.
 * Invalid data hides the ball. Repeated calls allocate no objects/timers. */
void maze_ui_render(const maze_snapshot_t *snapshot,
                    const maze_level_t *level);

/* Idempotent. A must stop/detach its refresh reference before calling this
 * in a future production integration. Only this module's objects are freed. */
void maze_ui_destroy(void);

/* Lab-only entry, supplies explicit fixed simulated data; no model actions.
 * BACK closes this preview directly and reveals the existing Lab screen. */
void maze_ui_preview_open(void);

#endif
