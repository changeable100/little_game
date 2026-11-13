#include "roll3_view.h"
#include "roll3_renderer.h"
#include "roll3_font.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define INK 0x55356e
#define PAPER 0xe9cbe5
#define ROSE 0xc58bca
typedef enum {MODAL_NONE,MODAL_READY,MODAL_CAL,MODAL_GO,MODAL_MENU,
              MODAL_FALL,MODAL_ERROR,MODAL_RESULT,MODAL_INVALID} modal_t;
typedef struct {r3_edge_aa_t edges;uint16_t depth[];} workspace_t;
_Static_assert(offsetof(workspace_t,depth)==sizeof(r3_edge_aa_t),"workspace padding");
static lv_obj_t *page,*image_obj,*clock_box,*clock_text,*menu_button;
static lv_obj_t *shade,*card,*title,*info,*score,*hint,*badge,*badge_text;
static lv_obj_t *buttons[3],*captions[3];
static uint16_t *frame_color;
static workspace_t *workspace;
static unsigned depth_rows;
static lv_image_dsc_t image;
static r3_camera_follow_t camera_follow;
static void (*send_action)(r3_action_kind_t);
static void (*on_closed)(void);
static bool valid_frame,have_snapshot,action_pending,menu_requested,ready_toast,result_active;
static r3_state_t view_state;
static r3_result_t last_result;
static modal_t modal=MODAL_NONE;
static uint32_t shown_run,shown_seconds,toast_started,result_started;
static unsigned result_progress=257;
static const char *error_text="";
static char time_text[24],score_text[32];
static bool pressed_valid[4];
static uint32_t pressed_run[4];

const char *roll3_view_error_text(void){return error_text;}
unsigned roll3_view_depth_rows(void){return depth_rows;}
static void visible(lv_obj_t *o,bool show) {
    bool hidden=lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN);
    if(show&&hidden)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
    if(!show&&!hidden)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void enable(lv_obj_t *o,bool yes) {
    bool disabled=lv_obj_has_state(o,LV_STATE_DISABLED);
    if(yes&&disabled)lv_obj_remove_state(o,LV_STATE_DISABLED);
    if(!yes&&!disabled)lv_obj_add_state(o,LV_STATE_DISABLED);
}
static void text(lv_obj_t *o,const char *s)
{if(strcmp(lv_label_get_text(o),s))lv_label_set_text_static(o,s);}
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_radius(o,radius,0);return o;
}
/* Rectangles only: 2px steps, opaque offset shadow, no rounded/blurred layer.
 * The custom fill uses text opacity so the native object's transparent fill
 * cannot claim a cover rectangle over the cut-away corners. */
