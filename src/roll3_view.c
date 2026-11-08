#include "roll3_view.h"
#include "roll3_renderer.h"
#include "roll3_font.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <stddef.h>
#include <stdio.h>

static lv_obj_t *page,*image_obj,*status,*detail,*primary,*secondary;
static uint16_t *buffers;
static lv_image_dsc_t image;
static const char *error_text="";
static char status_text[80],detail_text[240];

const char *roll3_view_error_text(void){return error_text;}
static lv_obj_t *label(lv_obj_t *p,const char *s,int x,int y,int width,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(p);lv_label_set_text_static(o,s);lv_obj_set_pos(o,x,y);
    lv_obj_set_width(o,width);lv_obj_set_style_text_font(o,font,0);
    lv_obj_set_style_text_color(o,lv_color_hex(0xedf4fc),0);return o;
}
static void deleted(lv_event_t *event)
{
    (void)event;
    /* Drop image decoder/cache references before releasing pixel memory. */
    lv_image_cache_drop(&image);
    if(image_obj)lv_image_set_src(image_obj,NULL);
    if(buffers)lv_free(buffers);
    buffers=NULL;page=image_obj=status=detail=primary=secondary=NULL;
    image.data=NULL;
}
void roll3_view_destroy(void){if(page)lv_obj_delete(page);}
static void back(lv_event_t *e){(void)e;roll3_view_destroy();}
int roll3_view_create(void)
{
    if(page){error_text="三维预览已经打开";return -100;}
    if(!lv_display_get_default()||lv_display_get_horizontal_resolution(NULL)!=390||
       lv_display_get_vertical_resolution(NULL)!=450){error_text="屏幕尺寸不支持";return -102;}
    buffers=lv_malloc(R3_VIEW_PIXELS*sizeof(uint16_t)*2);
    if(!buffers){error_text="三维画面内存不足，请返回";return -103;}
    page=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(page);
    lv_obj_set_size(page,390,450);lv_obj_set_style_bg_color(page,lv_color_hex(0x0c1220),0);
    lv_obj_set_style_bg_opa(page,LV_OPA_COVER,0);lv_obj_remove_flag(page,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(page,deleted,LV_EVENT_DELETE,NULL);
    image=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
        .w=R3_VIEW_W,.h=R3_VIEW_H,.stride=R3_VIEW_W*2},.data_size=R3_VIEW_PIXELS*2,.data=(const uint8_t *)buffers};
    /* Empty frame is safe even before the first valid snapshot. */
    roll3_renderer_render(buffers,buffers+R3_VIEW_PIXELS,NULL,NULL);
    image_obj=lv_image_create(page);lv_image_set_src(image_obj,&image);
    lv_image_set_pivot(image_obj,0,0);lv_image_set_scale(image_obj,512);lv_obj_set_pos(image_obj,35,40);
    status=label(page,"三维预览",40,10,310,&roll3_font_20);
    detail=label(page,"等待完整快照",40,335,310,&roll3_font_16);
    label(page,"主机预览：操作尚未接入",40,378,310,&roll3_font_16);
    for(unsigned i=0;i<3;++i){lv_obj_t *b=lv_button_create(page);lv_obj_remove_style_all(b);
        lv_obj_set_pos(b,28+i*113,405);lv_obj_set_size(b,108,35);
        lv_obj_set_style_bg_color(b,lv_color_hex(0x263d55),0);lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
        lv_obj_set_style_radius(b,8,0);lv_obj_set_style_opa(b,LV_OPA_40,LV_STATE_DISABLED);
        lv_obj_t *t=label(b,i==0?"开始":i==1?"重开":"返回",0,0,108,&roll3_font_16);
        lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(t);
        if(i==0)primary=t;
        if(i==1)secondary=t;
        if(i==2)lv_obj_add_event_cb(b,back,LV_EVENT_CLICKED,NULL);else lv_obj_add_state(b,LV_STATE_DISABLED);
    }
    error_text="";return 0;
}
void roll3_view_render(const r3_snapshot_t *s,const r3_world_t *world)
{
    if(!page)return;
    int result=roll3_renderer_render(buffers,buffers+R3_VIEW_PIXELS,s,world);
    lv_image_cache_drop(&image);lv_obj_invalidate(image_obj);
    if(result){lv_label_set_text_static(status,"三维数据无效");lv_label_set_text_static(detail,"请检查快照版本、姿态与关卡");return;}
    static const char *states[]={"准备开始","请放平校准","游玩中","已暂停","小球掉落","通关","输入异常"};
    static const char *messages[]={"点击开始后，保持板子平放","等待真实校准完成","倾斜板子，沿坡道前进",
        "暂停期间不累计时间","返回当前检查点，保留本局时间","用时已冻结，可重开或返回","输入无效，请重开或返回"};
    snprintf(status_text,sizeof(status_text),"%s  %lu秒",states[s->state],(unsigned long)(s->elapsed_ms/1000));
    snprintf(detail_text,sizeof(detail_text),"%s\n掉落%lu次  检查点%lu",messages[s->state],(unsigned long)s->falls,(unsigned long)s->checkpoint);
    lv_label_set_text_static(status,status_text);
    lv_label_set_text_static(detail,detail_text);
    lv_label_set_text_static(primary,s->state==R3_PAUSED?"继续":s->state==R3_RUNNING?"暂停":s->state==R3_FALLEN?"回检查点":"开始");
    lv_label_set_text_static(secondary,"重开");
}
