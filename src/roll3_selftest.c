#include "roll3_selftest.h"
#include "roll3_physics.h"
#include "roll3_scene.h"
#include "roll3_camera_basis.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define CHECK(e) do {if(!(e))return false;}while(0)
#define BASIS {{1,0,0},{0,1,0},{0,0,1}}
static const r3_input_t gravity={{0,0,-9.81f},true}, zero={{0,0,0},true};
static const r3_box_t floor_box={{0,0,-0.2f},{20,20,0.2f},BASIS,0,0.6f,0,0x123456};
static r3_world_t test_world(const r3_box_t *b,unsigned count) {
    r3_world_t w={b,count,{0,0,R3_RADIUS},{-3,-3,R3_RADIUS},
        {{-4,-4,0},{-2,-2,2}},{{9,9,0},{10,10,2}},-3};
    return w;
}
static r3_result_t act(r3_game_t *g,r3_action_kind_t kind) {
    r3_action_t a={kind,g->s.run,g->last_request+1};return r3_action(g,&a);
}
static bool start(r3_game_t *g,const r3_world_t *w) {
    return r3_init(g,w)==R3_OK&&act(g,R3_START)==R3_OK&&r3_calibrated(g,g->s.run)==R3_OK;
}
static bool steps(r3_game_t *g,const r3_input_t *in,unsigned n) {
    for(unsigned i=0;i<n;i++)if(r3_step(g,in,R3_STEP_MS)!=R3_OK)return false;
    return true;
}
static float norm2(r3_vec_t v) {return v.x*v.x+v.y*v.y+v.z*v.z;}
static float energy(const r3_game_t *g) {return 0.5f*norm2(g->s.velocity)+0.2f*R3_RADIUS*R3_RADIUS*norm2(g->s.omega);}
static bool near(float a,float b,float eps) {return fabsf(a-b)<=eps;}
static r3_vec_t from_uv(float u,float v,float z) {
    return (r3_vec_t){u*R3_CAM_COS-v*R3_CAM_SIN,-u*R3_CAM_SIN-v*R3_CAM_COS,z};
}
static r3_vec_t to_uv(r3_vec_t p) {
    return (r3_vec_t){p.x*R3_CAM_COS-p.y*R3_CAM_SIN,-p.x*R3_CAM_SIN-p.y*R3_CAM_COS,p.z};
}
static float projection(r3_vec_t v,r3_vec_t axis) {
    return v.x*axis.x+v.y*axis.y+v.z*axis.z;
}
static bool top_point(const r3_box_t *b,r3_vec_t p,unsigned boundaries) {
    p=(r3_vec_t){p.x-b->center.x,p.y-b->center.y,p.z-b->center.z};
    float x=fabsf(projection(p,b->axis[0])),y=fabsf(projection(p,b->axis[1]));
    float dx=fmaxf(0,x-b->half.x),dy=fmaxf(0,y-b->half.y);
    float ex=fabsf(x-b->half.x),ey=fabsf(y-b->half.y);
    if(boundaries==2){dx=ex;dy=ey;}
    if(boundaries==1){if(ex<=ey)dx=ex;else dy=ey;}
    float dz=projection(p,b->axis[2])-b->half.z;
    /* Each side is within 0.05mm of the same seam endpoint, hence the
     * two actual edges differ by at most 0.10mm. */
    float eps=boundaries==1?.00005f:.0001f;
    return dx*dx+dy*dy+dz*dz<=eps*eps;
}
static bool scene(void) {
    r3_game_t g;r3_snapshot_t s;
    CHECK(r3_world_validate(r3_demo_world())==R3_OK&&start(&g,r3_demo_world()));
    CHECK(r3_snapshot(&g,&s)==R3_OK&&s.version==1&&s.radius==R3_RADIUS);
    const r3_world_t *w=g.world;
    CHECK(w->box_count==7&&w->fall_z==-3);
    CHECK(s.orientation.w==1&&near(s.position.z,1.50f+R3_RADIUS,.0001f));
    CHECK(near(w->checkpoint_spawn.z,1.05f+R3_RADIUS,.0001f));
    CHECK(top_point(&w->boxes[0],(r3_vec_t){s.position.x,s.position.y,s.position.z-R3_RADIUS},0));
    CHECK(top_point(&w->boxes[2],(r3_vec_t){w->checkpoint_spawn.x,w->checkpoint_spawn.y,
                                         w->checkpoint_spawn.z-R3_RADIUS},0));
    for(unsigned i=1;i<6;i+=2)CHECK(w->boxes[i].axis[0].z<0);
    return true;
}
static bool invalid_world(void) {
    r3_box_t b=floor_box;r3_world_t w=test_world(&b,1);r3_game_t g;
    CHECK(r3_world_validate(NULL)==R3_INVALID&&r3_init(NULL,&w)==R3_INVALID);
    w.box_count=R3_MAX_BOXES+1;CHECK(r3_world_validate(&w)==R3_INVALID);w.box_count=1;
    b.half.x=0;CHECK(r3_init(&g,&w)==R3_INVALID);b=floor_box;
    b.axis[1]=b.axis[0];CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    b.axis[2].z=-1;CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    b.center.z=NAN;CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    b.restitution=1.1f;CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    b.friction=-1;CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    b.rolling_friction=INFINITY;CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    b.color=0x1000000u;CHECK(r3_world_validate(&w)==R3_INVALID);b=floor_box;
    w.spawn.z=0;CHECK(r3_world_validate(&w)==R3_INVALID);w=test_world(&b,1);
    w.checkpoint_spawn.x=0;CHECK(r3_world_validate(&w)==R3_INVALID);w=test_world(&b,1);
    w.goal.max.x=NAN;CHECK(r3_world_validate(&w)==R3_INVALID);
    return true;
}
static bool guards(void) {
    r3_game_t g,before;CHECK(start(&g,r3_demo_world()));before=g;
    r3_action_t a={R3_RESTART,g.s.run-1,g.last_request+1};
    CHECK(r3_action(&g,&a)==R3_STALE&&memcmp(&g,&before,sizeof(g))==0);
    a.expected_run=g.s.run;a.request=g.last_request;CHECK(r3_action(&g,&a)==R3_STALE);
    a.request=0;CHECK(r3_action(&g,&a)==R3_INVALID);
    CHECK(r3_step(NULL,&zero,5)==R3_INVALID&&r3_step(&g,NULL,5)==R3_INVALID);
    CHECK(r3_step(&g,&zero,4)==R3_INVALID&&memcmp(&g,&before,sizeof(g))==0);
    CHECK(r3_snapshot(NULL,&g.s)==R3_INVALID&&r3_snapshot(&g,NULL)==R3_INVALID);
    CHECK(r3_calibrated(&g,g.s.run-1)==R3_STALE);
    return true;
}
static bool lifecycle(void) {
    r3_game_t g;CHECK(r3_init(&g,r3_demo_world())==R3_OK);
    CHECK(r3_step(&g,&zero,5)==R3_WRONG_STATE);
    CHECK(act(&g,R3_START)==R3_OK&&g.s.state==R3_CALIBRATING);
    CHECK(r3_step(&g,&zero,5)==R3_WRONG_STATE&&r3_calibrated(&g,g.s.run)==R3_OK);
    g.s.velocity.x=1;CHECK(steps(&g,&gravity,10));uint32_t elapsed=g.s.elapsed_ms,run=g.s.run;
    CHECK(act(&g,R3_PAUSE)==R3_OK&&norm2(g.s.velocity)==0&&norm2(g.s.omega)==0);
    CHECK(r3_step(&g,&gravity,5)==R3_WRONG_STATE&&g.s.elapsed_ms==elapsed);
    CHECK(act(&g,R3_RESUME)==R3_OK&&g.s.run==run&&act(&g,R3_RESPAWN)==R3_WRONG_STATE);
    CHECK(act(&g,R3_RESTART)==R3_OK&&g.s.run==run+1&&g.s.elapsed_ms==0);
    CHECK(act(&g,R3_BACK)==R3_OK&&g.s.state==R3_READY);
    return true;
}
static bool stationary(void) {
    r3_game_t g;r3_box_t b=floor_box;r3_world_t w=test_world(&b,1);CHECK(start(&g,&w));
    CHECK(steps(&g,&gravity,2000));
    CHECK(near(g.s.position.z,R3_RADIUS,0.0001f)&&fabsf(g.s.position.x)<1e-6f);
    CHECK(energy(&g)<1e-7f&&g.s.grounded&&!g.s.limited&&g.s.elapsed_ms==10000);
    return true;
}
static bool flight(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));
    g.s.position=(r3_vec_t){0,0,2};g.s.velocity=(r3_vec_t){1,2,3};g.s.omega=(r3_vec_t){0,4,0};
    CHECK(steps(&g,&zero,1));
    CHECK(near(g.s.position.x,0.005f,1e-6f)&&near(g.s.position.y,0.01f,1e-6f)&&near(g.s.position.z,2.015f,1e-5f));
    CHECK(g.s.orientation.y>0&&g.s.contacts==0&&!g.s.grounded);
    return true;
}
static bool freefall(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));g.s.position.z=2;
    CHECK(steps(&g,&gravity,10));
    CHECK(near(g.s.velocity.z,-0.4905f,1e-5f)&&near(g.s.position.z,2-9.81f*0.000025f*55,1e-5f));
    return true;
}
static bool wall(bool thin) {
    r3_box_t b[]={floor_box,{{1,0,1},{thin?0.001f:0.05f,2,1},BASIS,1,0,0,0}};
    r3_world_t w=test_world(b,2);r3_game_t g;CHECK(start(&g,&w));
    g.s.velocity=(r3_vec_t){thin?8:6,thin?0:1,0};
    float initial=energy(&g),max_x=-100;
    for(unsigned i=0;i<40;i++) {CHECK(steps(&g,&zero,1));if(g.s.position.x>max_x)max_x=g.s.position.x;}
    CHECK(g.s.velocity.x<0&&max_x<=1-b[1].half.x-R3_RADIUS+0.0001f);
    CHECK(near(g.s.velocity.y,thin?0:1,1e-6f)&&near(energy(&g),initial,0.0002f));
    return true;
}
static bool rounded_contact(bool corner) {
    r3_box_t b[]={floor_box,{{1,1,1},{0.2f,0.2f,corner?0.2f:0.6f},BASIS,1,0,0,0}};
    r3_world_t w=test_world(b,2);r3_game_t g;CHECK(start(&g,&w));
    g.s.position=(r3_vec_t){corner?0.65f:0.6f,corner?0.65f:0.6f,corner?0.65f:1};
    g.s.velocity=(r3_vec_t){3,3,corner?3:0};
    CHECK(steps(&g,&zero,8));
    CHECK(g.s.velocity.x<0&&g.s.velocity.y<0);
    if(corner)CHECK(g.s.velocity.z<0);else CHECK(fabsf(g.s.velocity.z)<1e-6f);
    CHECK(!g.s.limited&&near(energy(&g),corner?13.5f:9,0.002f));
    return true;
}
static bool rotated(void) {
    const float c=0.70710678f;
    r3_box_t b[]={floor_box,{{2,0,2},{0.1f,2,2},{{c,0,c},{0,1,0},{-c,0,c}},1,0,0,0}};
    r3_world_t w=test_world(b,2);r3_game_t g;CHECK(start(&g,&w));
    g.s.position=(r3_vec_t){2-c*0.5f,0,2-c*0.5f};g.s.velocity=(r3_vec_t){c*6,0,c*6};
    CHECK(steps(&g,&zero,12));
    CHECK(near(g.s.velocity.x,-c*6,0.001f)&&near(g.s.velocity.z,-c*6,0.001f));
    return true;
}
static bool ramp(void) {
    const float c=0.9701425f,s=0.2425356f;
    r3_box_t b={{0,0,-0.2f},{10,2,0.2f},{{c,0,s},{0,1,0},{-s,0,c}},0,0.8f,0,0};
    r3_world_t w=test_world(&b,1);
    w.spawn=(r3_vec_t){-s*0.4f,0,-0.2f+c*0.4f};w.checkpoint_spawn=w.spawn;
    w.checkpoint=(r3_zone_t){{-1,-1,-1},{1,1,1}};
    r3_game_t g;CHECK(start(&g,&w)&&steps(&g,&gravity,200));
    CHECK(g.s.position.x<w.spawn.x-0.5f&&g.s.velocity.x<-0.5f&&g.s.omega.y<-1);
    CHECK(near(-s*g.s.position.x+c*(g.s.position.z+0.2f),0.4f,0.0005f)&&g.s.grounded);
    return true;
}
static bool no_slip(bool initial_slip) {
    r3_box_t b=floor_box;r3_world_t w=test_world(&b,1);r3_game_t g;CHECK(start(&g,&w));
    g.s.velocity.x=initial_slip?2:1;g.s.omega.y=initial_slip?0:5;
    CHECK(steps(&g,&gravity,200));
    CHECK(near(g.s.velocity.x-R3_RADIUS*g.s.omega.y,0,0.005f));
    CHECK(near(g.s.velocity.x,initial_slip?10.0f/7:1,0.01f));
    CHECK(near(energy(&g),initial_slip?10.0f/7:0.7f,0.02f));
    return true;
}
static bool rolling_resistance(void) {
    r3_box_t b=floor_box;b.rolling_friction=0.02f;r3_world_t w=test_world(&b,1);
    r3_game_t g;CHECK(start(&g,&w));g.s.velocity.x=1;g.s.omega.y=5;
    float last=energy(&g);
    for(unsigned i=0;i<200;i++) {CHECK(steps(&g,&gravity,1));float e=energy(&g);CHECK(e<=last+0.00001f);last=e;}
    CHECK(last<0.6f&&g.s.velocity.x>=0);return true;
}
static bool repeated_energy(void) {
    r3_box_t b[]={floor_box,{{2,0,1},{0.05f,2,1},BASIS,1,0,0,0},{{-2,0,1},{0.05f,2,1},BASIS,1,0,0,0}};
    r3_world_t w=test_world(b,3);r3_game_t g;CHECK(start(&g,&w));g.s.velocity.x=6;
    float initial=energy(&g);
    for(unsigned i=0;i<1000;i++) {CHECK(steps(&g,&zero,1));CHECK(near(energy(&g),initial,0.002f)&&fabsf(g.s.position.x)<1.751f);}
    return true;
}
static bool corner_stability(void) {
    r3_box_t b[]={floor_box,{{-0.5f,0,1},{0.05f,2,1},BASIS,0,0.4f,0,0},{{0,-0.5f,1},{2,0.05f,1},BASIS,0,0.4f,0,0}};
    r3_world_t w=test_world(b,3);w.checkpoint_spawn=w.spawn;
    w.checkpoint=(r3_zone_t){{-1,-1,0},{1,1,1}};
    r3_game_t g;CHECK(start(&g,&w));r3_input_t input={{-4,-4,-9.81f},true};
    CHECK(steps(&g,&input,1000));
    CHECK(g.s.position.x>=-0.2501f&&g.s.position.y>=-0.2501f&&g.s.position.z>=0.1999f&&!g.s.limited);
    return true;
}
static bool respawn(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));
    g.s.position=g.world->checkpoint_spawn;CHECK(steps(&g,&gravity,1)&&g.s.checkpoint==1);
    uint32_t run=g.s.run;g.s.position=from_uv(-2,5.5f,g.world->fall_z+.001f);g.s.velocity.z=-2;
    CHECK(steps(&g,&gravity,1)&&g.s.state==R3_FALLEN&&g.s.falls==1);
    uint32_t elapsed=g.s.elapsed_ms;
    CHECK(act(&g,R3_RESPAWN)==R3_OK&&g.s.run==run&&g.s.elapsed_ms==elapsed&&g.s.falls==1);
    CHECK(near(g.s.position.x,g.world->checkpoint_spawn.x,1e-6f)&&
          near(g.s.position.y,g.world->checkpoint_spawn.y,1e-6f)&&
          near(g.s.position.z,g.world->checkpoint_spawn.z,1e-6f)&&norm2(g.s.velocity)==0);
    CHECK(act(&g,R3_RESTART)==R3_OK&&g.s.checkpoint==0&&g.s.falls==0);
    return true;
}
static bool win(void) {
    /* Single transition only; demo_route exercises the continuous run. */
    r3_game_t g;CHECK(start(&g,r3_demo_world()));g.s.position=from_uv(5.5f,11,.10f+R3_RADIUS);
    CHECK(steps(&g,&gravity,1)&&g.s.state==R3_WON&&g.s.checkpoint==0);
    r3_snapshot_t before=g.s;CHECK(r3_step(&g,&gravity,5)==R3_WRONG_STATE&&memcmp(&before,&g.s,sizeof(before))==0);
    CHECK(act(&g,R3_RESTART)==R3_OK&&g.s.elapsed_ms==0);return true;
}
static bool invalid_input(void) {
    for(unsigned i=0;i<4;i++) {
        r3_game_t g;CHECK(start(&g,r3_demo_world()));r3_input_t input=gravity;
        if(i==0)input.valid=false;if(i==1)input.acceleration.x=NAN;
        if(i==2)input.acceleration.z=INFINITY;if(i==3)input.acceleration.x=21;
        CHECK(r3_step(&g,&input,5)==R3_INVALID&&g.s.state==R3_ERROR&&g.s.error==R3_INPUT_ERROR&&g.s.elapsed_ms==0);
    }
    return true;
}
static bool counters(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));g.s.elapsed_ms=UINT32_MAX-3;
    CHECK(steps(&g,&gravity,1)&&g.s.elapsed_ms==UINT32_MAX);
    g.s.revision=UINT32_MAX;r3_game_t before=g;
    CHECK(r3_step(&g,&gravity,5)==R3_EXHAUSTED&&memcmp(&before,&g,sizeof(g))==0);
    CHECK(act(&g,R3_RESTART)==R3_EXHAUSTED);
    g.s.revision=10;g.s.run=UINT32_MAX;CHECK(act(&g,R3_RESTART)==R3_EXHAUSTED);
    CHECK(start(&g,r3_demo_world()));g.s.falls=UINT32_MAX;g.s.position=(r3_vec_t){10,0,-3};g.s.velocity.z=-2;
    CHECK(steps(&g,&gravity,1)&&g.s.falls==UINT32_MAX&&g.s.state==R3_FALLEN);
    return true;
}
static bool quaternion(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));g.s.omega.z=40;
    CHECK(steps(&g,&zero,2000));r3_quat_t q=g.s.orientation;
    CHECK(near(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z,1,0.00001f)&&near(g.s.omega.z,40,0.00001f));
    return true;
}
static bool near_miss(void) {
    r3_box_t b[]={floor_box,{{1,1,1},{0.2f,0.2f,0.5f},BASIS,1,0,0,0}};
    r3_world_t w=test_world(b,2);r3_game_t g;CHECK(start(&g,&w));
    g.s.position=(r3_vec_t){0,0.64f,0.34f};g.s.velocity.x=6;
    CHECK(steps(&g,&zero,50)&&near(g.s.velocity.x,6,1e-5f)&&near(g.s.position.x,1.5f,0.0001f));
    return true;
}
static bool iteration_budget(void) {
    r3_box_t b[]={floor_box,{{1,0,1},{0.95f,2,1},BASIS,0,0,0,0},{{3,0,1},{0.95f,2,1},BASIS,0,0,0,0}};
    r3_world_t w=test_world(b,3);w.spawn.x=-2;r3_game_t g;CHECK(start(&g,&w));
    /* Deliberately invalid private placement tests bounded recovery, not a normal spawn. */
    g.s.position=(r3_vec_t){2,0,0.5f};g.s.velocity.x=1;
    CHECK(steps(&g,&zero,1)&&g.s.limited&&g.s.limited_steps==1&&norm2(g.s.velocity)==0);
    return true;
}
static bool speed_budget(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));
    g.s.position.z=2;g.s.velocity.x=100;g.s.omega.z=100;
    CHECK(steps(&g,&zero,1)&&g.s.speed_limited&&!g.s.limited);
    CHECK(near(g.s.velocity.x,8,1e-6f)&&near(g.s.omega.z,80,1e-6f));
    CHECK(near(g.s.position.x,0.04f,1e-6f));return true;
}
static bool tuned_coast(void) {
    r3_box_t b=floor_box;b.rolling_friction=r3_demo_world()->boxes[0].rolling_friction;
    r3_world_t w=test_world(&b,1);r3_game_t g;CHECK(start(&g,&w));
    g.s.velocity.x=1;g.s.omega.y=1/R3_RADIUS;
    float previous=energy(&g);
    for(unsigned i=0;i<500;++i){CHECK(steps(&g,&gravity,1));
        float current=energy(&g);CHECK(current<=previous+.00001f&&g.s.velocity.x>=-.00001f);
        previous=current;}
    CHECK(g.s.velocity.x<.08f&&g.s.position.x<1.5f&&g.s.state==R3_RUNNING);
    return true;
}
static bool deck_geometry(void)
{
    const r3_box_t *b=r3_demo_world()->boxes;
    /* u edges, v edges and high/low z, independent of the production box math. */
    static const float limits[7][6]={
        {-1.1f,1.1f,-1,1,1.50f,1.50f},{-1.1f,1.1f,1,4.5f,1.50f,1.05f},
        {-1.1f,1.1f,4.5f,6.5f,1.05f,1.05f},{1.1f,4.3f,4.5f,6.5f,1.05f,.60f},
        {4.3f,6.7f,4.5f,6.5f,.60f,.60f},{4.4f,6.6f,6.5f,10,.60f,.10f},
        {4.4f,6.6f,10,12,.10f,.10f}
    };
    for(unsigned i=0;i<7;++i)for(unsigned u=0;u<2;++u)for(unsigned v=0;v<2;++v){
        const float *a=limits[i];
        CHECK(top_point(&b[i],from_uv(a[u],a[2+v],a[4+(i==3?u:v)]),2));
    }
    return true;
}

