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
#include <stdlib.h>
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

bool roll3_runtime_open(void){return roll3_runtime_open_level(1);}
bool roll3_runtime_open_level(unsigned level)
{
    if(!r3_level_world(level))return false;
    if (live_open) return false;
    live_snapshot = (r3_snapshot_t){.version = ROLL3_CONTRACT_VERSION, .radius = R3_RADIUS,
        .position = r3_level_world(level)->spawn, .orientation = {1, 0, 0, 0}};
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
    if(o==lv_layer_top())lv_obj_update_layout(o);
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
    lv_obj_update_layout(lv_layer_top());lv_refr_now(NULL);
    FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n390 450\n255\n");
    for(unsigned i=0;i<sizeof(frame);i+=4){fputc(frame[i+2],f);fputc(frame[i+1],f);fputc(frame[i],f);}fclose(f);
}
static void click(const char *caption)
{
    lv_obj_t *target = find(lv_layer_top(), caption);
    assert(target);
    lv_obj_send_event(lv_obj_get_parent(target), LV_EVENT_PRESSED, NULL);
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
static unsigned bit_at(const uint8_t *p,unsigned i){return (p[i/8]>>(7-i%8))&1;}
static void pixel_font(const lv_font_t *font)
{
    const lv_font_fmt_txt_dsc_t *d=font->dsc;
    assert(d->bpp==1&&font->line_height%2==0&&font->base_line%2==0);
    for(unsigned c=0;c<d->cmaps[0].list_length;++c){
        const lv_font_fmt_txt_glyph_dsc_t *g=&d->glyph_dsc[c+1];
        assert(g->box_w%2==0&&g->box_h%2==0&&g->ofs_x%2==0&&g->ofs_y%2==0&&g->adv_w%32==0);
        const uint8_t *b=d->glyph_bitmap+g->bitmap_index;
        for(unsigned y=0;y<g->box_h;y+=2)for(unsigned x=0;x<g->box_w;x+=2){
            unsigned i=y*g->box_w+x,v=bit_at(b,i);
            assert(v==bit_at(b,i+1)&&v==bit_at(b,i+g->box_w)&&v==bit_at(b,i+g->box_w+1));
        }
    }
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    puts("3D host view and production page adapter with fake runtime");
    pixel_font(&roll3_font_16);pixel_font(&roll3_font_20);pixel_font(&roll3_font_28);
    puts("TC-526 monochrome 2x2 block glyphs/metrics, three sizes PASS");
    color[0]=color[R3_VIEW_PIXELS+1]=depth[0]=depth[R3_VIEW_PIXELS+1]=0xabcd;
    assert(roll3_renderer_selftest(math_report)==0);
    const r3_world_t *world=r3_m1_world();
    r3_snapshot_t s={.version=ROLL3_CONTRACT_VERSION,.radius=.2f,.orientation={1,0,0,0},.position={3,0,1}};
    r3_box_t distant=world->boxes[0];distant.center=(r3_vec_t){80,80,0}; /* Legal world, beyond far plane in size fixture. */
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
    unsigned base=objects(lv_layer_top()),clock_base=timers();
    const r3_world_t *m3=r3_demo_world();

    fail_buffer=true;assert(roll3_view_create()==-103);fail_buffer=false;
    assert(!buffer_live&&objects(lv_layer_top())==base);
    buffer_budget=R3_VIEW_COLOR_BYTES;assert(roll3_view_create()==-103&&!buffer_live&&!buffer_bytes);
    buffer_budget=R3_VIEW_COLOR_BYTES+R3_VIEW_WORKSPACE_BYTES(R3_VIEW_LOW_DEPTH_ROWS);
    assert(roll3_view_create()==0&&roll3_view_depth_rows()==R3_VIEW_LOW_DEPTH_ROWS);
    s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},.position=m3->checkpoint_spawn,.state=R3_RUNNING};
    roll3_view_render(&s,m3);glyphs(lv_layer_top());screenshot("IMMERSIVE-LOW.ppm");roll3_view_destroy();
    assert(!buffer_live&&!buffer_bytes&&objects(lv_layer_top())==base&&timers()==clock_base);
    buffer_budget=R3_VIEW_COLOR_BYTES+R3_VIEW_WORKSPACE_BYTES(R3_VIEW_DEFAULT_DEPTH_ROWS);
    assert(buffer_budget<=153600&&R3_VIEW_COLOR_BYTES<=96000);
    assert(R3_VIEW_COLOR_BYTES+R3_VIEW_WORKSPACE_BYTES(50)<=129600);
    assert(roll3_view_create()==0);
    roll3_view_render(&s,m3);
    lv_obj_t *root=lv_obj_get_child(lv_layer_top(),-1),*plane=lv_obj_get_child(root,0);
    const lv_image_dsc_t *picture=lv_image_get_src(plane);
    assert(picture->header.w==200&&picture->header.h==232&&lv_image_get_scale(plane)==512);
    lv_obj_update_layout(root);
    assert(lv_obj_get_x(plane)<=0&&lv_obj_get_y(plane)<=0);
    assert(lv_obj_get_x(plane)+(int)picture->header.w*2>=390&&lv_obj_get_y(plane)+(int)picture->header.h*2>=450);
    screenshot("IMMERSIVE-PLAY.ppm");
    unsigned top_pixel=frame[(2*390+195)*4]|frame[(2*390+195)*4+1]<<8|frame[(2*390+195)*4+2]<<16;
    unsigned bottom_pixel=frame[(447*390+195)*4]|frame[(447*390+195)*4+1]<<8|frame[(447*390+195)*4+2]<<16;
    assert(top_pixel!=0x0c1220&&bottom_pixel!=0x0c1220);
    assert(!find(lv_layer_top(),"游玩中")&&!find(lv_layer_top(),"主机预览：操作尚未接入"));
    assert(find(lv_layer_top(),"MENU")&&find(lv_layer_top(),"00:00"));
    lv_obj_t *menu=lv_obj_get_parent(find(lv_layer_top(),"MENU"));
    lv_obj_t *clock_panel=lv_obj_get_parent(find(lv_layer_top(),"00:00"));
    assert(lv_obj_get_width(menu)>=56&&lv_obj_get_height(menu)>=56);
    assert(lv_obj_get_style_radius(menu,0)==0&&lv_obj_get_style_radius(clock_panel,0)==0);
    assert((lv_color_to_u32(lv_obj_get_style_bg_color(clock_panel,0))&0xffffffu)==0xe9cbe5);
    const r3_vec_t poses[]={m3->spawn,from_uv(0,2.7f,3.375f+R3_RADIUS),
        from_uv(4.35f,13.15f,2.05f+R3_RADIUS),from_uv(.1f,16.6f,1.08f+R3_RADIUS)};
    const char *pose_names[]={"START","SLOPE","LONG","GOAL"};
    for(unsigned i=0;i<4;++i){
        s.position=poses[i];++s.run;roll3_view_render(&s,m3);
        char file[80];snprintf(file,sizeof(file),"PIXEL-%s.ppm",pose_names[i]);screenshot(file);
    }
    s.position=r3_bend_point(m3->bend,0,1,m3->bend->radius,
        r3_bend_height(m3->bend,0,1,m3->bend->radius)+R3_RADIUS);
    ++s.run;roll3_view_render(&s,m3);screenshot("PIXEL-BEND.ppm");
    roll3_view_destroy();
    puts("TC-525 full 390x450 portrait scene, only clock/menu HUD and smaller buffers PASS");

    const char *names[]={"READY","CALIBRATING","RUNNING","PAUSED","FALLEN","WON","ERROR"};
    for(unsigned round=0;round<20;++round){
        assert(roll3_view_create()==0&&roll3_view_create()==-100);
        unsigned count=objects(lv_layer_top()),allocations=buffer_attempts;
        for(unsigned state=0;state<7;++state){
            s=(r3_snapshot_t){.version=1,.radius=R3_RADIUS,.orientation={1,0,0,0},.position=m3->checkpoint_spawn,
                .state=(r3_state_t)state,.run=round+1,.revision=state,.elapsed_ms=state>=2?12345:0,.checkpoint=1};
            if(state==R3_WON)s.position=from_uv(.1f,16.6f,1.08f+R3_RADIUS);
            if(state==R3_ERROR)s.error=R3_INPUT_ERROR;
            roll3_view_render(&s,m3);
            if(state==R3_RUNNING){lv_tick_inc(600);roll3_view_tick();}
            if(state==R3_WON){lv_tick_inc(800);roll3_view_tick();}
            glyphs(lv_layer_top());
            if(round==0){char file[80];snprintf(file,sizeof(file),"IMMERSIVE-%s.ppm",names[state]);screenshot(file);}
            assert(objects(lv_layer_top())==count&&buffer_live==2&&buffer_bytes==buffer_budget&&buffer_attempts==allocations&&timers()==clock_base);
        }
        s.orientation=(r3_quat_t){0,0,0,0};roll3_view_render(&s,m3);assert(find(lv_layer_top(),"DATA ERROR"));
        if(round%2)lv_obj_delete(lv_obj_get_child(lv_layer_top(),-1));else click("EXIT");
        roll3_view_destroy();
        assert(!buffer_live&&!buffer_bytes&&objects(lv_layer_top())==base&&timers()==clock_base);
    }
    puts("TC-525 seven modal states, all product glyphs, 20 exits/external deletes and resource baseline PASS");
    puts("TC-527 English-only labels, printable glyph coverage, visible text widths and MENU hit area PASS");

    fail_buffer=true;assert(roll3_page_open()==-103&&!live_open);fail_buffer=false;
    fail_timer=true;assert(roll3_page_open()==-105&&!live_open);fail_timer=false;
    assert(!buffer_live&&objects(lv_layer_top())==base&&timers()==clock_base);
    assert(roll3_page_open()==0&&live_open);
    assert(find(lv_layer_top(),"READY?"));
    unsigned sent=requests;
    click("CALIBRATE");click("CALIBRATE");assert(requests==sent+1&&last_action==R3_START);
    publish(R3_CALIBRATING);assert(find(lv_layer_top(),"CALIBRATING"));
    screenshot("IMMERSIVE-CALIBRATION.ppm");
    live_snapshot.elapsed_ms=12345;publish(R3_RUNNING);assert(find(lv_layer_top(),"READY!"));
    screenshot("IMMERSIVE-GO.ppm");
    lv_tick_inc(600);lv_timer_handler();
    click("MENU");assert(last_action==R3_PAUSE&&live_pending);sent=requests;
    click("RESUME");assert(requests==sent);
    publish(R3_PAUSED);assert(find(lv_layer_top(),"BALL AND TIMER PAUSED"));
    screenshot("IMMERSIVE-MENU.ppm");
    lv_obj_t *pause_card=lv_obj_get_parent(find(lv_layer_top(),"PAUSED"));
    assert(lv_obj_get_style_radius(pause_card,0)==0);
    const char *actions[]={"RESUME","CHECKPOINT","EXIT"};
    for(unsigned i=0;i<3;++i){lv_obj_t *b=lv_obj_get_parent(find(pause_card,actions[i]));
        assert(lv_obj_get_height(b)>=44&&lv_obj_get_style_radius(b,0)==0);}
    lv_obj_t *old_checkpoint_button=lv_obj_get_parent(find(lv_layer_top(),"CHECKPOINT"));
    click("RESUME");assert(last_action==R3_RESUME);publish(R3_RUNNING);
    sent=requests;lv_obj_send_event(old_checkpoint_button,LV_EVENT_CLICKED,NULL);
    assert(live_open&&requests==sent);
    click("MENU");publish(R3_PAUSED);click("CHECKPOINT");assert(last_action==R3_RESPAWN);
    live_snapshot.position=m3->checkpoint_spawn;publish(R3_RUNNING);
    assert(find(lv_layer_top(),"00:12"));
    publish(R3_ERROR);click("RECALIBRATE");assert(last_action==R3_RESTART);
    ++live_snapshot.run;live_snapshot.elapsed_ms=0;publish(R3_CALIBRATING);publish(R3_RUNNING);
    lv_tick_inc(600);lv_timer_handler();
    puts("TC-525 real action sink: pause waits for acknowledgement, duplicate guard, resume, checkpoint and recalibrate PASS");

    live_snapshot.elapsed_ms=12345;live_snapshot.position=from_uv(.1f,16.6f,1.08f+R3_RADIUS);publish(R3_WON);
    lv_obj_t *result_title=find(lv_layer_top(),"LEVEL CLEAR!");assert(result_title);
    lv_obj_t *result_card=lv_obj_get_parent(result_title);
    lv_obj_update_layout(lv_layer_top());int start_y=lv_obj_get_y(result_card),start_width=lv_obj_get_width(result_card);
    assert(lv_obj_get_style_text_opa(result_title,LV_PART_MAIN)==0);
    screenshot("IMMERSIVE-RESULT-000.ppm");
    const unsigned times[]={200,400,800};unsigned elapsed=0,old_opacity=0;int old_width=start_width;
    for(unsigned i=0;i<3;++i){
        lv_tick_inc(times[i]-elapsed);elapsed=times[i];
        live_snapshot.position.x+=.01f;++live_snapshot.revision;
        roll3_view_render(&live_snapshot,m3);roll3_view_tick();lv_obj_update_layout(lv_layer_top());
        unsigned opacity=lv_obj_get_style_text_opa(result_title,LV_PART_MAIN);
        assert(lv_obj_get_x(result_card)%2==0&&lv_obj_get_y(result_card)%2==0);
        assert(lv_obj_get_width(result_card)%2==0&&lv_obj_get_height(result_card)%2==0);
        assert(opacity>old_opacity&&lv_obj_get_width(result_card)>old_width);
        old_opacity=opacity;old_width=lv_obj_get_width(result_card);
        char file[80];snprintf(file,sizeof(file),"IMMERSIVE-RESULT-%03u.ppm",times[i]);screenshot(file);
    }
    assert(lv_obj_get_y(result_card)<start_y&&old_opacity==255);
    assert(find(lv_layer_top(),"00:12.34")&&find(lv_layer_top(),"PLAY TO PICK LEVEL"));
    sent=requests;assert(!find(lv_layer_top(),"NEXT LEVEL"));assert(requests==sent);
    lv_tick_inc(1000);lv_timer_handler();assert(lv_obj_get_style_text_opa(result_title,LV_PART_MAIN)==255);
    assert(find(lv_layer_top(),"00:12.34"));
    click("EXIT");assert(!live_open&&!buffer_live);
    puts("TC-524 settlement reveal at 0/200/400/800ms, frozen score and return-to-PLAY selection hint PASS");

    assert(roll3_page_open()==0);publish(R3_RUNNING);live_snapshot.elapsed_ms=7000;publish(R3_WON);
    lv_tick_inc(200);roll3_view_tick();lv_obj_delete(lv_obj_get_child(lv_layer_top(),-1));
    assert(!live_open&&!buffer_live);
    assert(roll3_page_open()==0);assert(find(lv_layer_top(),"READY?"));
    lv_tick_inc(1000);lv_timer_handler();assert(!find(lv_layer_top(),"LEVEL CLEAR!"));
    lv_tick_inc((UINT32_MAX-300u)-lv_tick_get());publish(R3_WON);
    result_title=find(lv_layer_top(),"LEVEL CLEAR!");assert(result_title);
    lv_tick_inc(800);roll3_view_tick();assert(lv_obj_get_style_text_opa(result_title,LV_PART_MAIN)==255);
    click("EXIT");roll3_page_close();
    assert(!live_open&&!buffer_live&&objects(lv_layer_top())==base&&timers()==clock_base);
    puts("TC-524 close during animation, clean reentry and millisecond wrap PASS");
    const r3_world_t *m4=r3_m4_world();
    static const float m4_poses[][3]={{0,0,6},{2,5,5.35f},{4,0,4.85f},
        {0,-1.05f,4.85f},{-2,0,4.85f},{-4.7f,-1.7f,4.425f},{-.35f,-3.4f,3.7f},
        {.8f,-3.4f,3.066f},{3.1f,-3.4f,2.3f},{3.1f,-1.3f,1.624f},{3.1f,1.5f,.8f}};
    for(unsigned i=0;i<sizeof(m4_poses)/sizeof(*m4_poses);++i){
        assert(roll3_view_create()==0);
        s=(r3_snapshot_t){.version=1,.run=i+1,.radius=R3_RADIUS,.orientation={1,0,0,0},
            .state=R3_RUNNING,.position=from_uv(-m4_poses[i][0],-m4_poses[i][1],m4_poses[i][2])};
        roll3_view_render(&s,m4);glyphs(lv_layer_top());
        char file[64];snprintf(file,sizeof(file),"M4-%02u.ppm",i);screenshot(file);
        assert(roll3_renderer_render_edges(color+1,depth+1,100,&s,m4,NULL,NULL)==0);
        memcpy(saved,color+1,sizeof(saved));
        assert(roll3_renderer_render_edges(color+1,depth+1,50,&s,m4,NULL,NULL)==0);
        assert(memcmp(saved,color+1,sizeof(saved))==0);
        assert(color[0]==0xabcd&&color[R3_VIEW_PIXELS+1]==0xabcd);
        roll3_view_destroy();
    }
    assert(roll3_view_create()==0);
    s.state=R3_PAUSED;roll3_view_render(&s,m4);screenshot("M4-PAUSED.ppm");
    s.state=R3_WON;roll3_view_render(&s,m4);lv_tick_inc(800);roll3_view_tick();screenshot("M4-RESULT.ppm");
    roll3_view_destroy();
    puts("TC-528 M4 11 poses, 100/50 rows, guards and SDK pause/result PASS");
    assert(roll3_page_open_level(2)==0);assert(live_snapshot.position.z==r3_level2_world()->spawn.z);publish(R3_RUNNING);screenshot("LEVEL2-PAGE.ppm");click("MENU");publish(R3_PAUSED);click("EXIT");assert(!live_open&&!buffer_live);
    for(unsigned i=0;i<20;++i){assert(roll3_page_open_level(i%2+1)==0);publish(R3_RUNNING);click("MENU");publish(R3_PAUSED);click("EXIT");assert(!live_open&&!buffer_live&&objects(lv_layer_top())==base&&timers()==clock_base);}
    const char *pose_path=getenv("ROLL3_LEVEL2_POSES");assert(pose_path);FILE *physics_poses_file=fopen(pose_path,"r");assert(physics_poses_file);
    unsigned pi=0;while(1){unsigned state,cp,ms;float x,y,z,qw,qx,qy,qz;
        int got=fscanf(physics_poses_file,"%u,%u,%u,%f,%f,%f,%f,%f,%f,%f",&ms,&state,&cp,&x,&y,&z,&qw,&qx,&qy,&qz);if(got!=10)break;
        s=(r3_snapshot_t){.version=1,.run=1000+pi,.radius=R3_RADIUS,.orientation={qw,qx,qy,qz},.state=(r3_state_t)state,.position={x,y,z},.elapsed_ms=ms,.checkpoint=cp};
        assert(roll3_view_create()==0);roll3_view_render(&s,r3_level2_world());glyphs(lv_layer_top());
        char file[64];snprintf(file,sizeof(file),"LEVEL2-%02u.ppm",pi);screenshot(file);
        assert(roll3_renderer_render_edges(color+1,depth+1,100,&s,r3_level2_world(),NULL,NULL)==0);memcpy(saved,color+1,sizeof(saved));
        assert(roll3_renderer_render_edges(color+1,depth+1,50,&s,r3_level2_world(),NULL,NULL)==0);assert(!memcmp(saved,color+1,sizeof(saved)));
        assert(color[0]==0xabcd&&color[R3_VIEW_PIXELS+1]==0xabcd&&depth[0]==0xabcd&&depth[R3_VIEW_PIXELS+1]==0xabcd);
        roll3_view_destroy();++pi;
    }fclose(physics_poses_file);assert(pi>=10);
    puts("TC-533/534 actual level2 page and physical-replay poses, 100/50 rows, canaries PASS");
    printf("LVGL draw-buffer malloc attempts=%u largest=%lu bytes (caller planes excluded)\n",draw_attempts,(unsigned long)largest_draw);
    assert(draw_attempts==0);
    return 0;
}
