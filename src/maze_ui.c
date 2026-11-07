#include "maze_ui.h"
#include "maze_levels.h"
#include "lvgl.h"
#include <math.h>
#include <stddef.h>

#define BG 0x0C1220
#define TEXT 0xEDF4FC
#define MUTED 0x97A8C0
#define CELL 20
#define MAP_X 35
#define MAP_Y 61

static lv_obj_t *page, *status_label, *ball;
static lv_obj_t *tiles[MAZE_MAX_ROWS * MAZE_MAX_COLS];
static lv_obj_t *start_mark, *goal_mark;
static lv_obj_t *hint_label, *pause_text, *controls[4];
static maze_action_sink_t action_sink;
static void *sink_context, *delete_context;
static void (*delete_handler)(void *);
static maze_snapshot_t displayed;
static bool have_snapshot, data_valid, exhausted;
static maze_result_t feedback;
static uintptr_t generation;

static void update_controls(void)
{
    for (unsigned i=0;i<4;++i) {
        bool enabled = !action_sink && i==3;
        if (action_sink && have_snapshot && !exhausted) {
            maze_state_t st=displayed.state;
            enabled = i==3 || (data_valid && ((i==0 && st==MAZE_READY) ||
                (i==1 && (st==MAZE_RUNNING || st==MAZE_PAUSED)) ||
                (i==2 && (st==MAZE_RUNNING || st==MAZE_PAUSED || st==MAZE_WON ||
                    (st==MAZE_ERROR && (displayed.error_code==MAZE_ERROR_INPUT || displayed.error_code==MAZE_ERROR_TIMING))))));
        }
        if(enabled)lv_obj_remove_state(controls[i],LV_STATE_DISABLED);
        else lv_obj_add_state(controls[i],LV_STATE_DISABLED);
    }
}

static void show_feedback(void)
{
    const char *message = NULL;
    switch(feedback) {
    case MAZE_OK: break;
    case MAZE_INVALID: message="Invalid action; see current state";break;
    case MAZE_WRONG_STATE: message="Not ready; see current state";break;
    case MAZE_STALE: message="Old intent discarded; not retried";break;
    case MAZE_EXHAUSTED: message="Requests exhausted; restart device";break;
    default: message="Snapshot unavailable; retry later";break;
    }
    if(message) {
        const char *cause=NULL;
        if(have_snapshot && displayed.state==MAZE_ERROR) {
            if(displayed.error_code==MAZE_ERROR_INPUT)cause="INPUT ERROR";
            if(displayed.error_code==MAZE_ERROR_TIMING)cause="TIMING ERROR";
            if(displayed.error_code==MAZE_ERROR_LEVEL)cause="LEVEL ERROR";
        }
        if(cause)lv_label_set_text_fmt(hint_label,"%s: %s",cause,message);
        else lv_label_set_text(hint_label,message);
    }
}

void maze_ui_feedback(maze_result_t result, bool snapshot_available)
{
    if(!page)return;
    feedback=result;
    if(!snapshot_available){have_snapshot=false;data_valid=false;}
    if(result==MAZE_EXHAUSTED)exhausted=true;
    update_controls();show_feedback();
}

void maze_ui_set_delete_handler(void (*handler)(void *), void *context)
{
    if(page){delete_handler=handler;delete_context=context;}
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h,
                     uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_pos(o, x, y);
    return o;
}

static void action_event(lv_event_t *event)
{
    if (!page || (uintptr_t)lv_event_get_user_data(event)!=generation) return;
    lv_obj_t *target=lv_event_get_target(event);
    unsigned index;
    for(index=0;index<4 && controls[index]!=target;++index){}
    if(index==4 || lv_obj_has_state(target,LV_STATE_DISABLED))return;
    if(!action_sink){if(index==3)maze_ui_destroy();return;}
    if(!have_snapshot || exhausted)return;
    maze_action_t intent={.expected_run_id=displayed.run_id,.request_id=0};
    intent.action=index==0?MAZE_ACTION_START:index==1?
        (displayed.state==MAZE_PAUSED?MAZE_ACTION_RESUME:MAZE_ACTION_PAUSE):
        index==2?MAZE_ACTION_RESTART:MAZE_ACTION_BACK;
    /* Sink can close this page. Never touch UI after the call. */
    (void)action_sink(sink_context,&intent);
}

/* Keep a safe page if its parent is externally deleted. */
static void deleted(lv_event_t *event)
{
    (void)event;
    void (*handler)(void *)=delete_handler;
    void *context=delete_context;
    delete_handler=NULL; delete_context=NULL;
    if(handler)handler(context); /* LVGL DELETE precedes object destruction. */
    action_sink=NULL; sink_context=NULL; have_snapshot=false;
    page = NULL;
    status_label = ball = start_mark = goal_mark = NULL;
    hint_label = pause_text = NULL;
    for (unsigned i = 0; i < MAZE_MAX_ROWS * MAZE_MAX_COLS; ++i)
        tiles[i] = NULL;
}

