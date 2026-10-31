#include "maze_menu.h"
#include "maze_levels.h"
#include "lvgl.h"
#include <stddef.h>

#define BG 0x0C1220
#define PANEL 0x17283A
#define TEXT 0xEDF4FC
#define MUTED 0x97A8C0
#define MINT 0x86F5CA

static lv_obj_t *home, *help;

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h,
                       uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *text(lv_obj_t *parent, const char *s, int x, int y,
                      int width, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, s);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, width);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    return o;
}

static void button(lv_obj_t *parent, const char *s, int y, uint32_t color,
                    lv_event_cb_t cb)
{
    lv_obj_t *o = panel(parent, 44, y, 302, 48, color, 12);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_t *label = text(o, s, 0, 0, 286, &lv_font_montserrat_16,
                           color == MINT ? BG : TEXT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, NULL);
}

/* Draw the actual shared level; no copied map and no extra pixel buffer. */
static void thumbnail(lv_event_t *event)
{
    const maze_level_t *level = maze_levels_default();
    lv_area_t bounds;
    lv_obj_get_coords(lv_event_get_target(event), &bounds);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.border_opa = LV_OPA_TRANSP;
    for (unsigned y = 0; y < level->rows; ++y) {
        for (unsigned x = 0; x < level->cols; ++x) {
            uint32_t color = level->cells[y * level->cols + x] == MAZE_CELL_WALL
                ? 0x2A465D : BG;
            if (x == level->start_col && y == level->start_row) color = MINT;
            if (x == level->goal_col && y == level->goal_row) color = 0xFFD67D;
            d.bg_color = lv_color_hex(color);
            lv_area_t cell = {bounds.x1 + (int)x * 10, bounds.y1 + (int)y * 10,
                              bounds.x1 + (int)x * 10 + 9,
                              bounds.y1 + (int)y * 10 + 9};
            lv_draw_rect(lv_event_get_layer(event), &d, &cell);
        }
    }
}

static void close_help(lv_event_t *event)
{
    (void)event;
    if (help) lv_obj_delete(help);
}

static void help_deleted(lv_event_t *event)
{
    (void)event;
    help = NULL;
}

static void open_help(lv_event_t *event)
{
    (void)event;
    if (help || !home) return;
    help = panel(home, 0, 0, 390, 450, BG, 0);
    lv_obj_add_flag(help, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(help, help_deleted, LV_EVENT_DELETE, NULL);
    text(help, "HOW TO PLAY", 44, 42, 302, &lv_font_montserrat_24, TEXT);
    static const char *titles[] = {"TILT TO STEER", "FOLLOW THE PATH", "FIND THE GOAL"};
    static const char *details[] = {
        "Gently tilt the board to guide\nthe ball.",
        "Stay in the corridors.\nThe walls stop the ball.",
        "Reach the gold goal tile\nto complete the maze."
    };
    static const char *numbers[] = {"01", "02", "03"};
    for (unsigned i = 0; i < 3; ++i) {
        int y = 101 + (int)i * 73;
        text(help, numbers[i], 44, y, 36, &lv_font_montserrat_20, MINT);
        text(help, titles[i], 90, y, 256, &lv_font_montserrat_16, TEXT);
        text(help, details[i], 90, y + 24, 256, &lv_font_montserrat_14, MUTED);
    }
    text(help, "Preview only: motion controls\nare not enabled yet.",
          44, 328, 302, &lv_font_montserrat_14, 0xFFD67D);
    button(help, "BACK TO MENU", 380, PANEL, close_help);
}

static void enter_maze(lv_event_t *event)
{
    (void)event;
    if (!help) maze_ui_preview_open();
}

static void home_deleted(lv_event_t *event)
{
    (void)event;
    /* Even external parent deletion must not leave the preview orphaned. */
    maze_ui_destroy();
    home = NULL;
}

int maze_menu_create(void)
{
    if (home) return MAZE_UI_ERR_EXISTS;
    if (!lv_display_get_default() ||
        lv_display_get_horizontal_resolution(NULL) != 390 ||
        lv_display_get_vertical_resolution(NULL) != 450)
        return MAZE_UI_ERR_DISPLAY;
    home = panel(lv_screen_active(), 0, 0, 390, 450, BG, 0);
    lv_obj_add_flag(home, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(home, home_deleted, LV_EVENT_DELETE, NULL);
    text(home, "MOTION / PUZZLE", 44, 30, 206, &lv_font_montserrat_12, MINT);
    lv_obj_t *badge = panel(home, 264, 26, 82, 26, PANEL, 13);
    lv_obj_t *badge_text = text(badge, "PREVIEW", 0, 0, 82,
                                &lv_font_montserrat_12, 0xFFD67D);
    lv_obj_set_style_text_align(badge_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(badge_text);
    text(home, "TILT MAZE", 42, 65, 304, &lv_font_montserrat_28, TEXT);
    text(home, "Find your way to the goal.", 44, 106, 302,
          &lv_font_montserrat_14, MUTED);
    lv_obj_t *frame = panel(home, 36, 140, 176, 176, PANEL, 12);
    lv_obj_t *map = panel(frame, 8, 8, 160, 160, BG, 0);
    lv_obj_add_event_cb(map, thumbnail, LV_EVENT_DRAW_MAIN, NULL);
    text(home, "01", 232, 149, 110, &lv_font_montserrat_28, MINT);
    text(home, "SNAKE PATH", 232, 197, 114, &lv_font_montserrat_12, TEXT);
    text(home, "16 x 16 grid\nOne fixed maze", 232, 224, 114,
          &lv_font_montserrat_12, MUTED);
    text(home, "Static preview", 232, 286, 114, &lv_font_montserrat_12, 0xFFD67D);
    button(home, "ENTER MAZE", 330, MINT, enter_maze);
    button(home, "HOW TO PLAY", 388, PANEL, open_help);
    return MAZE_UI_SUCCESS;
}

void maze_menu_destroy(void)
{
    maze_ui_destroy();
    if (home) lv_obj_delete(home);
}
