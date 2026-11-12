/* Link with a prefixed, untouched m3-slim renderer for a direct A/B comparison. */
#include "roll3_renderer.h"
#include "roll3_scene.h"
#include "roll3_bend.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
int baseline_renderer_render_play(uint16_t *,uint16_t *,unsigned,const r3_snapshot_t *,const r3_world_t *,r3_camera_follow_t *);
int baseline_renderer_render_edges(uint16_t *,uint16_t *,unsigned,const r3_snapshot_t *,const r3_world_t *,r3_camera_follow_t *,r3_edge_aa_t *);
static uint16_t old_color[R3_VIEW_PIXELS],new_color[R3_VIEW_PIXELS],low_color[R3_VIEW_PIXELS];
static uint16_t old_depth[R3_VIEW_W*100+2],new_depth[R3_VIEW_W*100+2],low_depth[R3_VIEW_W*50+2];
static r3_vec_t uv(float u,float v,float z)
{return (r3_vec_t){u*R3_M1_COS-v*R3_M1_SIN,-u*R3_M1_SIN-v*R3_M1_COS,z};}
static double now(void){struct timespec ts;timespec_get(&ts,TIME_UTC);return ts.tv_sec*1000.0+ts.tv_nsec/1e6;}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    const r3_world_t *w=r3_demo_world();const r3_bend_t *b=w->bend;
    r3_vec_t p[]={w->spawn,uv(1.4f,5.09f,3.25f),uv(4.35f,10.3f,2.75f),w->checkpoint_spawn,
        r3_bend_point(b,0,1,b->radius,r3_bend_height(b,0,1,b->radius)+R3_RADIUS),uv(.1f,16.6f,1.28f)};
    double old_ms=0,new_ms=0;unsigned differences=0;
    new_depth[0]=low_depth[0]=0x4321;new_depth[240*100+1]=low_depth[240*50+1]=0x7654;
    for(unsigned frame=0;frame<42;++frame){
        r3_snapshot_t s={.version=1,.state=R3_RUNNING,.radius=R3_RADIUS,.orientation={1,0,0,0}};
        if(frame<6)s.position=p[frame];
        else{float t=3.141592654f*(frame-6)/35,c=cosf(t),si=sinf(t);
            s.position=r3_bend_point(b,c,si,b->radius,r3_bend_height(b,c,si,b->radius)+R3_RADIUS);}
        assert(baseline_renderer_render_play(old_color,old_depth+1,100,&s,w,NULL)==0);
        assert(roll3_renderer_render_play(new_color,new_depth+1,100,&s,w,NULL)==0);
        assert(roll3_renderer_render_play(low_color,low_depth+1,50,&s,w,NULL)==0);
        for(unsigned i=0;i<R3_VIEW_PIXELS;++i)differences+=old_color[i]!=new_color[i];
        assert(memcmp(low_color,new_color,sizeof(new_color))==0);
        assert(new_depth[0]==0x4321&&new_depth[240*100+1]==0x7654&&low_depth[0]==0x4321&&low_depth[240*50+1]==0x7654);
        if(frame<6){
            const unsigned rounds=40;
            double t=now();
            for(unsigned k=0;k<rounds;++k)assert(baseline_renderer_render_play(old_color,old_depth+1,100,&s,w,NULL)==0);
            double before=(now()-t)/rounds;t=now();
            for(unsigned k=0;k<rounds;++k)assert(roll3_renderer_render_play(new_color,new_depth+1,100,&s,w,NULL)==0);
            double after=(now()-t)/rounds;
            old_ms+=before;new_ms+=after;
            printf("M3 pose=%u host_ms before=%.4f after=%.4f ratio=%.3f\n",frame,before,after,after/before);
            t=now();
            for(unsigned k=0;k<rounds;++k)assert(baseline_renderer_render_edges(old_color,old_depth+1,100,&s,w,NULL,NULL)==0);
            double old_raw=(now()-t)/rounds;t=now();
            for(unsigned k=0;k<rounds;++k)assert(roll3_renderer_render_edges(new_color,new_depth+1,100,&s,w,NULL,NULL)==0);
            printf(" raw old=%.4f new=%.4f\n",old_raw,(now()-t)/rounds);
        }
    }
    printf("TC-521 direct A/B 42 frames changed_pixels=%u mean_ms before=%.4f after=%.4f ratio=%.3f\n",differences,old_ms/6,new_ms/6,new_ms/old_ms);
    assert(differences==0);assert(new_ms<=.75*old_ms);
    puts("TC-521 identical old/new and 100/50 rows, guards, >=25% faster host draw PASS");
}