static void stepped(lv_layer_t *layer,int x,int y,int w,int h,uint32_t color,lv_opa_t opacity) {
    if(!opacity||w<16||h<16)return;
    const int inset[]={6,4,2,0,2,4,6};
    const int top[]={0,2,4,6,h-6,h-4,h-2};
    const int high[]={2,2,2,h-12,2,2,2};
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.base.layer=layer;
    d.bg_color=lv_color_hex(color);d.bg_opa=opacity;d.radius=0;
    for(unsigned i=0;i<7;++i){
        lv_area_t a={x+inset[i],y+top[i],x+w-inset[i]-1,y+top[i]+high[i]-1};
        lv_draw_rect(layer,&d,&a);
    }
}
static void pixel_panel(lv_event_t *event) {
    lv_obj_t *o=lv_event_get_target(event);lv_area_t a;lv_obj_get_coords(o,&a);
    lv_layer_t *layer=lv_event_get_layer(event);lv_opa_t opacity=lv_obj_get_style_text_opa(o,0);
    int w=lv_area_get_width(&a)-4,h=lv_area_get_height(&a)-4;
    uint32_t fill=lv_color_to_u32(lv_obj_get_style_bg_color(o,0))&0xffffffu;
    stepped(layer,a.x1+4,a.y1+4,w,h,0x422657,opacity);
    stepped(layer,a.x1,a.y1,w,h,INK,opacity);
    stepped(layer,a.x1+2,a.y1+2,w-4,h-4,fill,opacity);
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.base.layer=layer;
    d.bg_color=lv_color_hex(lv_obj_has_state(o,LV_STATE_DISABLED)?0xcbb7d0:0xf4dcee);d.bg_opa=opacity;
    lv_area_t highlight={a.x1+8,a.y1+4,a.x1+w-9,a.y1+5};lv_draw_rect(layer,&d,&highlight);
}
static void pixel_style(lv_obj_t *o,uint32_t color) {
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
    lv_obj_set_style_radius(o,0,0);lv_obj_set_style_border_width(o,0,0);
    lv_obj_set_style_text_opa(o,LV_OPA_COVER,0);
    lv_obj_add_event_cb(o,pixel_panel,LV_EVENT_DRAW_MAIN,NULL);
}
static lv_obj_t *pixel_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *o=box(p,x,y,w,h,color,0);pixel_style(o,color);return o;
}
static lv_obj_t *label(lv_obj_t *p,const char *s,int x,int y,int w,const lv_font_t *font) {
    lv_obj_t *o=lv_label_create(p);lv_label_set_text_static(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(INK),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);return o;
}
static void controls(void) {
    if(!page)return;
    bool live=send_action&&valid_frame&&!action_pending;
    enable(menu_button,live&&view_state==R3_RUNNING&&!ready_toast&&!menu_requested);
    bool first=false,second=modal!=MODAL_NONE&&modal!=MODAL_GO,third=false;
    if(modal==MODAL_READY)first=live&&view_state==R3_READY;
    else if(modal==MODAL_ERROR)first=live&&view_state==R3_ERROR;
    else if(modal==MODAL_FALL)first=live&&view_state==R3_FALLEN;
    else if(modal==MODAL_MENU){first=second=live&&view_state==R3_PAUSED;third=true;}
    else if(modal==MODAL_RESULT)second=result_progress>=256;
    enable(buttons[0],first);enable(buttons[1],second);enable(buttons[2],third);
}
static void deleted(lv_event_t *e) {
    (void)e;lv_image_cache_drop(&image);if(image_obj)lv_image_set_src(image_obj,NULL);
    lv_free(workspace);lv_free(frame_color);workspace=NULL;frame_color=NULL;depth_rows=0;image.data=NULL;
    page=image_obj=clock_box=clock_text=menu_button=NULL;shade=card=title=info=score=hint=badge=badge_text=NULL;
    for(unsigned i=0;i<3;++i)buttons[i]=captions[i]=NULL;
    result_active=ready_toast=menu_requested=valid_frame=have_snapshot=false;modal=MODAL_NONE;
    memset(pressed_valid,0,sizeof(pressed_valid));
    void (*closed)(void)=on_closed;on_closed=NULL;send_action=NULL;if(closed)closed();
}
void roll3_view_destroy(void){if(page)lv_obj_delete(page);}

/* Avoid a transformed/partly opaque parent layer, which can allocate a large
 * temporary ARGB bitmap. Resize/slide and fade individual drawing properties. */