int maze_ui_create(maze_action_sink_t sink, void *context)
{
    if (page) return MAZE_UI_ERR_EXISTS;
    if (generation==UINTPTR_MAX) return MAZE_UI_ERR_NOT_CONNECTED;
    if (!lv_display_get_default() ||
        lv_display_get_horizontal_resolution(NULL) != 390 ||
        lv_display_get_vertical_resolution(NULL) != 450)
        return MAZE_UI_ERR_DISPLAY;

    ++generation; action_sink=sink; sink_context=context;
    have_snapshot=data_valid=exhausted=false;feedback=MAZE_OK;
    page = box(lv_layer_top(), 0, 0, 390, 450, BG);
    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE); /* absorb background taps */
    lv_obj_add_event_cb(page, deleted, LV_EVENT_DELETE, NULL);
    label(page, "MAZE", 40, 12, &lv_font_montserrat_20, TEXT);
    status_label = label(page, "NO SNAPSHOT", 119, 16,
                         &lv_font_montserrat_14, MUTED);
    lv_obj_set_width(status_label, 255);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_CLIP);

    for (unsigned i = 0; i < MAZE_MAX_ROWS * MAZE_MAX_COLS; ++i)
        tiles[i] = box(page, MAP_X + (i % 16) * CELL,
                       MAP_Y + (i / 16) * CELL, CELL, CELL, 0x263D55);
    start_mark = label(page, "S", 0, 0, &lv_font_montserrat_12, 0x86F5CA);
    goal_mark = label(page, "G", 0, 0, &lv_font_montserrat_12, 0xFFD67D);
    ball = box(page, 0, 0, 8, 8, 0xFF969D);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(start_mark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goal_mark, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *preview = label(page, sink ? "TILT TO MOVE / REACH THE GOAL" : "PREVIEW / SIMULATED DATA", 0, 382,
                              &lv_font_montserrat_12, 0xFFD67D);
    lv_obj_set_width(preview, 390);
    lv_obj_set_style_text_align(preview, LV_TEXT_ALIGN_CENTER, 0);
    static const char *names[] = {"START", "PAUSE", "RESTART", "BACK"};
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *button = box(page, 28 + i * 84, 400, 78, 40, 0x263D55);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(button, 8, 0);
        lv_obj_t *text = label(button, names[i], 0, 0,
                               &lv_font_montserrat_12, TEXT);
        lv_obj_center(text);
        if (i == 1) pause_text = text;
        controls[i]=button;
        lv_obj_add_event_cb(button, action_event, LV_EVENT_CLICKED, (void *)generation);
        {
            lv_obj_add_state(button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
        }
    }
    hint_label = label(page, "Waiting for snapshot", 35, 40,
                        &lv_font_montserrat_12, MUTED);
    lv_obj_set_width(hint_label, 320);
    lv_label_set_long_mode(hint_label, LV_LABEL_LONG_CLIP);
    update_controls();
    return MAZE_UI_SUCCESS;
}

static bool valid(const maze_snapshot_t *s, const maze_level_t *l)
{
    if (!s || !l || !l->cells || l->cols != 16 || l->rows != 16 ||
        l->cell_count != 256 || l->id != s->level_id ||
        l->start_col >= 16 || l->start_row >= 16 ||
        l->goal_col >= 16 || l->goal_row >= 16 ||
        (l->start_col == l->goal_col && l->start_row == l->goal_row))
        return false;
    for (unsigned i = 0; i < 256; ++i)
        if (l->cells[i] != MAZE_CELL_PATH && l->cells[i] != MAZE_CELL_WALL)
            return false;
    if (l->cells[l->start_row * 16 + l->start_col] != MAZE_CELL_PATH ||
        l->cells[l->goal_row * 16 + l->goal_col] != MAZE_CELL_PATH)
        return false;
    return isfinite(s->ball_x) && isfinite(s->ball_y) &&
        isfinite(s->ball_radius) && s->ball_radius > 0.0f &&
        s->ball_radius <= 0.5f && s->ball_radius * CELL >= 0.5f &&
        s->ball_x >= s->ball_radius && s->ball_y >= s->ball_radius &&
        s->ball_x <= 16.0f - s->ball_radius &&
        s->ball_y <= 16.0f - s->ball_radius;
}

