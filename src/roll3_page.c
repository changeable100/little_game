#include "roll3_page.h"
#include "roll3_runtime.h"
#include "roll3_scene.h"
#include "roll3_view.h"
#include "lvgl.h"

static lv_timer_t *refresh_timer;
static uint32_t displayed_run, displayed_revision;
static bool have_frame;

static void closed(void)
{
    if (refresh_timer) lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
    have_frame = false;
    roll3_runtime_close();
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    r3_snapshot_t snapshot;
    bool pending;
    r3_result_t result;
    if (!roll3_runtime_read(&snapshot, &pending, &result)) {
        roll3_view_destroy();
        return;
    }
    if (!have_frame || snapshot.revision != displayed_revision) {
        roll3_view_render(&snapshot, r3_demo_world());
        displayed_run = snapshot.run;
        displayed_revision = snapshot.revision;
        have_frame = true;
    }
    roll3_view_action_status(pending, result);
}

static void submit(r3_action_kind_t action)
{
    if (!have_frame) return;
    bool accepted = roll3_runtime_submit(action, displayed_run);
    roll3_view_action_status(accepted, accepted ? R3_OK : R3_STALE);
}

int roll3_page_open(void)
{
    if (!roll3_runtime_open()) return -104;
    int result = roll3_view_create_live(submit, closed);
    if (result != 0) {
        roll3_runtime_close();
        return result;
    }
    refresh_timer = lv_timer_create(refresh, 67, NULL);
    if (!refresh_timer) {
        roll3_view_destroy();
        return -103;
    }
    refresh(NULL);
    return 0;
}

void roll3_page_close(void)
{
    if (refresh_timer) roll3_view_destroy();
}