static bool deck_seam(unsigned i)
{
    const r3_box_t *b=r3_demo_world()->boxes;
    static const float edges[6][5]={
        {-1.1f,1,1.1f,1,1.50f},{-1.1f,4.5f,1.1f,4.5f,1.05f},
        {1.1f,4.5f,1.1f,6.5f,1.05f},{4.3f,4.5f,4.3f,6.5f,.60f},
        {4.4f,6.5f,6.6f,6.5f,.60f},{4.4f,10,6.6f,10,.10f}
    };
    const float *e=edges[i];
    for(unsigned j=0;j<4;j+=2){
        r3_vec_t p=from_uv(e[j],e[j+1],e[4]);
        CHECK(top_point(&b[i],p,1)&&top_point(&b[i+1],p,1));
    }
    return true;
}

static bool safe_zones(void)
{
    const r3_world_t *w=r3_demo_world();
    for(unsigned i=0;i<2;++i){
        r3_zone_t z=i?w->goal:w->checkpoint;
        const r3_box_t *b=&w->boxes[i?6:2];
        for(unsigned x=0;x<2;++x)for(unsigned y=0;y<2;++y){
            r3_vec_t p={(x?z.max.x:z.min.x)-b->center.x,(y?z.max.y:z.min.y)-b->center.y,0};
            CHECK(fabsf(projection(p,b->axis[0]))+R3_RADIUS<=b->half.x);
            CHECK(fabsf(projection(p,b->axis[1]))+R3_RADIUS<=b->half.y);
        }
    }
    CHECK(w->goal.max.z<w->checkpoint.min.z);
    return true;
}

