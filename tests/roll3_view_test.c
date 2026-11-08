/* Real renderer, shared production world, real SDK LVGL; synthetic snapshots.
 * Never steps/calibrates a model or reads a device. */
#include "roll3_view.h"
#include "roll3_renderer.h"
#include "roll3_scene.h"
#include "roll3_font.h"
#include "lvgl.h"
#include "src/draw/lv_draw_buf_private.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint16_t color[R3_VIEW_PIXELS+2],depth[R3_VIEW_PIXELS+2],saved[R3_VIEW_PIXELS];
static uint8_t frame[390*450*4],display_buffer[390*450*4];
static unsigned buffer_live,buffer_attempts,draw_attempts;
static size_t largest_draw;
static bool fail_buffer;
static void *owned_buffer;
extern void *__real_lv_malloc(size_t);
extern void __real_lv_free(void *);
void *__wrap_lv_malloc(size_t n)
{
    if(n==R3_VIEW_PIXELS*4){++buffer_attempts;if(fail_buffer)return NULL;
        void *p=__real_lv_malloc(n);assert(!owned_buffer);owned_buffer=p;if(p)++buffer_live;return p;}
    return __real_lv_malloc(n);
}
void __wrap_lv_free(void *p){if(p&&p==owned_buffer){owned_buffer=NULL;--buffer_live;}__real_lv_free(p);}
static lv_draw_buf_malloc_cb_t original_draw;
static void *draw_alloc(size_t n,lv_color_format_t f)
{++draw_attempts;if(n>largest_draw)largest_draw=n;return original_draw(n,f);}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{for(int y=a->y1;y<=a->y2;++y){memcpy(frame+(y*390+a->x1)*4,p,(a->x2-a->x1+1)*4);p+=(a->x2-a->x1+1)*4;}lv_display_flush_ready(d);}
static unsigned objects(lv_obj_t *o){unsigned n=1;for(unsigned i=0;i<lv_obj_get_child_count(o);++i)n+=objects(lv_obj_get_child(o,i));return n;}
static unsigned timers(void){unsigned n=0;for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static lv_obj_t *find(lv_obj_t *o,const char *s)
{if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),s))return o;
 for(unsigned i=0;i<lv_obj_get_child_count(o);++i){lv_obj_t *v=find(lv_obj_get_child(o,i),s);if(v)return v;}return NULL;}
static void glyphs(lv_obj_t *o)
{
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *t=lv_label_get_text(o);const lv_font_t *f=lv_obj_get_style_text_font(o,LV_PART_MAIN);
        while(*t){unsigned char c=(unsigned char)*t++;uint32_t cp=c;
            if(c>=0xe0){cp=(c&15)<<12;cp|=((unsigned char)*t++&63)<<6;cp|=(unsigned char)*t++&63;}
            else if(c>=0xc0){cp=(c&31)<<6;cp|=(unsigned char)*t++&63;}
            if(cp>=32){lv_font_glyph_dsc_t d;assert(lv_font_get_glyph_dsc(f,&d,cp,0));assert(!d.is_placeholder);}}
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);++i)glyphs(lv_obj_get_child(o,i));
}
static void screenshot(const char *name)
{
    lv_obj_update_layout(lv_layer_top());lv_refr_now(NULL);
    FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n390 450\n255\n");
    for(unsigned i=0;i<sizeof(frame);i+=4){fputc(frame[i+2],f);fputc(frame[i+1],f);fputc(frame[i],f);}fclose(f);
}
static void math_report(const char *name,bool ok){printf("%s %s\n",name,ok?"PASS":"FAIL");}
static void render(const r3_snapshot_t *s,const r3_world_t *w)
{assert(roll3_renderer_render(color+1,depth+1,s,w)==0);assert(color[0]==0xabcd&&color[R3_VIEW_PIXELS+1]==0xabcd);
 assert(depth[0]==0xabcd&&depth[R3_VIEW_PIXELS+1]==0xabcd);}
