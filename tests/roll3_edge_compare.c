/* Deterministic production raster comparison. Run in a writable output folder.
 * Build with roll3_scene.c and roll3_physics.c; the include exposes band internals. */
#include "../src/roll3_renderer.c"
#include "roll3_scene.h"
#include "roll3_bend.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { FRAME_COUNT = 36 };
static uint16_t baseline[R3_VIEW_PIXELS], reference[R3_VIEW_PIXELS];
static uint16_t local[R3_VIEW_PIXELS+2], scratch[R3_VIEW_PIXELS+2];
static uint16_t full_depth[R3_VIEW_PIXELS], full_color[EDGE_SAMPLES][R3_VIEW_PIXELS];
static uint16_t banded[R3_VIEW_PIXELS];
static uint16_t regular[R3_VIEW_W*R3_VIEW_DEFAULT_DEPTH_ROWS+2];
static uint16_t compact[R3_VIEW_W*R3_VIEW_LOW_DEPTH_ROWS+2];
static r3_edge_aa_t edges;

static r3_snapshot_t pose(unsigned frame)
{
    /* Twelve synthetic poses per straight leg, including both turns and the
     * finish. The continuous physics replay is checked in roll3_selftest.c. */
    unsigned leg=frame/12;
    float t=(float)(frame%12)/11,rotation=.39269908f+(float)frame*.04f;
    float u=leg==0?0:leg==1?5.5f*t:5.5f;
    float v=leg==0?5.5f*t:leg==1?5.5f:5.5f+5.5f*t;
    float z=leg==0?1.50f-.45f*fminf(1,fmaxf(0,(v-1)/3.5f)):
            leg==1?1.05f-.45f*fminf(1,fmaxf(0,(u-1.1f)/3.2f)):
                      .60f-.50f*fminf(1,fmaxf(0,(v-6.5f)/3.5f));
    return (r3_snapshot_t){
        .version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.state=R3_RUNNING,
        .position={u*R3_M1_COS-v*R3_M1_SIN,-u*R3_M1_SIN-v*R3_M1_COS,z+R3_RADIUS},
        .orientation={cosf(rotation),sinf(rotation),0,0}
    };
}

static r3_quat_t normalize(r3_quat_t q)
{
    float inv=1/sqrtf(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);
    return (r3_quat_t){q.w*inv,q.x*inv,q.y*inv,q.z*inv};
}

static void full_reference(const r3_snapshot_t *s,const r3_world_t *world)
{
    for(unsigned i=0;i<EDGE_SAMPLES;++i){
        r3_raster_t target={
            .color=full_color[i],.depth=full_depth,.style=world->style,
            .sample_x=i%2?.75f:.25f,.sample_y=i/2?.75f:.25f
        };
        clear_target(&target);
        draw_scene(&target,s,world,normalize(s->orientation),roll3_renderer_focus(s,world));
    }
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i){
        unsigned red=0,green=0,blue=0;
        for(unsigned j=0;j<EDGE_SAMPLES;++j){
            uint16_t pixel=full_color[j][i];
            red+=pixel>>11;
            green+=(pixel>>5)&63u;
            blue+=pixel&31u;
        }
        reference[i]=(uint16_t)((((red+2)/4)<<11)|(((green+2)/4)<<5)|((blue+2)/4));
    }
}

static bool close_color(uint16_t a,uint16_t b)
{
    return abs((int)(a>>11)-(int)(b>>11))<=1 &&
           abs((int)((a>>5)&63u)-(int)((b>>5)&63u))<=1 &&
           abs((int)(a&31u)-(int)(b&31u))<=1;
}

static void check_guards(void)
{
    assert(local[0]==0x7be1&&local[R3_VIEW_PIXELS+1]==0xc473);
    assert(scratch[0]==0x174d&&scratch[R3_VIEW_PIXELS+1]==0xfe18);
}

static void save_ppm(const char *mode,unsigned frame,const uint16_t *color)
{
    char name[64];
    snprintf(name,sizeof(name),"%s-%02u.ppm",mode,frame);
    FILE *file=fopen(name,"wb");
    assert(file);
    fprintf(file,"P6\n%d %d\n255\n",R3_VIEW_W,R3_VIEW_H);
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i){
        uint16_t c=color[i];
        fputc(((c>>11)*255+15)/31,file);
        fputc((((c>>5)&63u)*255+31)/63,file);
        fputc(((c&31u)*255+15)/31,file);
    }
    assert(fclose(file)==0);
}

