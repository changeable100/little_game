#include "roll3_menu.h"
#include "roll3_records.h"
#include "roll3_runtime.h"
#include "roll3_font.h"
#include "lvgl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t frame[390*450*4],display_buffer[390*450*4];
static r3_records_t scores,scores2;static r3_records_status_t record_status=R3_RECORDS_SAVED;
static bool busy;static int open_result;static unsigned opens,closes,last_level;
bool roll3_runtime_busy(void){return busy;}
int roll3_page_open(void){++opens;return open_result;}
int roll3_page_open_level(unsigned level){assert(level==1||level==2);last_level=level;return roll3_page_open();}
void roll3_page_close(void){++closes;}
r3_records_status_t roll3_records_read_level(unsigned level,r3_records_t *out){assert(level==1||level==2);*out=level==1?scores:scores2;return record_status;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{for(int y=a->y1;y<=a->y2;++y){memcpy(frame+(y*390+a->x1)*4,p,(a->x2-a->x1+1)*4);p+=(a->x2-a->x1+1)*4;}lv_display_flush_ready(d);}
static unsigned objects(lv_obj_t *o){unsigned n=1;for(unsigned i=0;i<lv_obj_get_child_count(o);++i)n+=objects(lv_obj_get_child(o,i));return n;}
static unsigned timers(void){unsigned n=0;for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static lv_obj_t *find(lv_obj_t *o,const char *s)
{if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),s))return o;
 for(unsigned i=0;i<lv_obj_get_child_count(o);++i){lv_obj_t *v=find(lv_obj_get_child(o,i),s);if(v)return v;}return NULL;}
static void glyphs(lv_obj_t *o)
{
    if(o==lv_screen_active())lv_obj_update_layout(o);
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *t=lv_label_get_text(o);const lv_font_t *f=lv_obj_get_style_text_font(o,LV_PART_MAIN);
        while(*t){unsigned char c=(unsigned char)*t++;uint32_t cp=c;
            if(c>=0xe0){cp=(c&15)<<12;cp|=((unsigned char)*t++&63)<<6;cp|=(unsigned char)*t++&63;}
            else if(c>=0xc0){cp=(c&31)<<6;cp|=(unsigned char)*t++&63;}
            if(cp>=32){assert(cp<=126||cp==0x2605);lv_font_glyph_dsc_t d;assert(lv_font_get_glyph_dsc(f,&d,cp,0));assert(!d.is_placeholder);}}
        bool visible=true;for(lv_obj_t *p=o;p;p=lv_obj_get_parent(p))visible=visible&&!lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN);
        if(visible){lv_point_t extent;lv_text_get_size(&extent,lv_label_get_text(o),f,
            lv_obj_get_style_text_letter_space(o,0),0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
            assert(extent.x<=lv_obj_get_width(o));}
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);++i)glyphs(lv_obj_get_child(o,i));
}
static void screenshot(const char *name)
{
    lv_obj_update_layout(lv_screen_active());lv_refr_now(NULL);
    FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n390 450\n255\n");
    for(unsigned i=0;i<sizeof(frame);i+=4){fputc(frame[i+2],f);fputc(frame[i+1],f);fputc(frame[i],f);}fclose(f);
}
static void click(const char *caption)
{
    lv_obj_t *target = find(lv_screen_active(), caption);
    assert(target);
    lv_obj_send_event(lv_obj_get_parent(target), LV_EVENT_PRESSED, NULL);
    lv_obj_send_event(lv_obj_get_parent(target), LV_EVENT_CLICKED, NULL);
}

int main(void){
 setvbuf(stdout,NULL,_IONBF,0);lv_init();lv_display_t *d=lv_display_create(390,450);
 lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);lv_display_set_buffers(d,display_buffer,NULL,sizeof(display_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
 unsigned baseline=objects(lv_screen_active()),base_timers=timers();
 assert(roll3_menu_create(true)==0&&roll3_menu_create(true)==-1);
 glyphs(lv_screen_active());screenshot("GAME-HOME.ppm");
 click("TOP 10");glyphs(lv_screen_active());screenshot("GAME-EMPTY.ppm");assert(find(lv_screen_active(),"01   --:--.--    -"));
 click("PLAY");assert(opens==0);click("BACK");
 click("HOW TO");glyphs(lv_screen_active());screenshot("GAME-HELP.ppm");click("BACK");
 click("PLAY");glyphs(lv_screen_active());screenshot("GAME-SELECT.ppm");assert(find(lv_screen_active(),"SELECT LEVEL")&&opens==0);click("BACK");
 open_result=-104;click("PLAY");click("LEVEL 1");assert(opens==1&&find(lv_screen_active(),"CANNOT START (-104)"));screenshot("GAME-ERROR.ppm");
 open_result=0;click("LEVEL 1");assert(opens==2&&last_level==1);busy=true;click("PLAY");assert(opens==2);busy=false;
 click("PLAY");click("LEVEL 2");assert(opens==3&&last_level==2);
 r3_snapshot_t second={.version=1,.run=1,.state=R3_WON,.elapsed_ms=48000};assert(r3_records_observe_level(&scores2,&second,2));
 for(unsigned i=0;i<12;++i){r3_snapshot_t s={.version=1,.run=i+1,.state=R3_WON,.elapsed_ms=23000+i*1000,.falls=i};(void)r3_records_observe(&scores,&s);}
 click("TOP 10");assert(find(lv_screen_active(),"LEVEL 2 / TIME / FALLS"));assert(find(lv_screen_active(),"01   00:48.00   0"));screenshot("GAME-TOP10-LEVEL2.ppm");click("LEVEL 1");glyphs(lv_screen_active());screenshot("GAME-TOP10.ppm");assert(find(lv_screen_active(),"01   00:23.00   0"));
 record_status=R3_RECORDS_UNSAVED;lv_tick_inc(251);lv_timer_handler();assert(find(lv_screen_active(),"NOT SAVED - RETRYING"));screenshot("GAME-UNSAVED.ppm");
 record_status=R3_RECORDS_PENDING;lv_tick_inc(251);lv_timer_handler();assert(find(lv_screen_active(),"SAVING..."));
 record_status=R3_RECORDS_OFFLINE;lv_tick_inc(251);lv_timer_handler();assert(find(lv_screen_active(),"RECORDS UNAVAILABLE"));
 click("BACK");roll3_menu_destroy();assert(objects(lv_screen_active())==baseline&&timers()==base_timers);
 for(unsigned i=0;i<20;++i){assert(!roll3_menu_create(i%2==0));click("TOP 10");click("BACK");click("HOW TO");click("BACK");lv_obj_delete(lv_obj_get_child(lv_screen_active(),0));assert(objects(lv_screen_active())==baseline&&timers()==base_timers);}
 assert(!roll3_menu_create(false));unsigned old=opens;click("PLAY");assert(opens==old);roll3_menu_destroy();
 assert(closes==22);puts("TC-530 native SDK menu, help, empty/full board, real callbacks, error status, text widths and 20 reopen/delete cycles PASS");return 0;
}
