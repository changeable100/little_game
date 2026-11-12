/* Real renderer, shared production world, real SDK LVGL; synthetic snapshots.
 * Never steps/calibrates a model or reads a device. */
#include "roll3_view.h"
#include "roll3_renderer.h"
#include "roll3_bend.h"
#include "roll3_physics.h"
#include "roll3_camera_basis.h"
#include "roll3_scene.h"
#include "roll3_font.h"
#include "roll3_page.h"
#include "roll3_runtime.h"
#include "lvgl.h"
#include "src/draw/lv_draw_buf_private.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint16_t color[R3_VIEW_PIXELS+2],depth[R3_VIEW_PIXELS+2],saved[R3_VIEW_PIXELS],aa[R3_VIEW_PIXELS+2];
static uint8_t frame[390*450*4],display_buffer[390*450*4];
static unsigned buffer_live,buffer_attempts,draw_attempts;
static size_t largest_draw;
static bool fail_buffer,fail_timer;
static size_t buffer_budget=R3_VIEW_COLOR_BYTES+R3_VIEW_WORKSPACE_BYTES(R3_VIEW_DEFAULT_DEPTH_ROWS);
static size_t buffer_bytes;
static struct { void *ptr; size_t bytes; } owned[2];
void *roll3_test_malloc(size_t n)
{
    ++buffer_attempts;
    if(fail_buffer||n>R3_VIEW_COLOR_BYTES||n>buffer_budget-buffer_bytes)return NULL;
    void *p=lv_malloc(n);
    if(p){
        unsigned slot=owned[0].ptr?1:0;
        assert(!owned[slot].ptr);
        owned[slot].ptr=p;
        owned[slot].bytes=n;
        ++buffer_live;
        buffer_bytes+=n;
    }
    return p;
}
void roll3_test_free(void *p)
{
    for(unsigned i=0;i<2;++i){
        if(p&&p==owned[i].ptr){
            owned[i].ptr=NULL;
            --buffer_live;
            buffer_bytes-=owned[i].bytes;
        }
    }
    lv_free(p);
}
lv_timer_t *roll3_test_timer_create(lv_timer_cb_t cb,uint32_t period,void *data)
{
    return fail_timer?NULL:lv_timer_create(cb,period,data);
}

static r3_snapshot_t live_snapshot;
static bool live_open, live_pending;
static unsigned requests, closes;
static r3_action_kind_t last_action;

bool roll3_runtime_open(void)
{
    if (live_open) return false;
    live_snapshot = (r3_snapshot_t){.version = ROLL3_CONTRACT_VERSION, .radius = R3_RADIUS,
        .position = r3_demo_world()->spawn, .orientation = {1, 0, 0, 0}};
    live_pending = false;
    return live_open = true;
}
void roll3_runtime_close(void)
{
    assert(live_open);
    live_open = false;
    ++closes;
}
bool roll3_runtime_read(r3_snapshot_t *out, bool *pending, r3_result_t *result, uint32_t *sample_age_ms)
{
    if (!live_open) return false;
    *out = live_snapshot;
    if (sample_age_ms) *sample_age_ms = 0;
    *pending = live_pending;
    *result = R3_OK;
    return true;
}
void roll3_runtime_report_frame(uint32_t frames, uint32_t age, uint32_t draw, uint32_t gap)
{ (void)frames; (void)age; (void)draw; (void)gap; }

static int last_open_result;
static unsigned last_depth_rows;
void roll3_runtime_report_open(int result,unsigned depth_rows)
{ last_open_result=result;last_depth_rows=depth_rows; }