static double time_ms(void)
{
    struct timespec t;
    assert(timespec_get(&t,TIME_UTC)==TIME_UTC);
    return (double)t.tv_sec*1000.0+(double)t.tv_nsec/1e6;
}

static uint64_t frame_change(const uint16_t *previous, const uint16_t *current)
{
    uint64_t energy=0;
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i){
        int delta=(int)((previous[i]>>5)&63u)-(int)((current[i]>>5)&63u);
        energy+=(unsigned)(delta*delta);
    }
    return energy;
}

static void m1_motion(const r3_world_t *world, bool save)
{
    const r3_box_t *deck=&world->boxes[0];
    r3_vec_t view=sub(add(world->spawn,eye_offset),deck->center);
    assert(dot(view,deck->axis[2])>deck->half.z);
    assert(fabsf(dot(view,deck->axis[0]))>deck->half.x);
    assert(fabsf(dot(view,deck->axis[1]))>deck->half.y);
    r3_vec_t top=add(deck->center,mul(deck->axis[2],deck->half.z));
    float x,y,z,x1,y1,z1;
    assert(roll3_renderer_project_focus(top,world->spawn,&x,&y,&z));
    for(unsigned axis=0;axis<2;++axis){
        assert(roll3_renderer_project_focus(add(top,mul(deck->axis[axis],.5f)),
            world->spawn,&x1,&y1,&z1));
        assert(fabsf(y-y1)>4.0f);
    }
    puts("TC-516 M1 start top and both sides, two oblique top edges PASS");

    uint64_t raw_energy=0,filtered_energy=0;
    double raw_ms=0,filtered_ms=0;
    for(unsigned repeat=0;repeat<3;++repeat){
        r3_camera_follow_t raw={0},filtered={0},low={0};
        for(unsigned i=0;i<FRAME_COUNT;++i){
            const float v=1.9f+.0045f*(float)i;
            r3_snapshot_t s={
                .version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.state=R3_RUNNING,.run=1,
                .elapsed_ms=i*50,.position={-v*R3_M1_SIN,-v*R3_M1_COS,
                    1.50f-.45f*(v-1.0f)/3.5f+R3_RADIUS},
                .orientation={1,0,0,0}
            };
            double start=time_ms();
            assert(roll3_renderer_render_edges(baseline,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,&raw,NULL)==0);
            raw_ms+=time_ms()-start;
            start=time_ms();
            assert(roll3_renderer_render_play(banded,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,&filtered)==0);
            filtered_ms+=time_ms()-start;
            if(repeat==0){
                assert(roll3_renderer_render_play(local+1,compact+1,R3_VIEW_LOW_DEPTH_ROWS,&s,world,&low)==0);
                assert(memcmp(banded,local+1,sizeof(banded))==0);
                if(i){
                    raw_energy+=frame_change(full_color[0],baseline);
                    filtered_energy+=frame_change(full_color[1],banded);
                }
                memcpy(full_color[0],baseline,sizeof(baseline));
                memcpy(full_color[1],banded,sizeof(banded));
                if(save){
                    save_ppm("motion-raw",i,baseline);
                    save_ppm("motion-smoothed",i,banded);
                }
                check_guards();
                assert(regular[0]==0xdada&&regular[sizeof(regular)/sizeof(*regular)-1]==0x1cad);
                assert(compact[0]==0xdada&&compact[sizeof(compact)/sizeof(*compact)-1]==0x1cad);
            }
        }
    }
    printf("TC-516 slow pan frame energy raw=%llu filtered=%llu reduction=%.1f%%\n",
           (unsigned long long)raw_energy,(unsigned long long)filtered_energy,
           100.0*(1.0-(double)filtered_energy/raw_energy));
    printf("TC-516 slow pan mean host ms raw=%.3f filtered=%.3f ratio=%.2f\n",
           raw_ms/(3*FRAME_COUNT),filtered_ms/(3*FRAME_COUNT),filtered_ms/raw_ms);
    assert(raw_energy>0&&filtered_energy*100<=85*raw_energy);
    assert(raw_ms>0&&filtered_ms<=1.5*raw_ms);
    puts("TC-516 motion edges, 100/50-row identity and guards PASS");
}

