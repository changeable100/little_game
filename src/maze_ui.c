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

static void back(lv_event_t *event)
{
    (void)event;
    maze_ui_destroy();
}

/* Keep a safe page if its parent is externally deleted. */
static void deleted(lv_event_t *event)
{
    (void)event;
    page = NULL;
    status_label = ball = start_mark = goal_mark = NULL;
    for (unsigned i = 0; i < MAZE_MAX_ROWS * MAZE_MAX_COLS; ++i)
        tiles[i] = NULL;
}

int maze_ui_create(maze_action_sink_t sink, void *context)
{
    (void)context;
    if (page) return MAZE_UI_ERR_EXISTS;
    if (sink) return MAZE_UI_ERR_NOT_CONNECTED;
    if (!lv_display_get_default() ||
        lv_display_get_horizontal_resolution(NULL) != 390 ||
        lv_display_get_vertical_resolution(NULL) != 450)
        return MAZE_UI_ERR_DISPLAY;

    page = box(lv_layer_top(), 0, 0, 390, 450, BG);
    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE); /* absorb background taps */
    lv_obj_add_event_cb(page, deleted, LV_EVENT_DELETE, NULL);
    label(page, "MAZE", 16, 8, &lv_font_montserrat_20, TEXT);
    status_label = label(page, "NO SNAPSHOT", 119, 12,
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

    lv_obj_t *preview = label(page, "PREVIEW / SIMULATED DATA", 0, 382,
                              &lv_font_montserrat_12, 0xFFD67D);
    lv_obj_set_width(preview, 390);
    lv_obj_set_style_text_align(preview, LV_TEXT_ALIGN_CENTER, 0);
    static const char *names[] = {"START", "PAUSE", "RESTART", "BACK"};
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *button = box(page, 4 + i * 97, 400, 91, 46, 0x263D55);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(button, 8, 0);
        lv_obj_t *text = label(button, names[i], 0, 0,
                               &lv_font_montserrat_12, TEXT);
        lv_obj_center(text);
        if (i == 3) lv_obj_add_event_cb(button, back, LV_EVENT_CLICKED, NULL);
        else {
            lv_obj_add_state(button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
        }
    }
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
    lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(start_mark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goal_mark, LV_OBJ_FLAG_HIDDEN);
    if (!valid(snapshot, level) || snapshot->state != MAZE_READY ||
        snapshot->error_code != MAZE_ERROR_NONE) {
        lv_label_set_text(status_label, "INVALID / UNSUPPORTED PREVIEW");
        for (unsigned i = 0; i < 256; ++i)
            lv_obj_set_style_bg_color(tiles[i], lv_color_hex(0x263D55), 0);
        return;
    }
    lv_label_set_text_fmt(status_label, "READY  |  %lus",
                          (unsigned long)(snapshot->elapsed_ms / 1000u));
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
