#ifndef ROLL3_VIEW_H
#define ROLL3_VIEW_H
#include "roll3_contract.h"
/* UI-owner thread only. Standalone simulated view; no production sink or
 * runtime. Game controls remain disabled; local return only closes view.
 * Requires an initialized 390x450 LVGL display. create returns 0, or -100
 * already open, -102 display mismatch, -103 color/depth allocation failed.
 * Failure reason is a static UTF-8 string from error_text. No partial large
 * buffer is retained on failure. NULL/invalid render inputs show an error.
 * render borrows snapshot/world only for the call; caller keeps ownership.
 * Do not call concurrently or while the display is being destroyed. Local
 * return and external page deletion release the same buffers; destroy is
 * safe to repeat. Does not alter the active screen or run physical actions. */
int roll3_view_create(void);
/* Live callbacks belong to the UI thread. The close callback runs once. */
int roll3_view_create_live(void (*action)(r3_action_kind_t), void (*closed)(void));
void roll3_view_action_status(bool pending, r3_result_t result);
void roll3_view_render(const r3_snapshot_t *snapshot,const r3_world_t *world);
void roll3_view_destroy(void);
const char *roll3_view_error_text(void);
unsigned roll3_view_depth_rows(void);
#endif