static void m2_curve_buffers(void)
{
    const r3_world_t *world=r3_m2_world();
    const r3_bend_t *bend=world->bend;
    double center_ms=0,play_ms=0;
    for(unsigned i=0;i<FRAME_COUNT;++i){
        float theta=3.141592654f*i/(FRAME_COUNT-1),c=cosf(theta),s=sinf(theta);
        r3_snapshot_t pose={
            .version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.state=R3_RUNNING,
            .position=r3_bend_point(bend,c,s,bend->radius,
                r3_bend_height(bend,c,s,bend->radius)+R3_RADIUS),.orientation={1,0,0,0}
        };
        double start=time_ms();
        assert(roll3_renderer_render_edges(baseline,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&pose,world,NULL,NULL)==0);
        center_ms+=time_ms()-start;
        start=time_ms();
        assert(roll3_renderer_render_play(banded,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&pose,world,NULL)==0);
        play_ms+=time_ms()-start;
        assert(roll3_renderer_render_play(local+1,compact+1,R3_VIEW_LOW_DEPTH_ROWS,&pose,world,NULL)==0);
        assert(memcmp(banded,local+1,sizeof(banded))==0);
        check_guards();
        assert(regular[0]==0xdada&&regular[sizeof(regular)/sizeof(*regular)-1]==0x1cad);
        assert(compact[0]==0xdada&&compact[sizeof(compact)/sizeof(*compact)-1]==0x1cad);
    }
    printf("TC-517 %u bend frames mean host ms center=%.3f play=%.3f\n",
           FRAME_COUNT,center_ms/FRAME_COUNT,play_ms/FRAME_COUNT);
    puts("TC-517 M2 bend 100/50-row output identity and guards PASS");
}

static void m3_style_buffers(void)
{
    const r3_world_t *world=r3_demo_world();
    r3_course_style_t bare_style=*world->style;
    bare_style.faces=NULL;
    bare_style.face_count=0;
    r3_world_t bare=*world;
    bare.style=&bare_style;
    const r3_bend_t *bend=world->bend;
    double bare_ms=0,style_ms=0;
    unsigned changed=0;
    for(unsigned i=0;i<FRAME_COUNT;++i){
        float theta=3.141592654f*i/(FRAME_COUNT-1),c=cosf(theta),s=sinf(theta);
        r3_snapshot_t pose={
            .version=ROLL3_CONTRACT_VERSION,.radius=R3_RADIUS,.state=R3_RUNNING,
            .position=r3_bend_point(bend,c,s,bend->radius,
                r3_bend_height(bend,c,s,bend->radius)+R3_RADIUS),.orientation={1,0,0,0}
        };
        double start=time_ms();
        assert(roll3_renderer_render_play(baseline,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&pose,&bare,NULL)==0);
        bare_ms+=time_ms()-start;
        start=time_ms();
        assert(roll3_renderer_render_play(banded,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&pose,world,NULL)==0);
        style_ms+=time_ms()-start;
        assert(roll3_renderer_render_play(local+1,compact+1,R3_VIEW_LOW_DEPTH_ROWS,&pose,world,NULL)==0);
        assert(memcmp(banded,local+1,sizeof(banded))==0);
        changed+=memcmp(banded,baseline,sizeof(banded))!=0;
        check_guards();
        assert(regular[0]==0xdada&&regular[sizeof(regular)/sizeof(*regular)-1]==0x1cad);
        assert(compact[0]==0xdada&&compact[sizeof(compact)/sizeof(*compact)-1]==0x1cad);
    }
    assert(changed>0);
    printf("TC-518 %u M3 bend frames mean host ms bare=%.3f scenery=%.3f visibly_changed=%u\n",
           FRAME_COUNT,bare_ms/FRAME_COUNT,style_ms/FRAME_COUNT,changed);
    puts("TC-518 M3 scenery 100/50-row output identity and guards PASS");
}

