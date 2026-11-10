/* Deterministic production raster comparison. Run in a writable output folder.
 * Build with roll3_scene.c and roll3_physics.c; the include exposes band internals. */
#include "../src/roll3_renderer.c"
#include "roll3_scene.h"
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
        .position={u*R3_CAM_COS-v*R3_CAM_SIN,-u*R3_CAM_SIN-v*R3_CAM_COS,z+R3_RADIUS},
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
            .color=full_color[i],.depth=full_depth,
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

int main(int argc,char **argv)
{
    (void)argv;
    const r3_world_t *world=r3_demo_world();
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
    return 0;
}
