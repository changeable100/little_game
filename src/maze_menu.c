#include "maze_menu.h"
#include "maze_levels.h"
#include "maze_ui_runtime.h"
#include "maze_runtime.h"
#include "lvgl.h"
#include "maze_font.h"
#include <stddef.h>

#define BG 0x0C1220
#define PANEL 0x17283A
#define TEXT 0xEDF4FC
#define MUTED 0x97A8C0
#define MINT 0x86F5CA

static lv_obj_t *home, *help, *entry_feedback;
static bool preview_mode;

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

static lv_obj_t *button(lv_obj_t *parent, const char *s, int y, uint32_t color,
                    lv_event_cb_t cb)
{
    lv_obj_t *o = panel(parent, 44, y, 302, 48, color, 12);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_70, LV_STATE_PRESSED);
    lv_obj_t *label = text(o, s, 0, 0, 286, &maze_font_16,
                           color == MINT ? BG : TEXT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, NULL);
    return o;
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
    text(help, "玩法说明", 44, 42, 302, &maze_font_20, TEXT);
    static const char *titles[] = {"倾斜控制", "沿路前进", "到达终点"};
    static const char *details[] = {
        "轻轻倾斜板子，\n控制小球移动。",
        "沿着通道前进，\n小球不会穿过墙壁。",
        "到达金色终点，\n即可完成本关挑战。"
    };
    static const char *numbers[] = {"01", "02", "03"};
    for (unsigned i = 0; i < 3; ++i) {
        int y = 101 + (int)i * 73;
        text(help, numbers[i], 44, y, 36, &maze_font_20, MINT);
        text(help, titles[i], 90, y, 256, &maze_font_16, TEXT);
        text(help, details[i], 90, y + 24, 256, &maze_font_16, MUTED);
    }
    text(help, preview_mode ? "当前为模拟预览，\n未启用体感控制。" : "开始前请将板子放平，\n保持静止，等待准备完成。",
          44, 328, 302, &maze_font_16, 0xFFD67D);
    button(help, "返回首页", 380, PANEL, close_help);
}

static void enter_maze(lv_event_t *event)
{
    (void)event;
    if (help || !home) return;
    if(preview_mode){maze_ui_preview_open();return;}
    if(maze_runtime_init_status()!=MAZE_RUNTIME_READY) {
        lv_label_set_text(entry_feedback,maze_ui_runtime_startup_text());return;
    }
    int result=maze_ui_runtime_open();
    if(result!=MAZE_UI_SUCCESS)
        lv_label_set_text_fmt(entry_feedback,"游戏界面打开失败（%d）",result);
    else
        lv_label_set_text(entry_feedback,maze_ui_runtime_startup_text());
}

static void home_deleted(lv_event_t *event)
{
    (void)event;
    /* Even external parent deletion must not leave the preview orphaned. */
    (void)maze_ui_runtime_release();
    maze_ui_destroy();
    home = NULL; entry_feedback=NULL;
}

static int create_menu(bool preview)
{
    if (home) return MAZE_UI_ERR_EXISTS;
    if (!lv_display_get_default() ||
        lv_display_get_horizontal_resolution(NULL) != 390 ||
        lv_display_get_vertical_resolution(NULL) != 450)
        return MAZE_UI_ERR_DISPLAY;
    preview_mode=preview;
    home = panel(lv_screen_active(), 0, 0, 390, 450, BG, 0);
    lv_obj_add_flag(home, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(home, home_deleted, LV_EVENT_DELETE, NULL);
    text(home, "体感益智", 44, 30, 206, &maze_font_16, MINT);
    lv_obj_t *badge = panel(home, 264, 26, 82, 26, PANEL, 13);
    lv_obj_t *badge_text = text(badge, preview ? "预览" : (maze_runtime_init_status()==MAZE_RUNTIME_READY ? "已就绪" : "未就绪"), 0, 0, 82,
                                &maze_font_16, 0xFFD67D);
    lv_obj_set_style_text_align(badge_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(badge_text);
    text(home, "体感迷宫", 42, 65, 304, &maze_font_28, TEXT);
    text(home, "倾斜板子，带小球找到出口。", 44, 106, 302,
          &maze_font_16, MUTED);
    lv_obj_t *frame = panel(home, 36, 140, 176, 176, PANEL, 12);
    lv_obj_t *map = panel(frame, 8, 8, 160, 160, BG, 0);
    lv_obj_add_event_cb(map, thumbnail, LV_EVENT_DRAW_MAIN, NULL);
    text(home, "01", 232, 149, 110, &maze_font_28, MINT);
    text(home, "蛇形通道", 232, 197, 114, &maze_font_16, TEXT);
    text(home, "16×16格\n单关卡挑战", 232, 224, 114,
          &maze_font_16, MUTED);
    entry_feedback=text(home, preview ? "静态预览" : maze_ui_runtime_startup_text(), 232, 264, 114, &maze_font_16, 0xFFD67D);
    lv_obj_t *entry=button(home, "进入游戏", 330, MINT, enter_maze);
    lv_obj_set_style_opa(entry,LV_OPA_40,LV_STATE_DISABLED);
    if(!preview && maze_runtime_init_status()!=MAZE_RUNTIME_READY)
        lv_obj_add_state(entry,LV_STATE_DISABLED);
    button(home, "玩法说明", 388, PANEL, open_help);
    return MAZE_UI_SUCCESS;
}

int maze_menu_create(void) { return create_menu(false); }
int maze_menu_create_preview(void) { return create_menu(true); }

void maze_menu_destroy(void)
{
    (void)maze_ui_runtime_release();
    maze_ui_destroy();
    if (home) lv_obj_delete(home);
}