static void m4_buffers(void)
{
    const r3_world_t *world=r3_m4_world();
    unsigned total=0,matched=0;
    static const float points[][3]={{0,0,6},{2,5,5.35f},{4,0,4.85f},
        {0,-1.05f,4.85f},{-2,0,4.85f},{-4.7f,-1.7f,4.425f},{-.35f,-3.4f,3.7f},
        {.8f,-3.4f,3.066f},{3.1f,-3.4f,2.3f},{3.1f,-1.3f,1.624f},{3.1f,1.5f,.8f}};
    double play_ms=0,bare_ms=0;
    r3_course_style_t plain=*world->style;plain.face_count=0;plain.faces=NULL;
    r3_world_t bare=*world;bare.style=&plain;
    for(unsigned k=0;k<sizeof(points)/sizeof(*points);++k){
        float u=points[k][0],v=points[k][1];
        r3_snapshot_t pose={.version=1,.radius=R3_RADIUS,.state=R3_RUNNING,.orientation={1,0,0,0},
            .position={-u*R3_M1_COS+v*R3_M1_SIN,u*R3_M1_SIN+v*R3_M1_COS,points[k][2]}};
        double start=time_ms();
        assert(roll3_renderer_render_play(baseline,regular+1,100,&pose,&bare,NULL)==0);
        bare_ms+=time_ms()-start;
        start=time_ms();
        assert(roll3_renderer_render_play(banded,regular+1,100,&pose,world,NULL)==0);
        play_ms+=time_ms()-start;
        assert(roll3_renderer_render_play(local+1,compact+1,50,&pose,world,NULL)==0);
        assert(memcmp(banded,local+1,sizeof(banded))==0);
        full_reference(&pose,world);
        assert(roll3_renderer_render_edges(local+1,regular+1,100,&pose,world,NULL,&edges)==0);
        memcpy(baseline,local+1,sizeof(baseline));
        assert(roll3_renderer_render_edges(local+1,compact+1,50,&pose,world,NULL,&edges)==0);
        assert(memcmp(baseline,local+1,sizeof(baseline))==0);
        for(unsigned y=0;y<R3_VIEW_H;++y)for(unsigned x=0;x<R3_VIEW_W;++x)
            if(edges.mask[y][x/32]&(1u<<(x%32))){++total;matched+=close_color(local[1+y*R3_VIEW_W+x],reference[y*R3_VIEW_W+x]);}
        check_guards();
        assert(regular[0]==0xdada&&regular[sizeof(regular)/sizeof(*regular)-1]==0x1cad);
        assert(compact[0]==0xdada&&compact[sizeof(compact)/sizeof(*compact)-1]==0x1cad);
    }
    assert(total&&100u*matched>=99u*total);
    printf("TC-528 M4 edge quality %u/%u, 100/50 rows and tail guards PASS; mean host play ms bare=%.3f scenery=%.3f\n",matched,total,bare_ms/11,play_ms/11);
}

