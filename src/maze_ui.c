#include "maze_ui.h"
#include "maze_levels.h"
#include "lvgl.h"
#include "maze_font.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
static lv_obj_t *prepare_panel, *prepare_title, *prepare_instruction;
static lv_obj_t *prepare_message, *prepare_feedback, *prepare_retry, *prepare_back;
static maze_action_sink_t action_sink;
static void *sink_context, *delete_context;
static void (*delete_handler)(void *);
static maze_snapshot_t displayed;
static bool have_snapshot, data_valid, exhausted;
static maze_result_t feedback;
static uintptr_t generation;
/* Cache values, never borrowed level/cell pointers. 0 is the initial wall. */
static uint8_t tile_colors[MAZE_MAX_ROWS * MAZE_MAX_COLS];
static const char *base_hint;

static void set_text(lv_obj_t *object, const char *text)
{
    if (strcmp(lv_label_get_text(object),text)) lv_label_set_text(object,text);
}

static void paint_map(const maze_level_t *level)
{
    static const uint32_t colors[]={0x263D55,0x101B2B,0x245B50,0x66512D};
    for(unsigned i=0;i<256;++i) {
        unsigned color=0;
        if(level) {
            color=level->cells[i]==MAZE_CELL_WALL?0:1;
            if(i==level->start_row*16u+level->start_col)color=2;
            if(i==level->goal_row*16u+level->goal_col)color=3;
        }
        if(tile_colors[i]!=color) {
            lv_obj_set_style_bg_color(tiles[i],lv_color_hex(colors[color]),0);
            tile_colors[i]=(uint8_t)color;
        }
    }
}