static float clamp_input(float x){return fmaxf(-4,fminf(4,x));}
static bool demo_route(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));
    /* Synthetic feedback only: no writes to model pose after spawning. Stop
     * near each turn center before steering along the next segment. */
    static const r3_vec_t route[]={{0,5.5f,0},{5.5f,5.5f,0},{5.5f,11,0}};
    unsigned next=0,seams=0;
    for(unsigned i=0;i<16000&&g.s.state==R3_RUNNING;i++) {
        r3_vec_t p=to_uv(g.s.position),v=to_uv(g.s.velocity);
        float du=route[next].x-p.x,dv=route[next].y-p.y;
        if(next<2&&du*du+dv*dv<.04f&&norm2(v)<.16f){
            CHECK(next?seams==4:(seams==2&&g.s.checkpoint==1));
            ++next;du=route[next].x-p.x;dv=route[next].y-p.y;
        }
        float cu=0,cv=0;
        if(next==0&&p.y>1&&p.y<4.5f)cv=9.81f*.45f/3.5f;
        if(next==1&&p.x>1.1f&&p.x<4.3f)cu=9.81f*.45f/3.2f;
        if(next==2&&p.y>6.5f&&p.y<10)cv=9.81f*.50f/3.5f;
        r3_input_t input={from_uv(clamp_input(3*du-4*v.x-cu),clamp_input(3*dv-4*v.y-cv),-9.81f),true};
        CHECK(steps(&g,&input,1)&&!g.s.limited&&!g.s.speed_limited);
        p=to_uv(g.s.position);
        if(seams==0&&p.y>=1){CHECK(fabsf(p.x)<.9f);++seams;}
        if(seams==1&&p.y>=4.5f){CHECK(fabsf(p.x)<.9f);++seams;}
        if(seams==2&&p.x>=1.1f){CHECK(next==1&&fabsf(p.y-5.5f)<.8f);++seams;}
        if(seams==3&&p.x>=4.3f){CHECK(fabsf(p.y-5.5f)<.8f);++seams;}
        if(seams==4&&p.y>=6.5f){CHECK(next==2&&fabsf(p.x-5.5f)<.9f);++seams;}
        if(seams==5&&p.y>=10){CHECK(fabsf(p.x-5.5f)<.9f);++seams;}
    }
    CHECK(seams==6&&next==2&&g.s.state==R3_WON&&g.s.checkpoint==1&&g.s.falls==0);
    return true;
}
int r3_selftest_run(maze_test_report_t report) {
    int failures=0;
#define RUN(name,expr) do {bool passed=(expr);if(!passed)++failures;if(report)report(name,passed);}while(0)
    RUN("TC-501 scene-xyz-contract",scene());
    RUN("TC-501 malformed-world",invalid_world());
    RUN("TC-501 stale-and-api-guards",guards());
    RUN("TC-501 calibration-pause-restart-back",lifecycle());
    RUN("TC-502 stationary-2000-steps",stationary());
    RUN("TC-502 xyz-freeflight-orientation",flight());
    RUN("TC-502 freefall-gravity",freefall());
    RUN("TC-502 elastic-face-tangent",wall(false));
    RUN("TC-502 thin-wall-8mps",wall(true));
    RUN("TC-502 rounded-edge",rounded_contact(false));
    RUN("TC-502 rounded-corner",rounded_contact(true));
    RUN("TC-502 rotated-box-normal",rotated());
    RUN("TC-502 ramp-natural-roll",ramp());
    RUN("TC-502 no-slip-preserves-energy",no_slip(false));
    RUN("TC-502 friction-spinup",no_slip(true));
    RUN("TC-502 rolling-resistance-dissipates",rolling_resistance());
    RUN("TC-502 repeated-elastic-energy",repeated_energy());
    RUN("TC-502 multi-contact-corner",corner_stability());
    RUN("TC-501 checkpoint-fall-respawn",respawn());
    RUN("TC-501 goal-freeze",win());
    RUN("TC-502 invalid-input",invalid_input());
    RUN("TC-501 counters-and-exhaustion",counters());
    RUN("TC-502 quaternion-normalization",quaternion());
    RUN("TC-502 rounded-box-near-miss",near_miss());
    RUN("TC-502 bounded-deep-contact",iteration_budget());
    RUN("TC-502 linear-angular-speed-budget",speed_budget());
    RUN("TC-513 seven-deck-top-corners",deck_geometry());
    RUN("TC-513 seam S-R1",deck_seam(0));
    RUN("TC-513 seam R1-C1",deck_seam(1));
    RUN("TC-513 seam C1-R2",deck_seam(2));
    RUN("TC-513 seam R2-C2",deck_seam(3));
    RUN("TC-513 seam C2-R3",deck_seam(4));
    RUN("TC-513 seam R3-G",deck_seam(5));
    RUN("TC-513 checkpoint-goal-clearance",safe_zones());
    RUN("TC-513 continuous-six-seam-checkpoint-route",demo_route());
    RUN("TC-506 level-coast-stops-without-energy-gain",tuned_coast());
#undef RUN
    return failures;
}