int main(int argc,char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    (void)argv;
    const r3_world_t *world=r3_m1_world();
    local[0]=0x7be1;local[R3_VIEW_PIXELS+1]=0xc473;
    scratch[0]=0x174d;scratch[R3_VIEW_PIXELS+1]=0xfe18;
    unsigned candidates=0,matched=0,modified=0;
    regular[0]=compact[0]=0xdada;
    regular[sizeof(regular)/sizeof(*regular)-1]=0x1cad;
    compact[sizeof(compact)/sizeof(*compact)-1]=0x1cad;
    double center_ms=0,local_ms=0,full_ms=0,regular_ms=0,compact_ms=0,play_ms=0;
    for(unsigned frame=0;frame<FRAME_COUNT;++frame){
        r3_snapshot_t s=pose(frame);
        double start=time_ms();
        assert(roll3_renderer_render(baseline,full_depth,&s,world)==0);
        center_ms+=time_ms()-start;
        start=time_ms();
        assert(roll3_renderer_render_edges(local+1,scratch+1,R3_VIEW_H,&s,world,NULL,&edges)==0);
        local_ms+=time_ms()-start;
        start=time_ms();
        full_reference(&s,world);
        full_ms+=time_ms()-start;
        check_guards();
        for(unsigned y=0;y<R3_VIEW_H;++y){
            for(unsigned x=0;x<R3_VIEW_W;++x){
                unsigned i=y*R3_VIEW_W+x;
                bool edge_pixel=(edges.mask[y][x/32u]&(UINT32_C(1)<<(x%32u)))!=0;
                if(edge_pixel){
                    ++candidates;
                    matched+=close_color(local[i+1],reference[i]);
                    modified+=local[i+1]!=baseline[i];
                }else{
                    assert(local[i+1]==baseline[i]);
                }
            }
        }
        start=time_ms();
        assert(roll3_renderer_render_edges(banded,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,NULL,&edges)==0);
        regular_ms+=time_ms()-start;
        assert(memcmp(banded,local+1,sizeof(banded))==0);
        start=time_ms();
        assert(roll3_renderer_render_edges(banded,compact+1,R3_VIEW_LOW_DEPTH_ROWS,&s,world,NULL,&edges)==0);
        compact_ms+=time_ms()-start;
        assert(memcmp(banded,local+1,sizeof(banded))==0);
        assert(regular[0]==0xdada&&regular[sizeof(regular)/sizeof(*regular)-1]==0x1cad);
        assert(compact[0]==0xdada&&compact[sizeof(compact)/sizeof(*compact)-1]==0x1cad);
        start=time_ms();
        assert(roll3_renderer_render_edges(banded,regular+1,R3_VIEW_DEFAULT_DEPTH_ROWS,&s,world,NULL,NULL)==0);
        play_ms+=time_ms()-start;
        assert(memcmp(banded,baseline,sizeof(banded))==0);
        assert(regular[0]==0xdada&&regular[sizeof(regular)/sizeof(*regular)-1]==0x1cad);
        if(argc>1){
            save_ppm("center",frame,baseline);
            save_ppm("local",frame,local+1);
            save_ppm("full",frame,reference);
        }
    }
    printf("frames=%u candidates=%u coverage_match=%u changed=%u\n",
           FRAME_COUNT,candidates,matched,modified);
    assert(candidates>0&&modified>0&&100u*matched>=99u*candidates);

    /* Exercise every pixel across compact bands, including the partial bitmap word. */
    r3_snapshot_t s=pose(0);
    full_reference(&s,world);
    for(unsigned y=0;y<R3_VIEW_H;++y){
        for(unsigned word=0;word<R3_EDGE_WORDS;++word)edges.mask[y][word]=UINT32_MAX;
        edges.mask[y][R3_EDGE_WORDS-1]&=UINT32_MAX>>(32u-R3_VIEW_W%32u);
    }
    draw_edge_bands(local+1,compact+1,R3_VIEW_LOW_DEPTH_ROWS,&s,world,normalize(s.orientation),
                    roll3_renderer_focus(&s,world),&edges);
    check_guards();
    unsigned dense_match=0;
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i)dense_match+=close_color(local[i+1],reference[i]);
    printf("dense_match=%u/%u\n",dense_match,R3_VIEW_PIXELS);
    assert(100u*dense_match>=99u*R3_VIEW_PIXELS);

    memset(baseline,0,sizeof(baseline));
    build_edge_mask(baseline,&edges);
    draw_edge_bands(baseline,compact+1,R3_VIEW_LOW_DEPTH_ROWS,&s,world,normalize(s.orientation),
                    roll3_renderer_focus(&s,world),&edges);
    check_guards();
    for(unsigned i=0;i<R3_VIEW_PIXELS;++i)assert(baseline[i]==0);
    assert(compact[0]==0xdada&&compact[sizeof(compact)/sizeof(*compact)-1]==0x1cad);
    printf("mean_host_ms center=%.3f local_full_depth=%.3f full=%.3f rows100=%.3f rows50=%.3f\n",
           center_ms/FRAME_COUNT,local_ms/FRAME_COUNT,full_ms/FRAME_COUNT,
           regular_ms/FRAME_COUNT,compact_ms/FRAME_COUNT);
    printf("mean_host_ms play_center_rows100=%.3f\n",play_ms/FRAME_COUNT);
    puts("TC-512 play center bands match full-depth center output and guards PASS");
    puts("TC-510 100/50-row exact output, dense low-memory bands and scratch guards PASS");
    puts("TC-509 local coverage, unchanged interior, dense bands and guards PASS");
    m1_motion(world,argc>1);
    m2_curve_buffers();
    m3_style_buffers();
    m4_buffers();
    return 0;
}