static void hide_markers(void)
{
    lv_obj_add_flag(ball,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(start_mark,LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goal_mark,LV_OBJ_FLAG_HIDDEN);
}

static void update_prepare(void)
{
    if(!prepare_panel)return;
    maze_state_t state=displayed.state;
    bool show=exhausted || !have_snapshot || !data_valid ||
        state==MAZE_CALIBRATING || state==MAZE_ERROR;
    if(!show) {lv_obj_add_flag(prepare_panel,LV_OBJ_FLAG_HIDDEN);return;}
    const char *title="游戏数据异常", *instruction="请返回首页";
    const char *message="当前数据不可用，请重新进入游戏";
    if(exhausted) {
        title="操作次数已达上限";instruction="请重启设备";
        message="当前无法继续操作";
    } else if(!have_snapshot) {
        title="等待游戏状态";instruction="请稍候";
        message="暂时无法读取状态，请稍后再试";
    } else if(data_valid && state==MAZE_CALIBRATING) {
        title="准备开始";instruction="请将板子放平";
        message="保持静止，正在准备游戏";
    } else if(state==MAZE_ERROR && displayed.error_code==MAZE_ERROR_INPUT) {
        title="准备未完成";instruction="请将板子放平";
        message="输入异常，请重新加载或返回首页";
    } else if(state==MAZE_ERROR && displayed.error_code==MAZE_ERROR_TIMING) {
        title="运行超时";instruction="请重新加载";
        message="游戏推进中断，可重新加载或返回首页";
    } else if(state==MAZE_ERROR && displayed.error_code==MAZE_ERROR_LEVEL) {
        title="关卡异常";message="关卡数据不可用，请返回首页";
    }
    set_text(prepare_title,title);set_text(prepare_instruction,instruction);
    set_text(prepare_message,message);
    bool retry=action_sink && have_snapshot && data_valid && !exhausted &&
        state==MAZE_ERROR && (displayed.error_code==MAZE_ERROR_INPUT || displayed.error_code==MAZE_ERROR_TIMING);
    if(retry)lv_obj_remove_state(prepare_retry,LV_STATE_DISABLED);
    else lv_obj_add_state(prepare_retry,LV_STATE_DISABLED);
    if(have_snapshot && state==MAZE_CALIBRATING)lv_obj_add_flag(prepare_retry,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(prepare_retry,LV_OBJ_FLAG_HIDDEN);
    if(lv_obj_has_state(controls[3],LV_STATE_DISABLED))lv_obj_add_state(prepare_back,LV_STATE_DISABLED);
    else lv_obj_remove_state(prepare_back,LV_STATE_DISABLED);
    lv_obj_remove_flag(prepare_panel,LV_OBJ_FLAG_HIDDEN);
}

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
    update_prepare();
}

static void show_feedback(void)
{
    const char *message = NULL;
    switch(feedback) {
    case MAZE_OK: break;
    case MAZE_INVALID: message="操作无效，请查看当前状态";break;
    case MAZE_WRONG_STATE: message="尚未就绪，请稍后再试";break;
    case MAZE_STALE: message="旧操作已取消，请重新操作";break;
    case MAZE_EXHAUSTED: message="操作次数已达上限，请重启设备";break;
    default: message="暂时无法读取状态，请稍后再试";break;
    }
    if(message) {
        const char *cause=NULL;
        if(have_snapshot && displayed.state==MAZE_ERROR) {
            if(displayed.error_code==MAZE_ERROR_INPUT)cause="输入异常";
            if(displayed.error_code==MAZE_ERROR_TIMING)cause="运行超时";
            if(displayed.error_code==MAZE_ERROR_LEVEL)cause="关卡异常";
        }
        if(cause) {
            char text[192];
            snprintf(text,sizeof(text),"%s：%s",cause,message);
            set_text(hint_label,text);
        }
        else set_text(hint_label,message);
    }
    else if(base_hint)set_text(hint_label,base_hint);
    if(prepare_feedback)set_text(prepare_feedback,message?lv_label_get_text(hint_label):"");
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
    if(target==prepare_retry)index=2;
    if(target==prepare_back)index=3;
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
    prepare_panel=prepare_title=prepare_instruction=prepare_message=prepare_feedback=NULL;
    prepare_retry=prepare_back=NULL;
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
    memset(tile_colors,0,sizeof(tile_colors));base_hint="等待游戏状态";
    page = box(lv_layer_top(), 0, 0, 390, 450, BG);
    lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE); /* absorb background taps */
    lv_obj_add_event_cb(page, deleted, LV_EVENT_DELETE, NULL);
    label(page, "迷宫", 40, 12, &maze_font_20, TEXT);
    status_label = label(page, "等待游戏状态", 119, 16,
                         &maze_font_16, MUTED);
    lv_obj_set_width(status_label, 255);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_CLIP);

    for (unsigned i = 0; i < MAZE_MAX_ROWS * MAZE_MAX_COLS; ++i)
        tiles[i] = box(page, MAP_X + (i % 16) * CELL,
                       MAP_Y + (i / 16) * CELL, CELL, CELL, 0x263D55);
    start_mark = label(page, "起", 0, 0, &maze_font_16, 0x86F5CA);
    goal_mark = label(page, "终", 0, 0, &maze_font_16, 0xFFD67D);
    ball = box(page, 0, 0, 8, 8, 0xFF969D);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(start_mark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(goal_mark, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *preview = label(page, sink ? "倾斜控制小球，到达终点" : "模拟预览", 0, 382,
                              &maze_font_16, 0xFFD67D);
    lv_obj_set_width(preview, 390);
    lv_obj_set_style_text_align(preview, LV_TEXT_ALIGN_CENTER, 0);
    static const char *names[] = {"开始", "暂停", "重开", "返回"};
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *button = box(page, 28 + i * 84, 400, 78, 40, 0x263D55);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(button, 8, 0);
        lv_obj_t *text = label(button, names[i], 0, 0,
                               &maze_font_16, TEXT);
        lv_obj_center(text);
        if (i == 1) pause_text = text;
        controls[i]=button;
        lv_obj_add_event_cb(button, action_event, LV_EVENT_CLICKED, (void *)generation);
        {
            lv_obj_add_state(button, LV_STATE_DISABLED);
            lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
        }
    }
    hint_label = label(page, "等待游戏状态", 35, 40,
                        &maze_font_16, MUTED);
    lv_obj_set_width(hint_label, 320);
    lv_label_set_long_mode(hint_label, LV_LABEL_LONG_CLIP);
    /* Prebuilt modal: no extra clock, allocation or page ownership per frame. */
    prepare_panel=box(page,0,0,390,450,BG);
    lv_obj_add_flag(prepare_panel,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *emblem=box(prepare_panel,129,36,132,40,0x17283A);
    lv_obj_set_style_radius(emblem,10,0);
    lv_obj_t *emblem_text=label(emblem,sink?"平放校准":"模拟预览",0,0,&maze_font_16,0x86F5CA);
    lv_obj_center(emblem_text);
    prepare_title=label(prepare_panel,"准备开始",44,98,&maze_font_28,TEXT);
    prepare_instruction=label(prepare_panel,"请将板子放平",44,157,&maze_font_20,TEXT);
    prepare_message=label(prepare_panel,"保持静止，正在准备游戏",44,202,&maze_font_16,MUTED);
    prepare_feedback=label(prepare_panel,"",44,246,&maze_font_16,0xFFD67D);
    lv_obj_t *centered[]={prepare_title,prepare_instruction,prepare_message,prepare_feedback};
    for(unsigned i=0;i<4;++i){
        lv_obj_set_width(centered[i],302);
        lv_obj_set_style_text_align(centered[i],LV_TEXT_ALIGN_CENTER,0);
    }
    lv_obj_set_height(prepare_feedback,40);
    lv_label_set_long_mode(prepare_feedback,LV_LABEL_LONG_CLIP);
    static const char *prepare_names[]={"重新加载","返回首页"};
    for(unsigned i=0;i<2;++i){
        lv_obj_t *b=box(prepare_panel,44,300+(int)i*60,302,48,i==0?0x245B50:0x263D55);
        lv_obj_set_style_radius(b,12,0);lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(b,LV_OPA_40,LV_STATE_DISABLED);
        lv_obj_t *t=label(b,prepare_names[i],0,0,&maze_font_16,TEXT);lv_obj_center(t);
        lv_obj_add_event_cb(b,action_event,LV_EVENT_CLICKED,(void *)generation);
        if(i==0)prepare_retry=b;else prepare_back=b;
    }
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
    set_text(pause_text, snapshot && snapshot->state == MAZE_PAUSED ? "继续" : "暂停");
    if (!snapshot || snapshot->state < MAZE_READY || snapshot->state > MAZE_ERROR ||
        snapshot->input_status < MAZE_INPUT_NOT_READY || snapshot->input_status > MAZE_INPUT_FAILED ||
        snapshot->error_code < MAZE_ERROR_NONE || snapshot->error_code > MAZE_ERROR_TIMING) {
        set_text(status_label, "游戏数据异常");
        base_hint="状态或数据异常，请返回首页";
        hide_markers();paint_map(NULL);update_controls();show_feedback();
        return;
    }
    static const char *states[] = {"待开始", "准备中", "进行中", "已暂停", "已通关", "发生错误"};
    static const char *hints[] = {"先放平板子，再点击开始", "请将板子放平，保持静止",
        "沿着通道前进，到达金色终点", "已暂停，小球和用时保持不变",
        "恭喜通关，已记录最终用时", "发生错误，请返回首页"};
    static const char *errors[] = {"错误原因未知，请返回首页", "关卡异常，请返回首页",
        "输入异常，请重新加载或返回", "运行超时，请重新加载或返回"};
    char status[64];
    snprintf(status,sizeof(status),"%s｜%lu秒", states[snapshot->state],
             (unsigned long)(snapshot->elapsed_ms / 1000u));
    base_hint=snapshot->error_code != MAZE_ERROR_NONE || snapshot->state == MAZE_ERROR
        ? errors[snapshot->error_code] : hints[snapshot->state];
    if (!valid(snapshot, level)) {
        set_text(status_label,snapshot->state==MAZE_ERROR?status:"游戏数据异常");
        base_hint=snapshot->state == MAZE_ERROR && snapshot->error_code == MAZE_ERROR_LEVEL
            ? errors[MAZE_ERROR_LEVEL] : "地图或小球数据异常，请返回";
        hide_markers();paint_map(NULL);update_controls();show_feedback();
        return;
    }
    set_text(status_label,status);
    paint_map(level);
    lv_obj_set_pos(start_mark, MAP_X + level->start_col * CELL + 2,
                   MAP_Y + level->start_row * CELL + 2);
    lv_obj_set_pos(goal_mark, MAP_X + level->goal_col * CELL + 2,
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
