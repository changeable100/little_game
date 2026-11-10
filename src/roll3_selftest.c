#include "roll3_selftest.h"
#include "roll3_physics.h"
#include "roll3_scene.h"
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
static bool scene(void) {
    r3_game_t g;r3_snapshot_t s;
    CHECK(r3_world_validate(r3_demo_world())==R3_OK&&start(&g,r3_demo_world()));
    CHECK(r3_snapshot(&g,&s)==R3_OK&&s.version==1&&s.radius==R3_RADIUS);
    CHECK(r3_demo_world()->box_count==10&&r3_demo_world()->boxes[1].axis[0].z>0);
    CHECK(s.orientation.w==1&&s.position.z==R3_RADIUS);
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
    uint32_t run=g.s.run;g.s.position=(r3_vec_t){4.8f,1,-2.999f};g.s.velocity.z=-2;
    CHECK(steps(&g,&gravity,1)&&g.s.state==R3_FALLEN&&g.s.falls==1);
    uint32_t elapsed=g.s.elapsed_ms;
    CHECK(act(&g,R3_RESPAWN)==R3_OK&&g.s.run==run&&g.s.elapsed_ms==elapsed&&g.s.falls==1);
    CHECK(near(g.s.position.x,g.world->checkpoint_spawn.x,1e-6f)&&norm2(g.s.velocity)==0);
    CHECK(act(&g,R3_RESTART)==R3_OK&&g.s.checkpoint==0&&g.s.falls==0);
    return true;
}
static bool win(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));g.s.position=(r3_vec_t){10,4.2f,0.7f};
    CHECK(steps(&g,&gravity,1)&&g.s.state==R3_WON);
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
static bool ramp_deck_seam(void)
{
    const r3_box_t *b=r3_demo_world()->boxes;
    const r3_box_t *ramp=&b[1];
    float top_x=ramp->center.x+ramp->half.z*ramp->axis[2].x;
    float top_z=ramp->center.z+ramp->half.z*ramp->axis[2].z;
    float half_x=ramp->half.x*ramp->axis[0].x;
    float half_z=ramp->half.x*ramp->axis[0].z;
    return near(top_x-half_x,b[0].center.x+b[0].half.x,.0001f) &&
           near(top_z-half_z,b[0].center.z+b[0].half.z,.0001f) &&
           near(top_x+half_x,b[2].center.x-b[2].half.x,.0001f) &&
           near(top_z+half_z,b[2].center.z+b[2].half.z,.0001f) &&
           near(ramp->half.y,b[2].half.y,.0001f);
}
static bool demo_route(void) {
    r3_game_t g;CHECK(start(&g,r3_demo_world()));
    const r3_vec_t route[]={{3.5f,0,0},{6,0,0},{6,4.2f,0},{10,4.2f,0}};
    unsigned next=0;
    for(unsigned i=0;i<16000&&g.s.state==R3_RUNNING;i++) {
        r3_vec_t target=route[next];
        float dx=target.x-g.s.position.x,dy=target.y-g.s.position.y;
        if(next<3&&dx*dx+dy*dy<.04f&&norm2(g.s.velocity)<.16f)++next;
        r3_input_t input={{fmaxf(-4,fminf(4,3*dx-4*g.s.velocity.x+((g.s.position.x>1&&g.s.position.x<3)?2.4525f:0))),
                           fmaxf(-4,fminf(4,3*dy-4*g.s.velocity.y)),-9.81f},true};
        CHECK(steps(&g,&input,1)&&!g.s.limited);
    }
    CHECK(g.s.state==R3_WON&&g.s.checkpoint==1&&g.s.falls==0);
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
    RUN("TC-508 ramp-and-platform-share-top-edge",ramp_deck_seam());
    RUN("TC-505 continuous-ramp-two-turn-route",demo_route());
    RUN("TC-506 level-coast-stops-without-energy-gain",tuned_coast());
#undef RUN
    return failures;
}