bool roll3_runtime_submit(r3_action_kind_t action, uint32_t run)
{
    assert(live_open && run == live_snapshot.run && !live_pending);
    last_action = action;
    ++requests;
    return live_pending = true;
}
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
static void click(const char *caption)
{
    lv_obj_t *target = find(lv_layer_top(), caption);
    assert(target);
    lv_obj_send_event(lv_obj_get_parent(target), LV_EVENT_CLICKED, NULL);
}
static void publish(r3_state_t state)
{
    live_snapshot.state = state;
    ++live_snapshot.revision;
    live_pending = false;
    lv_tick_inc(ROLL3_PAGE_REFRESH_MS);
    lv_timer_handler();
}
static void math_report(const char *name,bool ok){printf("%s %s\n",name,ok?"PASS":"FAIL");}
static r3_vec_t from_uv(float u,float v,float z)
{return (r3_vec_t){u*R3_M1_COS-v*R3_M1_SIN,-u*R3_M1_SIN-v*R3_M1_COS,z};}
static void render(const r3_snapshot_t *s,const r3_world_t *w)
{assert(roll3_renderer_render(color+1,depth+1,s,w)==0);assert(color[0]==0xabcd&&color[R3_VIEW_PIXELS+1]==0xabcd);
 assert(depth[0]==0xabcd&&depth[R3_VIEW_PIXELS+1]==0xabcd);}
