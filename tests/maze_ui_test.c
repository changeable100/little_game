/* TC-201/202/203/205/206/209/210: host LVGL, simulated data only. */
#include "maze_ui.h"
#include "maze_levels.h"
#include "maze_menu.h"
#include "lvgl.h"
/* Zig's optimized builds define NDEBUG. These are executable acceptance
 * checks (some create objects), so they must remain enabled on every host. */
#ifdef NDEBUG
#undef NDEBUG
#endif
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
static void chinese_glyphs(lv_obj_t *object)
{
    if(lv_obj_check_type(object,&lv_label_class)) {
        const unsigned char *s=(const unsigned char *)lv_label_get_text(object);
        const lv_font_t *font=lv_obj_get_style_text_font(object,0);
        while(*s) {
            uint32_t cp=*s++;unsigned extra=0;
            if(cp>=0xf0){cp&=7;extra=3;}else if(cp>=0xe0){cp&=15;extra=2;}else if(cp>=0xc0){cp&=31;extra=1;}
            while(extra--){assert((*s&0xc0)==0x80);cp=(cp<<6)|(*s++&63);}
            if(cp=='\n')continue;
            assert(!(cp>='A'&&cp<='Z') && !(cp>='a'&&cp<='z'));
            lv_font_glyph_dsc_t glyph;
            assert(lv_font_get_glyph_dsc(font,&glyph,cp,0) && !glyph.is_placeholder);
            if(cp>=0x4e00 && cp<=0x9fff)assert(glyph.box_w && glyph.box_h);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(object);++i)chinese_glyphs(lv_obj_get_child(object,i));
}
static void screenshot(const char *name)
{
    chinese_glyphs(lv_screen_active());chinese_glyphs(lv_layer_top());
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
static maze_result_t sink(void *ctx, const maze_action_t *a);
static unsigned invalidations;
static bool outside_ball;
static void invalidated(lv_event_t *event)
{
    const lv_area_t *a = lv_event_get_param(event);
    ++invalidations;
    /* Test movement is from (1.5,1.5) to (1.6,1.5): old/new 8px ball. */
    if (a->x1 < 60 || a->x2 > 71 || a->y1 < 86 || a->y2 > 95) outside_ball=true;
}
static void partial_refresh_test(lv_display_t *d, lv_obj_t *top,
                                 const maze_snapshot_t *initial, const maze_level_t *level)
{
    maze_snapshot_t frame=*initial;
    frame.state=MAZE_RUNNING; frame.input_status=MAZE_INPUT_VALID;
    assert(maze_ui_create(sink, NULL)==MAZE_UI_SUCCESS);
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
    maze_ui_render(&frame,level); lv_obj_update_layout(top); lv_refr_now(d);
    invalidations=0;
    for(unsigned i=0;i<100;++i) {
        ++frame.revision; frame.elapsed_ms=i*5;
        maze_ui_render(&frame,level); lv_obj_update_layout(top);
    }
    assert(invalidations==0); /* Same visible state must not request a redraw. */
    frame.ball_x=1.6f; outside_ball=false;
    maze_ui_render(&frame,level); lv_obj_update_layout(top);
    assert(invalidations>0 && !outside_ball);
    lv_refr_now(d); invalidations=0;
    frame.elapsed_ms=1000; /* Same revision still changes visible text. */
    maze_ui_render(&frame,level);
    assert(text(top,"进行中｜1秒") && invalidations>0);
    uint8_t cells[256]; memcpy(cells,level->cells,sizeof(cells));
    maze_level_t map=*level; map.cells=cells;
    maze_ui_render(&frame,&map); lv_refr_now(d); invalidations=0;
    cells[5]=cells[5]==MAZE_CELL_WALL?MAZE_CELL_PATH:MAZE_CELL_WALL;
    maze_ui_render(&frame,&map);
    assert(invalidations>0); /* Same address/revision, changed map bytes. */
    lv_obj_t *tile=lv_obj_get_child(lv_obj_get_child(top,-1),7);
    assert(lv_color_eq(lv_obj_get_style_bg_color(tile,0),lv_color_hex(
        cells[5]==MAZE_CELL_WALL?0x263D55:0x101B2B)));
    cells[5]=99; maze_ui_render(&frame,&map);
    assert(text(top,"游戏数据异常"));
    maze_ui_render(&frame,level); assert(text(top,"进行中｜1秒"));
    maze_ui_destroy();
    lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);
    puts("TC-209 stationary-no-invalidation, ball-only region, same-revision/map mutation and recovery PASS");
}
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
        assert(maze_menu_create_preview() == MAZE_UI_SUCCESS);
        assert(maze_menu_create_preview() == MAZE_UI_ERR_EXISTS);
        assert(text(screen, "体感迷宫"));
        assert(!text(screen, "Peripheral Lab"));
        assert(!text(screen, "RGB light"));
        assert(text(screen, "预览"));
        lv_obj_update_layout(screen);
        lv_obj_t *enter = lv_obj_get_parent(text(screen, "进入游戏"));
        lv_obj_t *instructions = lv_obj_get_parent(text(screen, "玩法说明"));
        assert(lv_obj_get_x(enter) == 44 && lv_obj_get_y(enter) == 330);
        assert(lv_obj_get_width(enter) == 302 && lv_obj_get_height(enter) == 48);
        assert(lv_obj_get_y(instructions) == 388);
        unsigned menu_count = objects(screen);
        if (round == 0) screenshot("game-home.ppm");
        lv_obj_send_event(instructions, LV_EVENT_CLICKED, NULL);
        assert(text(screen, "玩法说明"));
        assert(text(screen, "当前为模拟预览，\n未启用体感控制。"));
        if (round == 0) screenshot("game-help.ppm");
        lv_obj_send_event(lv_obj_get_parent(text(screen, "返回首页")), LV_EVENT_CLICKED, NULL);
        assert(!text(screen, "返回首页"));
        assert(objects(screen) == menu_count);
        lv_obj_send_event(enter, LV_EVENT_CLICKED, NULL);
        assert(text(top, "模拟预览"));
        if (round == 0) screenshot("game-maze.ppm");
        lv_obj_send_event(lv_obj_get_parent(text(top, "返回")), LV_EVENT_CLICKED, NULL);
        assert(objects(top) == base && text(screen, "体感迷宫"));
        assert(objects(screen) == menu_count && timers() == clock_count);
        /* Also destroy while the maze or nested help page is still open. */
        lv_obj_send_event(enter, LV_EVENT_CLICKED, NULL);
        maze_menu_destroy(); maze_menu_destroy();
        assert(objects(screen) == screen_base && objects(top) == base);
        assert(timers() == clock_count);
        assert(maze_menu_create_preview() == MAZE_UI_SUCCESS);
        lv_obj_send_event(lv_obj_get_parent(text(screen, "玩法说明")), LV_EVENT_CLICKED, NULL);
        maze_menu_destroy();
        assert(objects(screen) == screen_base && objects(top) == base);
    }
    puts("Standalone home/help/maze navigation: 20 cycles PASS; no Lab page, no extra timers");
    assert(maze_ui_create(sink, NULL) == MAZE_UI_SUCCESS);
    maze_ui_destroy();
    assert(objects(top) == base && sink_calls == 0);
    const maze_level_t *level = maze_levels_default();
    maze_snapshot_t s = {.revision=1, .level_id=level->id, .state=MAZE_READY,
                          .ball_x=1.5f, .ball_y=1.5f, .ball_radius=0.2f};
    for (unsigned round = 0; round < 20; ++round) {
        maze_ui_preview_open();
        assert(maze_ui_create(NULL, NULL) == MAZE_UI_ERR_EXISTS);
        assert(text(top, "模拟预览"));
        if (!text(top, "待开始｜0秒")) {
            lv_obj_t *p = lv_obj_get_child(top, -1);
            fprintf(stderr, "Actual status: %s\n", lv_label_get_text(lv_obj_get_child(p, 1)));
        }
        assert(text(top, "待开始｜0秒"));
        unsigned count = objects(top);
        for (unsigned i = 0; i < 100; ++i) {
            maze_snapshot_t repeated = s;
            repeated.state = (maze_state_t)(i % 6);
            maze_ui_render(&repeated, level);
        }
        maze_ui_render(&s, level);
        assert(objects(top) == count && timers() == clock_count);
        lv_obj_update_layout(top);
        lv_obj_t *page = lv_obj_get_child(top, -1);
        lv_obj_t *title = text(top, "迷宫");
        assert(lv_obj_get_x(title) == 40 && lv_obj_get_y(title) == 12);
        lv_obj_t *ball = lv_obj_get_child(page, 260);
        assert(lv_obj_get_x(ball) == 61 && lv_obj_get_y(ball) == 87);
        assert(lv_obj_get_width(ball) == 8 && lv_obj_get_height(ball) == 8);
        lv_obj_t *first = lv_obj_get_child(page, 2);
        lv_obj_t *last = lv_obj_get_child(page, 257);
        assert(lv_obj_get_x(first) == 35 && lv_obj_get_y(first) == 61);
        assert(lv_obj_get_x(last) == 335 && lv_obj_get_y(last) == 361);
        const char *names[] = {"开始", "暂停", "重开", "返回"};
        for (unsigned i = 0; i < 4; ++i) {
            lv_obj_t *button = lv_obj_get_parent(text(top, names[i]));
            assert(lv_obj_get_width(button) == 78 && lv_obj_get_height(button) == 40);
            assert(lv_obj_get_x(button) == 28 + (int)i * 84);
            assert(lv_obj_get_y(button) == 400);
            assert(lv_obj_has_state(button, LV_STATE_DISABLED) == (i < 3));
        }
        if (round == 0) screenshot("maze-preview.ppm");
        lv_tick_inc(2000); lv_timer_handler();
        assert(text(top, "待开始｜0秒"));
        maze_ui_render(NULL, level);
        assert(text(top, "游戏数据异常"));
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
        bad = s; bad.state = (maze_state_t)99;
        maze_ui_render(&bad, level);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        maze_level_t invalid = *level; invalid.cell_count--;
        maze_ui_render(&s, &invalid);
        assert(lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        maze_ui_render(&s, level);
        assert(!lv_obj_has_flag(ball, LV_OBJ_FLAG_HIDDEN));
        lv_obj_send_event(lv_obj_get_parent(text(top, "返回")), LV_EVENT_CLICKED, NULL);
        maze_ui_destroy(); maze_ui_destroy();
        maze_ui_render(&s, level);
        assert(objects(top) == base && timers() == clock_count);
    }
    assert(maze_ui_create(NULL, NULL) == MAZE_UI_SUCCESS);
    const char *states[] = {"待开始", "准备中", "进行中", "已暂停", "已通关", "发生错误"};
    const char *state_files[] = {"READY", "CALIBRATING", "RUNNING", "PAUSED", "WON", "ERROR"};
    unsigned six_count = objects(top);
    for (unsigned state = 0; state < 6; ++state) {
        maze_snapshot_t frame = s;
        frame.state = (maze_state_t)state;
        frame.input_status = state < 2 ? MAZE_INPUT_NOT_READY : MAZE_INPUT_VALID;
        frame.elapsed_ms = state < 2 ? 20 : 12345;
        frame.ball_x = state < 2 ? 1.5f : (state == MAZE_WON ? 14.5f : 4.5f);
        frame.ball_y = state == MAZE_WON ? 13.5f : 1.5f;
        frame.error_code = state == MAZE_ERROR ? MAZE_ERROR_INPUT : MAZE_ERROR_NONE;
        maze_ui_render(&frame, level);
        if(state==MAZE_CALIBRATING || state==MAZE_ERROR) {
            lv_obj_t *overlay=lv_obj_get_child(lv_obj_get_child(top,-1),-1);
            assert(!lv_obj_has_flag(overlay,LV_OBJ_FLAG_HIDDEN));
            assert(text(overlay,"模拟预览"));
        }
        char expected[80], filename[80];
        snprintf(expected, sizeof(expected), "%s｜%lu秒", states[state], (unsigned long)(frame.elapsed_ms / 1000));
        assert(text(top, expected));
        assert(text(top, state == MAZE_PAUSED ? "继续" : "暂停"));
        if (state == MAZE_ERROR) assert(text(top, "输入异常，请重新加载或返回"));
        lv_obj_update_layout(top);
        lv_obj_t *b = lv_obj_get_child(lv_obj_get_child(top, -1), 260);
        assert(lv_obj_get_x(b) == 35 + (int)(frame.ball_x * 20) - 4);
        assert(lv_obj_get_y(b) == 61 + (int)(frame.ball_y * 20) - 4);
        assert(!lv_obj_has_flag(b, LV_OBJ_FLAG_HIDDEN));
        snprintf(filename, sizeof(filename), "state-%s.ppm", state_files[state]);
        screenshot(filename);
        /* Same revision, borrowed stack value mutated after render. No UI clock. */
        frame.elapsed_ms = 99000;
        lv_tick_inc(3000); lv_timer_handler();
        assert(text(top, expected));
        assert(objects(top) == six_count && timers() == clock_count);
    }
    maze_snapshot_t extreme = s;
    extreme.state = MAZE_RUNNING; extreme.elapsed_ms = UINT32_MAX;
    maze_ui_render(&extreme, level);
    assert(text(top, "进行中｜4294967秒"));
    const char *causes[] = {"错误原因未知，请返回首页", "关卡异常，请返回首页",
                            "输入异常，请重新加载或返回", "运行超时，请重新加载或返回"};
    for (unsigned cause = 0; cause < 4; ++cause) {
        extreme = s; extreme.state = MAZE_ERROR; extreme.error_code = (maze_error_t)cause;
        maze_ui_render(&extreme, cause == MAZE_ERROR_LEVEL ? NULL : level);
        assert(text(top, causes[cause]));
    }
    extreme = s; extreme.state = (maze_state_t)-1;
    maze_ui_render(&extreme, level); assert(text(top, "游戏数据异常"));
    extreme = s; extreme.input_status = (maze_input_status_t)99;
    maze_ui_render(&extreme, level); assert(text(top, "游戏数据异常"));
    extreme = s; extreme.error_code = (maze_error_t)99;
    maze_ui_render(&extreme, level); assert(text(top, "游戏数据异常"));
    extreme = s; extreme.ball_radius = INFINITY;
    maze_ui_render(&extreme, level); assert(text(top, "游戏数据异常"));
    uint8_t malformed[256]; memcpy(malformed, level->cells, sizeof(malformed));
    maze_level_t invalid_level = *level; invalid_level.cells = malformed;
    malformed[5] = 99;
    maze_ui_render(&s, &invalid_level); assert(text(top, "游戏数据异常"));
    maze_ui_render(&s, level);
    screenshot("state-recovered.ppm");
    assert(text(top, "待开始｜0秒"));
    maze_ui_destroy();
    assert(objects(top) == base && timers() == clock_count);
    partial_refresh_test(d,top,&s,level);
    assert(objects(top) == base && timers() == clock_count);
    maze_ui_preview_open(); /* New instance with initial revision must render. */
    assert(text(top, "待开始｜0秒")); maze_ui_destroy();
    puts("TC-201/203/206 six simulated states, error reasons, enums, UINT32 time, borrowed input, recovery PASS");
    assert(sink_calls == 0);
    puts("TC-214 all product labels Chinese; actual glyphs present for visible and hidden labels PASS");
    puts("TC-201 host layout PASS; screenshot maze-preview.ppm");
    puts("TC-202/210 preview disabled controls PASS; production actions NOT TESTED");
    puts("TC-203 six synthetic states/time PASS; production runtime UI binding NOT TESTED");
    puts("TC-205/209 20 cycles, 2000 renders, object/timer baseline PASS; physical heap NOT TESTED");
    puts("TC-206 static invalid inputs PASS; stale/dynamic coverage NOT TESTED");
    return 0;
}
