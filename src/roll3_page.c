#include "roll3_page.h"
#include "roll3_runtime.h"
#include "roll3_scene.h"
#include "roll3_view.h"
#include "lvgl.h"

static lv_timer_t *refresh_timer;
static uint32_t displayed_run, displayed_revision;
static bool have_frame;

static struct {
    uint32_t start, previous, frames, sample_age, draw, gap;
    bool active;
} timing;

static void record_frame(uint32_t now, uint32_t draw, uint32_t age)
{
    if (!timing.active) {
        timing.start = timing.previous = now;
        timing.frames = timing.sample_age = timing.draw = timing.gap = 0;
        timing.active = true;
    }
    uint32_t gap = now - timing.previous;
    timing.previous = now;
    ++timing.frames;
    if (age != UINT32_MAX && age > timing.sample_age) timing.sample_age = age;
    if (draw > timing.draw) timing.draw = draw;
    if (gap > timing.gap) timing.gap = gap;
    if (now - timing.start >= 1000u) {
        roll3_runtime_report_frame(timing.frames,timing.sample_age,timing.draw,timing.gap);
        timing.start = now;
        timing.frames = timing.sample_age = timing.draw = timing.gap = 0;
    }
}

static void closed(void)
{
    if (refresh_timer) lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
    have_frame = false;
    timing.active = false;
    roll3_runtime_close();
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    r3_snapshot_t snapshot;
    bool pending;
    r3_result_t result;
    uint32_t sample_age;
    if (!roll3_runtime_read(&snapshot, &pending, &result, &sample_age)) {
        roll3_view_destroy();
        return;
    }
    if (!have_frame || snapshot.revision != displayed_revision) {
        uint32_t start = lv_tick_get();
        roll3_view_render(&snapshot, r3_demo_world());
        if (snapshot.state == R3_RUNNING)
            record_frame(start, lv_tick_elaps(start), sample_age);
        else
            timing.active = false;
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
        roll3_runtime_report_open(result,0);
        return result;
    }
    refresh_timer = lv_timer_create(refresh, ROLL3_PAGE_REFRESH_MS, NULL);
    if (!refresh_timer) {
        roll3_view_destroy();
        roll3_runtime_report_open(-105,0);
        return -105;
    }
    roll3_runtime_report_open(0,roll3_view_depth_rows());
    refresh(NULL);
    return 0;
}

void roll3_page_close(void)
{
    if (refresh_timer) roll3_view_destroy();
}
