/* TC-201/202/203/205/206/209/210: host LVGL, simulated data only. */
#include "maze_ui.h"
#include "maze_levels.h"
#include "maze_menu.h"
#include "lvgl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t pixels[390 * 450 * 4], buffer[390 * 450 * 4];
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p)
{
    for (int y = a->y1; y <= a->y2; ++y) {
        memcpy(pixels + (y * 390 + a->x1) * 4, p, (a->x2-a->x1+1)*4);
        p += (a->x2-a->x1+1)*4;
    }
    lv_display_flush_ready(d);
}
static unsigned objects(lv_obj_t *o)
{
    unsigned n = 1;
    for (unsigned i = 0; i < lv_obj_get_child_count(o); ++i)
        n += objects(lv_obj_get_child(o, i));
    return n;
}
static unsigned timers(void)
{
    unsigned n = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++n;
    return n;
}
static lv_obj_t *text(lv_obj_t *o, const char *s)
{
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), s)) return o;
    for (unsigned i = 0; i < lv_obj_get_child_count(o); ++i) {
        lv_obj_t *found = text(lv_obj_get_child(o, i), s);
        if (found) return found;
    }
    return NULL;
}
static void screenshot(const char *name)
{
    lv_tick_inc(20);
    lv_timer_handler();
    lv_refr_now(NULL);
    FILE *f = fopen(name, "wb");
    assert(f);
    fprintf(f, "P6\n390 450\n255\n");
    for (unsigned i = 0; i < sizeof(pixels); i += 4) {
        fputc(pixels[i+2], f); fputc(pixels[i+1], f); fputc(pixels[i], f);
    }
    fclose(f);
}
static unsigned sink_calls;
static maze_result_t sink(void *ctx, const maze_action_t *a)
{
    (void)ctx; (void)a; ++sink_calls; return MAZE_OK;
}
int main(void)
{
    puts("SIMULATED DATA ONLY; no board, IMU or production model actions");
    lv_init();
    lv_display_t *d = lv_display_create(390, 450);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(d, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d, flush);
    lv_obj_t *top = lv_layer_top();
    unsigned base = objects(top), clock_count = timers();
    lv_obj_t *screen = lv_screen_active();
    unsigned screen_base = objects(screen);
    for (unsigned round = 0; round < 20; ++round) {
        assert(maze_menu_create() == MAZE_UI_SUCCESS);
        assert(maze_menu_create() == MAZE_UI_ERR_EXISTS);
        assert(text(screen, "TILT MAZE"));
        assert(!text(screen, "Peripheral Lab"));
        assert(!text(screen, "RGB light"));
        assert(text(screen, "PREVIEW"));
        lv_obj_update_layout(screen);
        lv_obj_t *enter = lv_obj_get_parent(text(screen, "ENTER MAZE"));
        lv_obj_t *instructions = lv_obj_get_parent(text(screen, "HOW TO PLAY"));
        assert(lv_obj_get_x(enter) == 44 && lv_obj_get_y(enter) == 330);
        assert(lv_obj_get_width(enter) == 302 && lv_obj_get_height(enter) == 48);
        assert(lv_obj_get_y(instructions) == 388);
        unsigned menu_count = objects(screen);
        if (round == 0) screenshot("game-home.ppm");
        lv_obj_send_event(instructions, LV_EVENT_CLICKED, NULL);
        assert(text(screen, "HOW TO PLAY"));
        assert(text(screen, "Preview only: motion controls\nare not enabled yet."));
        if (round == 0) screenshot("game-help.ppm");
        lv_obj_send_event(lv_obj_get_parent(text(screen, "BACK TO MENU")), LV_EVENT_CLICKED, NULL);
        assert(!text(screen, "BACK TO MENU"));
        assert(objects(screen) == menu_count);
        lv_obj_send_event(enter, LV_EVENT_CLICKED, NULL);
        assert(text(top, "PREVIEW / SIMULATED DATA"));
        if (round == 0) screenshot("game-maze.ppm");
        lv_obj_send_event(lv_obj_get_parent(text(top, "BACK")), LV_EVENT_CLICKED, NULL);
        assert(objects(top) == base && text(screen, "TILT MAZE"));
        assert(objects(screen) == menu_count && timers() == clock_count);
        /* Also destroy while the maze or nested help page is still open. */
        lv_obj_send_event(enter, LV_EVENT_CLICKED, NULL);
        maze_menu_destroy(); maze_menu_destroy();
        assert(objects(screen) == screen_base && objects(top) == base);
        assert(timers() == clock_count);
        assert(maze_menu_create() == MAZE_UI_SUCCESS);
        lv_obj_send_event(lv_obj_get_parent(text(screen, "HOW TO PLAY")), LV_EVENT_CLICKED, NULL);
        maze_menu_destroy();
        assert(objects(screen) == screen_base && objects(top) == base);
    }
    puts("Standalone home/help/maze navigation: 20 cycles PASS; no Lab page, no extra timers");
    assert(maze_ui_create(sink, NULL) == MAZE_UI_ERR_NOT_CONNECTED);
    assert(objects(top) == base && sink_calls == 0);
    const maze_level_t *level = maze_levels_default();
    maze_snapshot_t s = {.revision=1, .level_id=level->id, .state=MAZE_READY,
                          .ball_x=1.5f, .ball_y=1.5f, .ball_radius=0.2f};
    for (unsigned round = 0; round < 20; ++round) {
        maze_ui_preview_open();
        assert(maze_ui_create(NULL, NULL) == MAZE_UI_ERR_EXISTS);
        assert(text(top, "PREVIEW / SIMULATED DATA"));
        if (!text(top, "READY  |  0s")) {
            lv_obj_t *p = lv_obj_get_child(top, -1);
            fprintf(stderr, "Actual status: %s\n", lv_label_get_text(lv_obj_get_child(p, 1)));
        }
        assert(text(top, "READY  |  0s"));
        unsigned count = objects(top);
        for (unsigned i = 0; i < 100; ++i) maze_ui_render(&s, level);
        assert(objects(top) == count && timers() == clock_count);
        lv_obj_update_layout(top);
        lv_obj_t *page = lv_obj_get_child(top, -1);
        lv_obj_t *title = text(top, "MAZE");
        assert(lv_obj_get_x(title) == 40 && lv_obj_get_y(title) == 12);
        lv_obj_t *ball = lv_obj_get_child(page, 260);
        assert(lv_obj_get_x(ball) == 61 && lv_obj_get_y(ball) == 87);
        assert(lv_obj_get_width(ball) == 8 && lv_obj_get_height(ball) == 8);
        lv_obj_t *first = lv_obj_get_child(page, 2);
        lv_obj_t *last = lv_obj_get_child(page, 257);
        assert(lv_obj_get_x(first) == 35 && lv_obj_get_y(first) == 61);
        assert(lv_obj_get_x(last) == 335 && lv_obj_get_y(last) == 361);
        const char *names[] = {"START", "PAUSE", "RESTART", "BACK"};
        for (unsigned i = 0; i < 4; ++i) {
            lv_obj_t *button = lv_obj_get_parent(text(top, names[i]));
            assert(lv_obj_get_width(button) == 78 && lv_obj_get_height(button) == 40);
            assert(lv_obj_get_x(button) == 28 + (int)i * 84);
            assert(lv_obj_get_y(button) == 400);
            assert(lv_obj_has_state(button, LV_STATE_DISABLED) == (i < 3));
        }
        if (round == 0) screenshot("maze-preview.ppm");
        lv_tick_inc(2000); lv_timer_handler();
        assert(text(top, "READY  |  0s"));
        maze_ui_render(NULL, level);
        assert(text(top, "INVALID / UNSUPPORTED PREVIEW"));
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        maze_ui_render(&s, NULL);
        maze_snapshot_t bad = s; bad.ball_x = NAN;
        maze_ui_render(&bad, level);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        bad = s; bad.level_id++;
        maze_ui_render(&bad, level);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        bad = s; bad.ball_x = 16.0f;
        maze_ui_render(&bad, level);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        bad = s; bad.state = MAZE_RUNNING;
        maze_ui_render(&bad, level);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        maze_level_t invalid = *level; invalid.cell_count--;
        maze_ui_render(&s, &invalid);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        maze_ui_render(&s, level);
        assert(!lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        lv_obj_send_event(lv_obj_get_parent(text(top, "BACK")), LV_EVENT_CLICKED, NULL);
        maze_ui_destroy(); maze_ui_destroy();
        maze_ui_render(&s, level);
        assert(objects(top) == base && timers() == clock_count);
    }
    assert(sink_calls == 0);
    puts("TC-201 host layout PASS; screenshot maze-preview.ppm");
    puts("TC-202/210 preview disabled controls PASS; production actions NOT TESTED");
    puts("TC-203 fixed READY/time PASS; dynamic states NOT TESTED");
    puts("TC-205/209 20 cycles, 2000 renders, object/timer baseline PASS; physical heap NOT TESTED");
    puts("TC-206 static invalid inputs PASS; stale/dynamic coverage NOT TESTED");
    return 0;
}