static unsigned coverage(void){unsigned n=0;for(unsigned i=1;i<=R3_VIEW_PIXELS;++i)n+=depth[i]!=65535;return n;}
int main(void)
{
    puts("3D host view and production page adapter with fake runtime");
    color[0]=color[R3_VIEW_PIXELS+1]=depth[0]=depth[R3_VIEW_PIXELS+1]=0xabcd;
    assert(roll3_renderer_selftest(math_report)==0);
    const r3_world_t *world=r3_m1_world();
    r3_snapshot_t s={.version=ROLL3_CONTRACT_VERSION,.radius=.2f,.orientation={1,0,0,0},.position={3,0,1}};
    r3_box_t distant=world->boxes[0];distant.center=(r3_vec_t){30,20,0};
    r3_world_t lone=*world;lone.boxes=&distant;lone.box_count=1;
    lone.spawn=(r3_vec_t){0,0,R3_RADIUS}; /* Independent cube/ball fixture. */
    render(&s,&lone);unsigned near_count=coverage();memcpy(saved,color+1,sizeof(saved));
    s.position.y=3;render(&s,&lone);assert(coverage()==near_count);
    float px,py,pz,qx,qy,qz;
    assert(roll3_renderer_project_focus((r3_vec_t){.2f*R3_CAM_COS,-.2f*R3_CAM_SIN,.2f},(r3_vec_t){0,0,.2f},&px,&py,&pz));
    assert(roll3_renderer_project_focus((r3_vec_t){.2f*R3_CAM_COS+3*R3_CAM_SIN,-.2f*R3_CAM_SIN+3*R3_CAM_COS,.2f},(r3_vec_t){0,0,.2f},&qx,&qy,&qz));
    assert(qz>pz&&fabsf(qx-R3_VIEW_W*.5f)<fabsf(px-R3_VIEW_W*.5f));
    s.position.y=0;s.orientation=(r3_quat_t){.92387953f,.38268343f,0,0};render(&s,&lone);
    assert(memcmp(saved,color+1,sizeof(saved))!=0);
    s.orientation=(r3_quat_t){1,0,0,0};s.position=world->checkpoint_spawn;
    render(&s,world);float x,y,z;
    assert(roll3_renderer_project_focus(s.position,roll3_renderer_focus(&s,world),&x,&y,&z));unsigned center=(unsigned)y*R3_VIEW_W+(unsigned)x;
    uint16_t ball_pixel=color[1+center];
    r3_box_t boxes[R3_MAX_BOXES];memcpy(boxes,world->boxes,world->box_count*sizeof(*boxes));
    boxes[world->box_count]=distant;
    boxes[world->box_count].center=(r3_vec_t){s.position.x-1.28f*R3_CAM_SIN,
        s.position.y-1.28f*R3_CAM_COS,s.position.z+1.04f};
    boxes[world->box_count].half=(r3_vec_t){.7f,.1f,1};boxes[world->box_count].color=0xff0000;
    r3_world_t occluded=*world;occluded.boxes=boxes;occluded.box_count++;
    render(&s,&occluded);assert(color[1+center]!=ball_pixel);assert((color[1+center]&31)==0);
    s.position=from_uv(0,2.75f,1.475f);render(&s,world);memcpy(saved,color+1,sizeof(saved));
    memcpy(boxes,world->boxes,world->box_count*sizeof(*boxes));
    boxes[1].axis[0]=from_uv(0,1,0);boxes[1].axis[2]=(r3_vec_t){0,0,1};
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

    s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},
        .position={99,99,-3},.state=R3_FALLEN};
    r3_vec_t focus=roll3_renderer_focus(&s,world);
    assert(focus.x==world->spawn.x&&focus.y==world->spawn.y&&focus.z==world->spawn.z);
    s.checkpoint=1;focus=roll3_renderer_focus(&s,world);
    assert(focus.x==world->checkpoint_spawn.x&&focus.y==world->checkpoint_spawn.y);
    s.state=R3_RUNNING;s.position=world->checkpoint_spawn;focus=roll3_renderer_focus(&s,world);
    assert(focus.x==s.position.x&&focus.y==s.position.y&&focus.z==s.position.z);
    puts("TC-505 fallen checkpoint framing and immediate respawn focus PASS");

    r3_camera_follow_t follow={0};
    s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},.position=world->spawn};
    assert(roll3_renderer_render_follow(color+1,depth+1,&s,world,&follow,aa+1)==0);
    r3_camera_follow_t before_follow=follow;
    s.orientation.w=0;
    assert(roll3_renderer_render_follow(color+1,depth+1,&s,world,&follow,aa+1)<0);
    assert(memcmp(&follow,&before_follow,sizeof(follow))==0);
    puts("TC-506 invalid snapshot preserves camera PASS");

    r3_box_t cube=world->boxes[0];
    cube.center=(r3_vec_t){0,0,-.5f};cube.half=(r3_vec_t){.5f,.5f,.5f};
    cube.axis[0]=(r3_vec_t){1,0,0};cube.axis[1]=(r3_vec_t){0,1,0};
    lone.boxes=&cube;
    s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},
        .position={99,99,-3},.state=R3_FALLEN};
    aa[0]=aa[R3_VIEW_PIXELS+1]=0xabcd;
    assert(roll3_renderer_render_follow(color+1,depth+1,&s,&lone,NULL,aa+1)==0);
    assert(aa[0]==0xabcd&&aa[R3_VIEW_PIXELS+1]==0xabcd);
    r3_vec_t face_centers[]={{.1f,.1f,0},{-.5f,0,-.5f},{0,-.5f,-.5f}};
    uint16_t face_colors[3];
    for(unsigned i=0;i<3;++i){
        assert(roll3_renderer_project_focus(face_centers[i],lone.spawn,&x,&y,&z));
        unsigned pixel=(unsigned)y*R3_VIEW_W+(unsigned)x;
        assert(depth[1+pixel]!=65535);
        face_colors[i]=color[1+pixel];
    }
    assert(face_colors[0]!=face_colors[1]&&face_colors[0]!=face_colors[2]&&face_colors[1]!=face_colors[2]);
    uint16_t background=color[1];
    unsigned edge_blends=0;
    for(unsigned i=1;i<=R3_VIEW_PIXELS;++i)
        edge_blends+=depth[i]==65535&&aa[i]==background&&color[i]!=background;
    assert(edge_blends>0);
    cube.center.x+=.0075f;
    assert(roll3_renderer_render_follow(color+1,depth+1,&s,&lone,NULL,aa+1)==0);
    edge_blends=0;
    for(unsigned i=1;i<=R3_VIEW_PIXELS;++i)
        edge_blends+=depth[i]==65535&&aa[i]==background&&color[i]!=background;
    assert(edge_blends>0);
    puts("TC-507 three cube faces and subpixel edge coverage PASS");

    lv_init();lv_display_t *d=lv_display_create(390,450);lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(d,display_buffer,NULL,sizeof(display_buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
    original_draw=lv_draw_buf_get_handlers()->buf_malloc_cb;lv_draw_buf_get_handlers()->buf_malloc_cb=draw_alloc;
    unsigned base=objects(lv_layer_top()),clock_base=timers();fail_buffer=true;
    assert(roll3_view_create()==-103);assert(strstr(roll3_view_error_text(),"内存不足"));
    assert(buffer_live==0&&objects(lv_layer_top())==base);fail_buffer=false;
    buffer_budget=R3_VIEW_COLOR_BYTES;
    assert(roll3_view_create()==-103&&buffer_live==0&&buffer_bytes==0);
    assert(objects(lv_layer_top())==base&&timers()==clock_base);
    buffer_budget=R3_VIEW_COLOR_BYTES+R3_VIEW_WORKSPACE_BYTES(R3_VIEW_LOW_DEPTH_ROWS);
    assert(roll3_view_create()==0&&roll3_view_depth_rows()==R3_VIEW_LOW_DEPTH_ROWS);
    s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},
        .position=world->spawn,.state=R3_RUNNING};
    roll3_view_render(&s,world);
    screenshot("roll3-LOW-MEMORY.ppm");
    roll3_view_destroy();
    assert(buffer_live==0&&buffer_bytes==0&&objects(lv_layer_top())==base);
    buffer_budget=R3_VIEW_COLOR_BYTES+R3_VIEW_WORKSPACE_BYTES(R3_VIEW_DEFAULT_DEPTH_ROWS);
    /* Inspect the actual image plane owned by LVGL for the two quality paths. */
    static r3_edge_aa_t expected_edges;
    assert(roll3_view_create()==0);
    s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},
        .position=world->checkpoint_spawn,.state=R3_RUNNING};
    roll3_view_render(&s,world);
    lv_obj_t *quality_page=lv_obj_get_child(lv_layer_top(),-1);
    lv_obj_t *quality_image=lv_obj_get_child(quality_page,0);
    const lv_image_dsc_t *quality=lv_image_get_src(quality_image);
    assert(roll3_renderer_render_play(color+1,depth+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,NULL)==0);
    assert(memcmp(quality->data,color+1,R3_VIEW_COLOR_BYTES)==0);
    assert(roll3_renderer_render_edges(saved,depth+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,NULL,NULL)==0);
    assert(memcmp(quality->data,saved,R3_VIEW_COLOR_BYTES)!=0);
    s.state=R3_PAUSED;
    roll3_view_render(&s,world);
    assert(roll3_renderer_render_edges(saved,depth+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,NULL,&expected_edges)==0);
    assert(memcmp(quality->data,saved,R3_VIEW_COLOR_BYTES)==0);
    assert(memcmp(color+1,saved,R3_VIEW_COLOR_BYTES)!=0);
    roll3_view_destroy();
    assert(buffer_live==0&&objects(lv_layer_top())==base&&timers()==clock_base);
    puts("TC-516 actual play smoothing and pause edge coverage PASS");
    const char *names[]={"READY","CALIBRATING","RUNNING","PAUSED","FALLEN","WON","ERROR"};
    for(unsigned round=0;round<20;++round){assert(roll3_view_create()==0);assert(roll3_view_create()==-100);
        unsigned count=objects(lv_layer_top()),allocations=buffer_attempts;
        for(unsigned st=0;st<7;++st){s=(r3_snapshot_t){.version=1,.radius=.2f,.orientation={1,0,0,0},
                .position=world->spawn,.state=(r3_state_t)st,.run=1,.revision=st,
                .elapsed_ms=st>=2?12345:0,.falls=st>=2?1:0,.checkpoint=st>=2?1:0};
            if(st>=2)s.position=world->checkpoint_spawn;
            if(st==R3_FALLEN)s.position=from_uv(-2,5.5f,-1);
            if(st==R3_WON)s.position=from_uv(5.5f,11,.10f+R3_RADIUS);
            if(st==R3_ERROR)s.error=R3_INPUT_ERROR;
            s.orientation=(r3_quat_t){.92387953f,.38268343f,0,0};
            roll3_view_render(&s,world);glyphs(lv_layer_top());
            lv_obj_t *close=find(lv_layer_top(),"返回");assert(close);
            lv_obj_t *root=lv_obj_get_child(lv_layer_top(),-1);
            for(int i=4;i<=5;++i)assert(lv_obj_has_state(lv_obj_get_child(root,i),LV_STATE_DISABLED));
            if(round==0){char file[80];snprintf(file,sizeof(file),"roll3-%s.ppm",names[st]);screenshot(file);}
            assert(buffer_live==2&&buffer_bytes==buffer_budget&&buffer_attempts==allocations&&objects(lv_layer_top())==count&&timers()==clock_base);
        }
        s.orientation=(r3_quat_t){0,0,0,0};roll3_view_render(&s,world);assert(find(lv_layer_top(),"三维数据无效"));
        if(round%2)lv_obj_delete(lv_obj_get_child(lv_layer_top(),-1));else
            lv_obj_send_event(lv_obj_get_parent(find(lv_layer_top(),"返回")),LV_EVENT_CLICKED,NULL);
        roll3_view_destroy();roll3_view_destroy();assert(buffer_live==0&&objects(lv_layer_top())==base&&timers()==clock_base);
    }
    printf("TC-501 seven Chinese states/glyphs, controls, allocation failure, 20 cycles PASS; persistent buffers=%lu bytes\n",(unsigned long)buffer_budget);
    const struct {const char *name;float u,v,z;r3_state_t state;} views[]={
        {"M1-S.ppm",0,0,1.70f,R3_READY},{"M1-C1.ppm",0,5.5f,1.25f,R3_RUNNING},
        {"M1-C2.ppm",5.5f,5.5f,.80f,R3_RUNNING},{"M1-G.ppm",5.5f,11,.30f,R3_WON}
    };
    for(unsigned i=0;i<sizeof(views)/sizeof(*views);++i){
        assert(roll3_view_create()==0);
        s=(r3_snapshot_t){.version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.orientation={1,0,0,0},
            .position=from_uv(views[i].u,views[i].v,views[i].z),.state=views[i].state,.checkpoint=i!=0};
        roll3_view_render(&s,world);screenshot(views[i].name);roll3_view_destroy();
    }
    assert(buffer_live==0&&objects(lv_layer_top())==base&&timers()==clock_base);
    puts("TC-513 four M1 preview poses captured with production camera direction");
    const r3_world_t *m2=r3_m2_world();
    const r3_bend_t *bend=m2->bend;
    assert(bend&&r3_world_validate(m2)==R3_OK);
    const r3_vec_t m2_positions[]={
        m2->spawn,from_uv(4.35f,9.20f,2.325f),m2->checkpoint_spawn,
        r3_bend_point(bend,0,1,bend->radius,r3_bend_height(bend,0,1,bend->radius)+R3_RADIUS),
        from_uv(.10f,12.90f,1.08f+R3_RADIUS)
    };
    const char *m2_views[]={"M2-S.ppm","M2-RAMP.ppm","M2-CP.ppm","M2-BEND.ppm","M2-G.ppm"};
    for(unsigned i=0;i<sizeof(m2_positions)/sizeof(*m2_positions);++i){
        assert(roll3_view_create()==0);
        s=(r3_snapshot_t){.version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.orientation={1,0,0,0},
            .position=m2_positions[i],.state=i==0?R3_READY:i==4?R3_WON:R3_RUNNING,.checkpoint=i>=2};
        roll3_view_render(&s,m2);
        assert(!find(lv_layer_top(),"三维数据无效"));
        screenshot(m2_views[i]);roll3_view_destroy();
    }
    assert(buffer_live==0&&objects(lv_layer_top())==base&&timers()==clock_base);
    puts("TC-517 M2 start, long ramp, checkpoint, banked bend and goal captured");
    const r3_world_t *m3=r3_demo_world();
    bend=m3->bend;
    assert(bend&&m3->style&&r3_world_validate(m3)==R3_OK);
    const r3_vec_t m3_positions[]={
        m3->spawn,from_uv(1.40f,5.09f,3.05f+R3_RADIUS),
        from_uv(4.35f,10.3f,2.55f+R3_RADIUS),m3->checkpoint_spawn,
        r3_bend_point(bend,0,1,bend->radius,r3_bend_height(bend,0,1,bend->radius)+R3_RADIUS),
        from_uv(.10f,16.60f,1.08f+R3_RADIUS)
    };
    const char *m3_views[]={"M3-S.ppm","M3-TURN.ppm","M3-LANDING.ppm","M3-CP.ppm","M3-BEND.ppm","M3-G.ppm"};
    for(unsigned i=0;i<sizeof(m3_positions)/sizeof(*m3_positions);++i){
        assert(roll3_view_create()==0);
        s=(r3_snapshot_t){.version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.orientation={1,0,0,0},
            .position=m3_positions[i],.state=i==0?R3_READY:i==5?R3_WON:R3_RUNNING,.checkpoint=i>=3};
        roll3_view_render(&s,m3);
        assert(!find(lv_layer_top(),"三维数据无效"));
        assert(buffer_live==2&&buffer_bytes==buffer_budget);
        screenshot(m3_views[i]);roll3_view_destroy();
    }
    assert(buffer_live==0&&objects(lv_layer_top())==base&&timers()==clock_base);
    puts("TC-518 M3 six scene poses with standard view allocation PASS");
    fail_buffer = true;
    assert(roll3_page_open() == -103 && !live_open && closes == 1);
    fail_buffer = false;
    assert(roll3_page_open() == 0 && roll3_page_open() == -104);
    assert(last_open_result==0&&last_depth_rows==R3_VIEW_DEFAULT_DEPTH_ROWS);
    lv_obj_update_layout(lv_layer_top());
    lv_obj_t *live_root = lv_obj_get_child(lv_layer_top(), -1);
    lv_obj_t *live_image = lv_obj_get_child(live_root, 0);
    lv_obj_t *live_detail = lv_obj_get_child(live_root, 2);
    int32_t image_bottom = lv_obj_get_y(live_image) +
        (R3_VIEW_H * lv_image_get_scale(live_image) + 255) / 256;
    assert(image_bottom <= lv_obj_get_y(live_detail));
    assert(lv_obj_get_x(live_image)>=0&&lv_obj_get_x(live_image)+R3_VIEW_W*3/2<=390);
    assert(lv_obj_get_y(live_image)>=35&&lv_image_get_scale(live_image)==384&&lv_image_get_antialias(live_image));
    click("开始"); assert(requests == 1 && last_action == R3_START);
    click("开始"); assert(requests == 1);
    live_snapshot.run = 1;
    publish(R3_CALIBRATING);
    click("开始"); assert(requests == 1);
    publish(R3_RUNNING);
    click("暂停"); assert(requests == 2 && last_action == R3_PAUSE);
    publish(R3_PAUSED);
    click("继续"); assert(requests == 3 && last_action == R3_RESUME);
    publish(R3_FALLEN);
    click("回检查点"); assert(requests == 4 && last_action == R3_RESPAWN);
    publish(R3_WON);
    click("重开"); assert(requests == 5 && last_action == R3_RESTART);
    glyphs(lv_layer_top());
    click("返回"); assert(!live_open && closes == 2);
    assert(roll3_page_open() == 0);
    lv_obj_delete(lv_obj_get_child(lv_layer_top(), -1));
    roll3_page_close();
    assert(!live_open && closes == 3 && !buffer_live &&
           objects(lv_layer_top()) == base && timers() == clock_base);
    fail_timer=true;
    assert(roll3_page_open()==-105&&last_open_result==-105&&last_depth_rows==0);
    assert(!live_open&&closes==4&&buffer_live==0&&buffer_bytes==0);
    assert(objects(lv_layer_top())==base&&timers()==clock_base);
    fail_timer=false;
    puts("TC-510 split budget, compact fallback, partial cleanup and timer error PASS");
    puts("TC-504 live layout, buttons, duplicate guard, return, external delete PASS");
    printf("LVGL draw-buffer malloc attempts=%u largest=%lu bytes (in addition to caller buffers)\n",draw_attempts,(unsigned long)largest_draw);
    return 0;
}