void maze_ui_render(const maze_snapshot_t *snapshot, const maze_level_t *level)
{
    if (!page) return;
    data_valid=false;
    have_snapshot=snapshot!=NULL;
    if(snapshot)displayed=*snapshot;
    update_controls();
    lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(start_mark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goal_mark, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(pause_text, snapshot && snapshot->state == MAZE_PAUSED ? "RESUME" : "PAUSE");
    if (!snapshot || snapshot->state < MAZE_READY || snapshot->state > MAZE_ERROR ||
        snapshot->input_status < MAZE_INPUT_NOT_READY || snapshot->input_status > MAZE_INPUT_FAILED ||
        snapshot->error_code < MAZE_ERROR_NONE || snapshot->error_code > MAZE_ERROR_TIMING) {
        lv_label_set_text(status_label, "INVALID SNAPSHOT");
        lv_label_set_text(hint_label, "Unknown state or data; use BACK");
        for (unsigned i = 0; i < 256; ++i)
            lv_obj_set_style_bg_color(tiles[i], lv_color_hex(0x263D55), 0);
        return;
    }
    static const char *states[] = {"READY", "CALIBRATING", "RUNNING", "PAUSED", "WON", "ERROR"};
    static const char *hints[] = {"Ready to start", "Hold the board flat; calibrating",
        "Follow the path to the gold goal", "Paused; position and time held",
        "Goal reached; final time", "Error; use BACK"};
    static const char *errors[] = {"No error detail; use BACK", "LEVEL ERROR: return to menu",
        "INPUT ERROR: restart or back", "TIMING ERROR: restart or back"};
    lv_label_set_text_fmt(status_label, "%s  |  %lus", states[snapshot->state],
                          (unsigned long)(snapshot->elapsed_ms / 1000u));
    lv_label_set_text(hint_label, snapshot->error_code != MAZE_ERROR_NONE || snapshot->state == MAZE_ERROR
        ? errors[snapshot->error_code] : hints[snapshot->state]);
    if (!valid(snapshot, level)) {
        if (snapshot->state != MAZE_ERROR) lv_label_set_text(status_label, "INVALID SNAPSHOT");
        lv_label_set_text(hint_label, snapshot->state == MAZE_ERROR && snapshot->error_code == MAZE_ERROR_LEVEL
            ? errors[MAZE_ERROR_LEVEL] : "Invalid map or ball; use BACK");
        for (unsigned i = 0; i < 256; ++i)
            lv_obj_set_style_bg_color(tiles[i], lv_color_hex(0x263D55), 0);
        return;
    }
    for (unsigned i = 0; i < 256; ++i)
        lv_obj_set_style_bg_color(tiles[i], lv_color_hex(
            level->cells[i] == MAZE_CELL_WALL ? 0x263D55 : 0x101B2B), 0);
    unsigned start = level->start_row * 16 + level->start_col;
    unsigned goal = level->goal_row * 16 + level->goal_col;
    lv_obj_set_style_bg_color(tiles[start], lv_color_hex(0x245B50), 0);
    lv_obj_set_style_bg_color(tiles[goal], lv_color_hex(0x66512D), 0);
    lv_obj_set_pos(start_mark, MAP_X + level->start_col * CELL + 2,
                   MAP_Y + level->start_row * CELL + 2);
    lv_obj_set_pos(goal_mark, MAP_X + level->goal_col * CELL + 5,
                   MAP_Y + level->goal_row * CELL + 2);
    lv_obj_remove_flag(start_mark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(goal_mark, LV_OBJ_FLAG_HIDDEN);
    int diameter = (int)(snapshot->ball_radius * 2.0f * CELL + 0.5f);
    lv_obj_set_size(ball, diameter, diameter);
    lv_obj_set_pos(ball, MAP_X + (int)(snapshot->ball_x * CELL + 0.5f) - diameter / 2,
                   MAP_Y + (int)(snapshot->ball_y * CELL + 0.5f) - diameter / 2);
    lv_obj_remove_flag(ball, LV_OBJ_FLAG_HIDDEN);
    data_valid=true; update_controls();show_feedback();
}

void maze_ui_destroy(void)
{
    if (page) lv_obj_delete(page); /* deleted() clears all cached objects */
}

void maze_ui_preview_open(void)
{
    if (maze_ui_create(NULL, NULL) != MAZE_UI_SUCCESS) return;
    const maze_level_t *level = maze_levels_default();
    const maze_snapshot_t simulated = {
        .run_id = 0, .revision = 1, .level_id = level->id,
        .state = MAZE_READY, .input_status = MAZE_INPUT_NOT_READY,
        .error_code = MAZE_ERROR_NONE,
        .ball_x = level->start_col + 0.5f,
        .ball_y = level->start_row + 0.5f,
        .ball_radius = MAZE_BALL_RADIUS, .elapsed_ms = 0
    };
    maze_ui_render(&simulated, level);
}