static void paint_popup(unsigned progress) {
    unsigned opacity=progress*255u/256u;
    int w=274+4*(int)(7u*progress/256u),h=272+4*(int)(7u*progress/256u);
    int y=102+2*(int)(24u*(256u-progress)/256u);
    lv_obj_set_pos(card,(390-w)/2,y);lv_obj_set_size(card,w,h);
    lv_obj_set_style_text_opa(card,(lv_opa_t)opacity,0);
    lv_obj_set_style_bg_opa(shade,(lv_opa_t)(80u*progress/256u),0);
    lv_obj_set_pos(badge,168,y-24);
    lv_obj_set_style_text_opa(badge,(lv_opa_t)opacity,0);
    lv_obj_t *labels[]={title,info,score,hint,badge_text,captions[0],captions[1],captions[2]};
    for(unsigned i=0;i<sizeof(labels)/sizeof(*labels);++i)lv_obj_set_style_text_opa(labels[i],(lv_opa_t)opacity,0);
    lv_obj_set_style_text_opa(hint,progress<256?0:LV_OPA_COVER,0);
    lv_obj_t *wide[]={title,info,score,hint};
    for(unsigned i=0;i<4;++i)lv_obj_set_width(wide[i],w-28);
    for(unsigned i=0;i<3;++i){
        lv_obj_set_width(buttons[i],w-56);lv_obj_set_width(captions[i],w-56);
        lv_obj_set_style_text_opa(buttons[i],(lv_opa_t)opacity,0);
    }
}
static void set_mode(modal_t wanted) {
    if(modal==wanted)return;
    memset(pressed_valid,0,sizeof(pressed_valid));
    modal=wanted;visible(shade,wanted!=MODAL_NONE);if(wanted==MODAL_NONE)return;
    paint_popup(256);
    for(unsigned i=0;i<3;++i){visible(buttons[i],true);lv_obj_set_y(buttons[i],128+(int)i*56);}
    visible(score,false);visible(hint,false);visible(badge,false);
    lv_obj_set_y(title,24);lv_obj_set_y(info,72);lv_obj_set_height(info,48);
    text(captions[0],"CALIBRATE");text(captions[1],"EXIT");text(captions[2],"EXIT");
    visible(buttons[2],wanted==MODAL_MENU);
    if(wanted==MODAL_READY){text(title,"READY?");text(info,"PLACE BOARD FLAT");}
    else if(wanted==MODAL_CAL){text(title,"CALIBRATING");text(info,"KEEP STILL");text(captions[0],"PLEASE WAIT");}
    else if(wanted==MODAL_GO){
        text(title,"READY!");text(info,"TILT TO ROLL");
        for(unsigned i=0;i<3;++i)visible(buttons[i],false);
        lv_obj_set_size(card,302,160);lv_obj_set_pos(card,44,146);lv_obj_set_style_bg_opa(shade,40,0);
    } else if(wanted==MODAL_MENU){
        text(title,"PAUSED");text(info,"BALL AND TIMER PAUSED");text(captions[0],"RESUME");text(captions[1],"CHECKPOINT");
    } else if(wanted==MODAL_FALL){text(title,"OOPS!");text(info,"BACK TO CHECKPOINT");text(captions[0],"CHECKPOINT");}
    else if(wanted==MODAL_ERROR){text(title,"INPUT ERROR");text(info,"PLACE BOARD FLAT");text(captions[0],"RECALIBRATE");}
    else if(wanted==MODAL_INVALID){text(title,"DATA ERROR");text(info,"EXIT AND TRY AGAIN");visible(buttons[0],false);}
    else if(wanted==MODAL_RESULT){
        text(title,"LEVEL CLEAR!");lv_obj_set_y(title,36);text(info,"YOUR TIME");lv_obj_set_y(info,80);lv_obj_set_height(info,26);
        visible(score,true);visible(hint,true);visible(badge,true);
        text(hint,"PLAY TO PICK LEVEL");text(captions[0],"");visible(buttons[0],false);text(captions[1],"EXIT");
        lv_obj_set_y(buttons[0],168);lv_obj_set_y(buttons[1],222);result_progress=257;
    }
    controls();
}
void roll3_view_tick(void) {
    if(!page)return;
    if(ready_toast&&lv_tick_elaps(toast_started)>=600u)ready_toast=false;
    modal_t wanted=MODAL_NONE;
    if(have_snapshot&&!valid_frame)wanted=MODAL_INVALID;
    else if(view_state==R3_WON)wanted=MODAL_RESULT;
    else if(view_state==R3_ERROR)wanted=MODAL_ERROR;
    else if(view_state==R3_CALIBRATING)wanted=MODAL_CAL;
    else if(view_state==R3_READY)wanted=MODAL_READY;
    else if(view_state==R3_FALLEN)wanted=MODAL_FALL;
    else if(view_state==R3_PAUSED||menu_requested)wanted=MODAL_MENU;
    else if(ready_toast)wanted=MODAL_GO;
    set_mode(wanted);visible(menu_button,wanted==MODAL_NONE&&view_state==R3_RUNNING);
    if(wanted==MODAL_MENU)text(info,view_state!=R3_PAUSED||action_pending?"PLEASE WAIT":last_result!=R3_OK?"TRY AGAIN":"BALL AND TIMER PAUSED");
    if(wanted==MODAL_RESULT){
        uint32_t elapsed=lv_tick_elaps(result_started);unsigned progress=256;
        if(elapsed<800u){float u=(float)elapsed/800.f,q=1-u;progress=(unsigned)((1-q*q*q)*256.f+.5f);}
        if(progress!=result_progress){paint_popup(progress);result_progress=progress;}
    }
    controls();
}
static unsigned control_index(lv_obj_t *o) {
    for(unsigned i=0;i<3;++i)if(o==buttons[i])return i;
    return o==menu_button?3:4;
}
static bool control_visible(lv_obj_t *o) {
    for(lv_obj_t *p=o;p;p=lv_obj_get_parent(p))if(lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN))return false;
    return true;
}
static void pressed(lv_event_t *event) {
    lv_obj_t *o=lv_event_get_target(event);unsigned i=control_index(o);
    if(i>=4||lv_obj_has_state(o,LV_STATE_DISABLED)||!control_visible(o))return;
    pressed_valid[i]=true;pressed_run[i]=shown_run;
}
static void clicked(lv_event_t *event) {
    lv_obj_t *o=lv_event_get_target(event);unsigned i=control_index(o);
    if(i>=4||lv_obj_has_state(o,LV_STATE_DISABLED)||!control_visible(o)||!pressed_valid[i]||pressed_run[i]!=shown_run)return;
    pressed_valid[i]=false;
    if(o==menu_button){
        if(!send_action||action_pending||view_state!=R3_RUNNING)return;
        menu_requested=true;roll3_view_tick();send_action(R3_PAUSE);return;
    }
    bool exit=(modal==MODAL_MENU?o==buttons[2]:o==buttons[1]);
    if(exit){roll3_view_destroy();return;}
    if(!send_action||!valid_frame||action_pending)return;
    if(modal==MODAL_MENU&&view_state==R3_PAUSED)send_action(o==buttons[0]?R3_RESUME:R3_RESPAWN);
    else if(modal==MODAL_READY&&o==buttons[0])send_action(R3_START);
    else if(modal==MODAL_ERROR&&o==buttons[0])send_action(R3_RESTART);
    else if(modal==MODAL_FALL&&o==buttons[0])send_action(R3_RESPAWN);
}
static int create(void (*action)(r3_action_kind_t),void (*closed)(void)) {
    if(page){error_text="GAME ALREADY OPEN";return -100;}
    if(!lv_display_get_default()||lv_display_get_horizontal_resolution(NULL)!=390||lv_display_get_vertical_resolution(NULL)!=450){
        error_text="UNSUPPORTED DISPLAY";return -102;
    }
    frame_color=lv_malloc(R3_VIEW_COLOR_BYTES);if(!frame_color){error_text="NOT ENOUGH MEMORY";return -103;}
    depth_rows=R3_VIEW_DEFAULT_DEPTH_ROWS;workspace=lv_malloc(R3_VIEW_WORKSPACE_BYTES(depth_rows));
    if(!workspace){depth_rows=R3_VIEW_LOW_DEPTH_ROWS;workspace=lv_malloc(R3_VIEW_WORKSPACE_BYTES(depth_rows));}
    if(!workspace){lv_free(frame_color);frame_color=NULL;depth_rows=0;error_text="NOT ENOUGH MEMORY";return -103;}
    send_action=action;on_closed=closed;valid_frame=have_snapshot=action_pending=false;
    ready_toast=result_active=menu_requested=false;view_state=R3_READY;last_result=R3_OK;
    shown_seconds=UINT32_MAX;shown_run=0;modal=MODAL_NONE;result_progress=257;memset(&camera_follow,0,sizeof(camera_follow));
    memset(pressed_valid,0,sizeof(pressed_valid));
    page=box(lv_layer_top(),0,0,390,450,0x512078,0);lv_obj_add_event_cb(page,deleted,LV_EVENT_DELETE,NULL);
    image=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
        .w=R3_VIEW_W,.h=R3_VIEW_H,.stride=R3_VIEW_W*2},.data_size=R3_VIEW_COLOR_BYTES,.data=(const uint8_t *)frame_color};
    roll3_renderer_render_edges(frame_color,workspace->depth,depth_rows,NULL,NULL,NULL,&workspace->edges);
    image_obj=lv_image_create(page);lv_image_set_src(image_obj,&image);lv_image_set_pivot(image_obj,0,0);
    lv_image_set_scale(image_obj,R3_VIEW_SCALE);lv_image_set_antialias(image_obj,true);
    lv_obj_set_pos(image_obj,(390-R3_VIEW_W*2)/2,(450-R3_VIEW_H*2)/2);
    clock_box=pixel_box(page,24,28,140,44,PAPER);
    lv_obj_t *clock=pixel_box(clock_box,8,8,28,28,ROSE);
    box(clock,10,6,2,8,INK,0);box(clock,10,12,8,2,INK,0);
    clock_text=label(clock_box,"00:00",36,8,96,&roll3_font_20);
    menu_button=lv_button_create(page);lv_obj_remove_style_all(menu_button);lv_obj_set_pos(menu_button,310,25);lv_obj_set_size(menu_button,56,56);
    lv_obj_set_y(menu_button,26);pixel_style(menu_button,PAPER);
    for(unsigned i=0;i<3;++i)box(menu_button,14,10+(int)i*6,24,2,INK,0);
    label(menu_button,"MENU",0,28,56,&roll3_font_16);
    lv_obj_add_event_cb(menu_button,pressed,LV_EVENT_PRESSED,NULL);lv_obj_add_event_cb(menu_button,clicked,LV_EVENT_CLICKED,NULL);
    shade=box(page,0,0,390,450,0x281b42,0);lv_obj_set_style_bg_opa(shade,80,0);
    card=pixel_box(shade,44,102,302,300,PAPER);
    title=label(card,"",14,24,274,&roll3_font_28);info=label(card,"",14,72,274,&roll3_font_20);
    score=label(card,"",14,112,274,&roll3_font_28);hint=label(card,"",14,270,274,&roll3_font_20);
    badge=pixel_box(shade,168,78,54,54,ROSE);
    badge_text=label(badge,"★",0,8,50,&roll3_font_28);lv_obj_add_flag(card,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    for(unsigned i=0;i<3;++i){
        buttons[i]=lv_button_create(card);lv_obj_remove_style_all(buttons[i]);lv_obj_set_pos(buttons[i],28,128+(int)i*56);lv_obj_set_size(buttons[i],246,44);
        pixel_style(buttons[i],i==0?0xcba2d7:i==1?ROSE:0xd7b0db);
        lv_obj_set_style_bg_color(buttons[i],lv_color_hex(0xc4afca),LV_STATE_DISABLED);
        captions[i]=label(buttons[i],"",0,8,246,&roll3_font_20);
        lv_obj_add_event_cb(buttons[i],pressed,LV_EVENT_PRESSED,NULL);lv_obj_add_event_cb(buttons[i],clicked,LV_EVENT_CLICKED,NULL);
    }
    error_text="";roll3_view_tick();return 0;
}
int roll3_view_create(void){return create(NULL,NULL);}
int roll3_view_create_live(void (*action)(r3_action_kind_t),void (*closed)(void))
{if(!action||!closed)return -101;return create(action,closed);}
void roll3_view_action_status(bool pending,r3_result_t result) {
    if(!page||!send_action)return;
    action_pending=pending;last_result=result;
    if(!pending&&result!=R3_OK&&view_state==R3_RUNNING)menu_requested=false;
    roll3_view_tick();
}
void roll3_view_render(const r3_snapshot_t *s,const r3_world_t *world) {
    if(!page)return;
    bool moving=s&&(s->state==R3_RUNNING||(s->state==R3_WON&&world&&world->finish_rolls));
    int result=moving?roll3_renderer_render_play(frame_color,workspace->depth,depth_rows,s,world,&camera_follow):
        roll3_renderer_render_edges(frame_color,workspace->depth,depth_rows,s,world,&camera_follow,&workspace->edges);
    lv_image_cache_drop(&image);lv_obj_invalidate(image_obj);
    r3_state_t previous=view_state;bool new_run=!have_snapshot||!s||s->run!=shown_run;
    have_snapshot=true;valid_frame=result==0;
    if(result){result_active=ready_toast=menu_requested=false;roll3_view_tick();return;}
    view_state=s->state;
    if(new_run){menu_requested=ready_toast=result_active=false;shown_seconds=UINT32_MAX;}
    if(previous==R3_PAUSED&&s->state==R3_RUNNING)menu_requested=false;
    if(previous==R3_CALIBRATING&&s->state==R3_RUNNING){ready_toast=true;toast_started=lv_tick_get();}
    if(s->state!=R3_RUNNING)ready_toast=false;
    if(s->state==R3_WON){
        if(!result_active){
            result_active=true;result_started=lv_tick_get();result_progress=257;
            snprintf(score_text,sizeof(score_text),"%02lu:%02lu.%02lu",(unsigned long)(s->elapsed_ms/60000u),
                (unsigned long)(s->elapsed_ms/1000u%60u),(unsigned long)(s->elapsed_ms%1000u/10u));
            lv_label_set_text_static(score,score_text);
        }
    } else result_active=false;
    shown_run=s->run;uint32_t seconds=s->elapsed_ms/1000u;
    if(seconds!=shown_seconds){
        shown_seconds=seconds;snprintf(time_text,sizeof(time_text),"%02lu:%02lu",(unsigned long)(seconds/60u),(unsigned long)(seconds%60u));
        lv_obj_set_style_text_font(clock_text,seconds/60u>=100?&roll3_font_16:&roll3_font_20,0);lv_label_set_text_static(clock_text,time_text);
    }
    roll3_view_tick();
}