static unsigned coverage(void){unsigned n=0;for(unsigned i=1;i<=R3_VIEW_PIXELS;++i)n+=depth[i]!=65535;return n;}
int main(void)
{
    puts("SYNTHETIC 3D VIEW ONLY: no physical input, no r3_step, no production page integration");
    color[0]=color[R3_VIEW_PIXELS+1]=depth[0]=depth[R3_VIEW_PIXELS+1]=0xabcd;
    assert(roll3_renderer_selftest(math_report)==0);
    const r3_world_t *world=r3_demo_world();
    r3_snapshot_t s={.version=ROLL3_CONTRACT_VERSION,.radius=.2f,.orientation={1,0,0,0},.position={3,0,1}};
    r3_box_t distant=world->boxes[0];distant.center=(r3_vec_t){30,20,0};
    r3_world_t lone=*world;lone.boxes=&distant;lone.box_count=1;
    render(&s,&lone);unsigned near_count=coverage();memcpy(saved,color+1,sizeof(saved));
    s.position.y=3;render(&s,&lone);assert(coverage()<near_count);
    s.position.y=0;s.orientation=(r3_quat_t){.92387953f,.38268343f,0,0};render(&s,&lone);
    assert(memcmp(saved,color+1,sizeof(saved))!=0);
    s.orientation=(r3_quat_t){1,0,0,0};render(&s,world);float x,y,z;
    assert(roll3_renderer_project(s.position,&x,&y,&z));unsigned center=(unsigned)y*R3_VIEW_W+(unsigned)x;
    uint16_t ball_pixel=color[1+center];
    r3_box_t boxes[R3_MAX_BOXES];memcpy(boxes,world->boxes,world->box_count*sizeof(*boxes));
    boxes[world->box_count]=distant;boxes[world->box_count].center=(r3_vec_t){3,-1.5f,1.2f};
    boxes[world->box_count].half=(r3_vec_t){.7f,.1f,1};boxes[world->box_count].color=0xff0000;
    r3_world_t occluded=*world;occluded.boxes=boxes;occluded.box_count++;
    render(&s,&occluded);assert(color[1+center]!=ball_pixel);assert((color[1+center]&31)==0);
    render(&s,world);memcpy(saved,color+1,sizeof(saved));
    memcpy(boxes,world->boxes,world->box_count*sizeof(*boxes));
    boxes[1].axis[0]=(r3_vec_t){1,0,0};boxes[1].axis[2]=(r3_vec_t){0,0,1};
    r3_world_t flat=*world;flat.boxes=boxes;render(&s,&flat);assert(memcmp(saved,color+1,sizeof(saved))!=0);
    r3_vec_t extremes[]={{3,-8,6},{3,-7.9f,6},{99,99,99},{3,-30,20},{-99,0,0}};
    for(unsigned i=0;i<sizeof(extremes)/sizeof(*extremes);++i){s.position=extremes[i];render(&s,world);}
    s.position.x=NAN;assert(roll3_renderer_render(color+1,depth+1,&s,world)<0);
    s.position=(r3_vec_t){3,0,1};s.state=(r3_state_t)-1;
    assert(roll3_renderer_render(color+1,depth+1,&s,world)<0);
    s.state=(r3_state_t)7;assert(roll3_renderer_render(color+1,depth+1,&s,world)<0);
    s.state=R3_READY;s.error=(r3_error_t)-1;
    assert(roll3_renderer_render(color+1,depth+1,&s,world)<0);
    s.error=R3_NO_ERROR;s.version=2;
    assert(roll3_renderer_render(color+1,depth+1,&s,world)<0);
    s.version=1;s.radius=0;assert(roll3_renderer_render(color+1,depth+1,&s,world)<0);
    s.radius=.2f;assert(roll3_renderer_render(color+1,depth+1,&s,NULL)<0);
    puts("TC-501 perspective size, quaternion stripes, OBB slope axes, occlusion, clipping/canaries PASS");

    lv_init();lv_display_t *d=lv_display_create(390,450);lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(d,display_buffer,NULL,sizeof(display_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
    original_draw=lv_draw_buf_get_handlers()->buf_malloc_cb;lv_draw_buf_get_handlers()->buf_malloc_cb=draw_alloc;
    unsigned base=objects(lv_layer_top()),clock_base=timers();fail_buffer=true;
    assert(roll3_view_create()==-103);assert(strstr(roll3_view_error_text(),"内存不足"));
    assert(buffer_live==0&&objects(lv_layer_top())==base);fail_buffer=false;
    const char *names[]={"READY","CALIBRATING","RUNNING","PAUSED","FALLEN","WON","ERROR"};
    for(unsigned round=0;round<20;++round){assert(roll3_view_create()==0);assert(roll3_view_create()==-100);
        unsigned count=objects(lv_layer_top()),allocations=buffer_attempts;
        for(unsigned st=0;st<7;++st){s=(r3_snapshot_t){.version=1,.radius=.2f,.orientation={1,0,0,0},
                .position=world->spawn,.state=(r3_state_t)st,.run=1,.revision=st,
                .elapsed_ms=st>=2?12345:0,.falls=st>=2?1:0,.checkpoint=st>=2?1:0};
            if(st>=2)s.position=(r3_vec_t){3.5f,0,.7f};
            if(st==R3_FALLEN)s.position.z=-1;if(st==R3_WON)s.position=(r3_vec_t){6,0,.7f};
            if(st==R3_ERROR)s.error=R3_INPUT_ERROR;
            s.orientation=(r3_quat_t){.92387953f,.38268343f,0,0};
            roll3_view_render(&s,world);glyphs(lv_layer_top());
            lv_obj_t *close=find(lv_layer_top(),"返回");assert(close);
            lv_obj_t *root=lv_obj_get_child(lv_layer_top(),-1);
            for(int i=4;i<=5;++i)assert(lv_obj_has_state(lv_obj_get_child(root,i),LV_STATE_DISABLED));
            if(round==0){char file[80];snprintf(file,sizeof(file),"roll3-%s.ppm",names[st]);screenshot(file);}
            assert(buffer_live==1&&buffer_attempts==allocations&&objects(lv_layer_top())==count&&timers()==clock_base);
        }
        s.orientation=(r3_quat_t){0,0,0,0};roll3_view_render(&s,world);assert(find(lv_layer_top(),"三维数据无效"));
        if(round%2)lv_obj_delete(lv_obj_get_child(lv_layer_top(),-1));else
            lv_obj_send_event(lv_obj_get_parent(find(lv_layer_top(),"返回")),LV_EVENT_CLICKED,NULL);
        roll3_view_destroy();roll3_view_destroy();assert(buffer_live==0&&objects(lv_layer_top())==base&&timers()==clock_base);
    }
    printf("TC-501 seven Chinese states/glyphs, controls, allocation failure, 20 cycles PASS; persistent buffers=115200 bytes\n");
    printf("LVGL draw-buffer malloc attempts=%u largest=%lu bytes (in addition to caller buffers)\n",draw_attempts,(unsigned long)largest_draw);
    return 0;
}
