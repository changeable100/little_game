#include "roll3_menu.h"
#include "roll3_records.h"
#include "roll3_page.h"
#include "roll3_runtime.h"
#include "roll3_font.h"
#include "lvgl.h"
#include <stdio.h>

#define BG 0x113448
#define INK 0x55356e
#define PAPER 0xe9cbe5
#define ROSE 0xc58bca
#define BLUE 0x66b9d6
#define WHITE 0xffece5
static lv_obj_t *home,*modal,*feedback,*best,*storage,*rows[10],*play,*rank_heading,*choice_info,*choices[2];
static lv_timer_t *timer;
static bool ready;
static unsigned selected_level=1,ranking_level=1;
static lv_obj_t *panel(lv_obj_t *parent,int x,int y,int w,int h,uint32_t paint,int border) {
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(paint),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(o,lv_color_hex(INK),0);lv_obj_set_style_border_width(o,border,0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);return o;
}
static lv_obj_t *label(lv_obj_t *parent,const char *s,int x,int y,int w,const lv_font_t *font,uint32_t paint) {
    lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(paint),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *s,int x,int y,int w,int h,lv_event_cb_t cb) {
    panel(parent,x+4,y+4,w,h,INK,0);
    lv_obj_t *o=panel(parent,x,y,w,h,PAPER,3);lv_obj_add_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(o,lv_color_hex(ROSE),LV_STATE_PRESSED);
    lv_obj_set_style_opa(o,LV_OPA_40,LV_STATE_DISABLED);
    lv_obj_t *caption=label(o,s,0,0,w-12,&roll3_font_20,INK);lv_obj_center(caption);
    lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,NULL);return o;
}
static void format_time(char *out,size_t n,uint32_t ms) {
    snprintf(out,n,"%02u:%02u.%02u",(unsigned)(ms/60000u),(unsigned)(ms/1000u%60u),(unsigned)(ms/10u%100u));
}
static void refresh(lv_timer_t *unused) {
    (void)unused;if(!home)return;
    r3_records_t t;r3_records_status_t status=roll3_records_read_level(storage?ranking_level:selected_level,&t);
    char time[24],line[64];
    if(t.count){format_time(time,sizeof(time),t.rows[0].elapsed_ms);snprintf(line,sizeof(line),"BEST %s",time);}
    else snprintf(line,sizeof(line),"BEST --:--.--");
    lv_label_set_text(best,line);
    if(rank_heading)lv_label_set_text_fmt(rank_heading,"LEVEL %u / TIME / FALLS",ranking_level);
    if(storage)lv_label_set_text(storage,status==R3_RECORDS_PENDING?"SAVING...":status==R3_RECORDS_UNSAVED?"NOT SAVED - RETRYING":status==R3_RECORDS_OFFLINE?"RECORDS UNAVAILABLE":"SAVED ON THIS DEVICE");
    for(unsigned i=0;i<10;++i)if(rows[i]){
        if(i<t.count){format_time(time,sizeof(time),t.rows[i].elapsed_ms);snprintf(line,sizeof(line),"%02u   %s   %u",i+1,time,(unsigned)t.rows[i].falls);}
        else snprintf(line,sizeof(line),"%02u   --:--.--    -",i+1);
        lv_label_set_text(rows[i],line);
    }
    if(ready&&!roll3_runtime_busy())lv_obj_remove_state(play,LV_STATE_DISABLED);
    else lv_obj_add_state(play,LV_STATE_DISABLED);
    for(unsigned i=0;i<2;++i)if(choices[i]){
        if(ready&&!roll3_runtime_busy())lv_obj_remove_state(choices[i],LV_STATE_DISABLED);
        else lv_obj_add_state(choices[i],LV_STATE_DISABLED);
    }
}
static void modal_deleted(lv_event_t *e) {
    (void)e;modal=storage=rank_heading=choice_info=NULL;choices[0]=choices[1]=NULL;for(unsigned i=0;i<10;++i)rows[i]=NULL;refresh(NULL);
}
static void back(lv_event_t *e) { (void)e;if(modal)lv_obj_delete(modal); }
static lv_obj_t *sheet(const char *title) {
    if(modal||!home)return NULL;
    modal=panel(home,0,0,390,450,BG,0);lv_obj_add_flag(modal,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modal,modal_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *p=panel(modal,16,14,358,418,PAPER,4);
    label(p,title,20,16,314,&roll3_font_28,INK);
    button(p,"BACK",20,350,306,44,back);return p;
}
static void rank_one(lv_event_t *e){(void)e;ranking_level=1;refresh(NULL);}
static void rank_two(lv_event_t *e){(void)e;ranking_level=2;refresh(NULL);}
static void top10(lv_event_t *e) {
    (void)e;lv_obj_t *p=sheet("TOP 10");if(!p)return;
    ranking_level=selected_level;
    button(p,"LEVEL 1",20,53,143,32,rank_one);button(p,"LEVEL 2",183,53,143,32,rank_two);
    rank_heading=label(p,"",20,94,314,&roll3_font_16,INK);
    for(unsigned i=0;i<10;++i){
        if(i%2==0)panel(p,16,117+(int)i*20,318,20,WHITE,0);
        rows[i]=label(p,"",24,118+(int)i*20,306,&roll3_font_16,INK);
    }
    storage=label(p,"",20,326,318,&roll3_font_16,INK);refresh(NULL);
}
static void help(lv_event_t *e) {
    (void)e;lv_obj_t *p=sheet("HOW TO");if(!p)return;
    label(p,"01  HOLD STILL",22,72,310,&roll3_font_20,INK);
    label(p,"Keep the board level\nwhile it gets ready.",22,101,306,&roll3_font_16,INK);
    label(p,"02  TILT TO ROLL",22,164,310,&roll3_font_20,INK);
    label(p,"Follow the track.\nYellow discs save progress.",22,193,306,&roll3_font_16,INK);
    label(p,"03  REACH GREEN",22,256,310,&roll3_font_20,INK);
    label(p,"Fastest 10 finishes win\na place on this device.",22,285,306,&roll3_font_16,INK);
}
static void choose(unsigned level){
    if(!home||!modal||!choice_info||!ready||roll3_runtime_busy())return;
    int result=roll3_page_open_level(level);
    if(result)lv_label_set_text_fmt(choice_info,"CANNOT START (%d)",result);
    else{selected_level=level;lv_label_set_text_fmt(feedback,"LEVEL %u / READY",level);lv_obj_delete(modal);}
}
static void choose_one(lv_event_t *e){(void)e;choose(1);}
static void choose_two(lv_event_t *e){(void)e;choose(2);}
static void start(lv_event_t *e){
    (void)e;if(!home||modal||!ready||roll3_runtime_busy())return;
    lv_obj_t *p=sheet("SELECT LEVEL");if(!p)return;
    choices[0]=button(p,"LEVEL 1",20,89,306,58,choose_one);
    label(p,"CLASSIC COURSE",28,158,290,&roll3_font_16,INK);
    choices[1]=button(p,"LEVEL 2",20,205,306,58,choose_two);
    label(p,"BLUE SWITCHBACK",28,274,290,&roll3_font_16,INK);
    choice_info=label(p,"CHOOSE A COURSE",24,317,304,&roll3_font_16,INK);refresh(NULL);
}
/* Small native checker-track illustration, not a second game or framebuffer. */
static void hero(lv_obj_t *parent) {
    lv_obj_t *art=panel(parent,28,91,334,163,BG,0);
    panel(art,48,103,82,42,0x32768c,0);panel(art,182,75,88,70,0x32768c,0);
    for(int y=0;y<4;++y)for(int x=0;x<10;++x){
        if(y<2&&x>2&&x<7)continue;
        panel(art,34+x*26,25+y*26,26,26,(x+y)%2?BLUE:WHITE,0);
    }
    panel(art,250,24,16,53,BG,0);
    lv_obj_t *shadow=panel(art,62,84,31,13,0x32768c,0);lv_obj_set_style_radius(shadow,LV_RADIUS_CIRCLE,0);
    lv_obj_t *ball=panel(art,60,64,30,30,ROSE,3);lv_obj_set_style_radius(ball,LV_RADIUS_CIRCLE,0);
    lv_obj_t *shine=panel(ball,6,5,7,7,WHITE,0);lv_obj_set_style_radius(shine,LV_RADIUS_CIRCLE,0);
    label(art,"01",189,80,60,&roll3_font_28,INK);
}
static void deleted(lv_event_t *e) {
    (void)e;if(timer)lv_timer_delete(timer);timer=NULL;
    roll3_page_close();home=feedback=best=play=NULL;
}
int roll3_menu_create(bool game_ready) {
    if(home)return -1;
    if(!lv_display_get_default()||lv_display_get_horizontal_resolution(NULL)!=390||lv_display_get_vertical_resolution(NULL)!=450)return -2;
    selected_level=ranking_level=1;ready=game_ready;home=panel(lv_screen_active(),0,0,390,450,BG,0);
    lv_obj_add_event_cb(home,deleted,LV_EVENT_DELETE,NULL);
    label(home,"MARBLE RUN",28,22,334,&roll3_font_28,PAPER);
    label(home,"ROLL. TURN. FINISH.",30,61,330,&roll3_font_16,BLUE);
    hero(home);
    play=button(home,"PLAY",28,269,330,52,start);
    button(home,"TOP 10",28,337,157,46,top10);
    button(home,"HOW TO",201,337,157,46,help);
    best=label(home,"",30,405,244,&roll3_font_16,PAPER);
    feedback=label(home,ready?"LEVEL 1 / READY":"GAME UNAVAILABLE",30,426,330,&roll3_font_16,BLUE);
    timer=lv_timer_create(refresh,250,NULL);
    if(!timer){lv_obj_delete(home);return -3;}
    refresh(NULL);return 0;
}
void roll3_menu_destroy(void) { if(home)lv_obj_delete(home); }
