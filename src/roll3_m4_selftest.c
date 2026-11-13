#include "roll3_scene.h"
#include "roll3_physics.h"
#include "roll3_bend.h"
#include "roll3_selftest.h"
#include <math.h>
#include <stdio.h>

#define CHECK(x) do{if(!(x)){printf("M4 check line %d failed\n",__LINE__);return false;}}while(0)
static r3_vec_t uv(float u,float v,float z){return (r3_vec_t){-u*R3_M1_COS+v*R3_M1_SIN,u*R3_M1_SIN+v*R3_M1_COS,z};}
static r3_vec_t local(r3_vec_t p){return (r3_vec_t){-p.x*R3_M1_COS+p.y*R3_M1_SIN,p.x*R3_M1_SIN+p.y*R3_M1_COS,p.z};}
static float dot(r3_vec_t a,r3_vec_t b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static r3_result_t action(r3_game_t *g,r3_action_kind_t k){r3_action_t a={k,g->s.run,g->last_request+1};return r3_action(g,&a);}
static bool start(r3_game_t *g){return r3_init(g,r3_m4_world())==R3_OK&&action(g,R3_START)==R3_OK&&r3_calibrated(g,g->s.run)==R3_OK;}
static bool geometry(void){
    const r3_world_t *w=r3_m4_world();r3_game_t g;
    CHECK(r3_world_validate(w)==R3_OK&&start(&g));
    CHECK(w->box_count==32&&w->extra_bend_count==2&&w->style->face_count<=144);
    float lo[2]={100,100},hi[2]={-100,-100};
    for(unsigned i=0;i<w->box_count;i++)for(unsigned j=0;j<8;j++){
        const r3_box_t *b=&w->boxes[i];r3_vec_t p=b->center;
        float h[]={b->half.x,b->half.y,b->half.z};
        for(unsigned k=0;k<3;k++){float t=(j&(1u<<k))?h[k]:-h[k];p.x+=b->axis[k].x*t;p.y+=b->axis[k].y*t;}
        p=local(p);lo[0]=fminf(lo[0],p.x);hi[0]=fmaxf(hi[0],p.x);lo[1]=fminf(lo[1],p.y);hi[1]=fmaxf(hi[1],p.y);
    }
    for(unsigned i=0;i<2;i++)for(unsigned j=0;j<=24;j++)for(int side=-1;side<=1;side+=2){
        const r3_bend_t *b=&w->extra_bends[i];float a=3.141592654f*j/24;
        r3_vec_t p=local(r3_bend_point(b,cosf(a),sinf(a),b->radius+side*b->half_width,0));
        lo[0]=fminf(lo[0],p.x);hi[0]=fmaxf(hi[0],p.x);lo[1]=fminf(lo[1],p.y);hi[1]=fmaxf(hi[1],p.y);
    }
    CHECK(hi[0]-lo[0]<=11&&hi[1]-lo[1]<=11);
    CHECK(fabsf(w->boxes[10].axis[0].x*w->boxes[14].axis[0].x+w->boxes[10].axis[0].y*w->boxes[14].axis[0].y)<.0001f);
    CHECK(fabsf(dot(w->extra_bends[0].radial_start,w->extra_bends[1].radial_start))<.0001f);
    CHECK(w->boxes[3].color==0xffc55c&&w->extra_bends[0].radius>w->extra_bends[1].radius);
    r3_world_t bad=*w;bad.extra_bends=NULL;CHECK(r3_world_validate(&bad)==R3_INVALID);
    bad=*w;bad.extra_bend_count=R3_MAX_EXTRA_BENDS+1;CHECK(r3_world_validate(&bad)==R3_INVALID);
    bad=*w;bad.box_count=31;bad.goal_boxes_mask=1u<<31;CHECK(r3_world_validate(&bad)==R3_INVALID);
    r3_input_t gravity={{0,0,-9.81f},true};
    for(unsigned i=0;i<200;i++)CHECK(r3_step(&g,&gravity,5)==R3_OK&&!g.s.limited);
    CHECK(fabsf(g.s.position.z-w->spawn.z)<.001f);
    g.s.checkpoint=1;g.s.position.z=-5;CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_FALLEN);
    CHECK(action(&g,R3_RESPAWN)==R3_OK);
    for(unsigned i=0;i<200;i++)CHECK(r3_step(&g,&gravity,5)==R3_OK&&!g.s.limited);
    CHECK(fabsf(g.s.position.z-w->checkpoint_spawn.z)<.001f);return true;
}
static bool seams(void){
    const r3_world_t *w=r3_m4_world();
    /* Independent authored end planes; checks whole width at each bend end. */
    for(unsigned i=0;i<2;i++)for(unsigned e=0;e<2;e++)for(int side=-1;side<=1;side+=2){
        const r3_bend_t *b=&w->extra_bends[i];
        r3_vec_t p=r3_bend_point(b,e?-1:1,0,b->radius+side*b->half_width,e?b->exit_z:b->origin.z);
        const r3_box_t *box=&w->boxes[i?(e?6:5):(e?2:1)];
        r3_vec_t d={p.x-box->center.x,p.y-box->center.y,p.z-box->center.z};
        CHECK(fabsf(dot(d,box->axis[2])-box->half.z)<.00005f);
        CHECK(fabsf(dot(d,box->axis[0]))<=box->half.x+.00005f);
        CHECK(fabsf(dot(d,box->axis[1]))<=box->half.y+.00005f);
    }
    /* Authored independent shared endpoints, including both chute outlets. */
    static const float joins[][9]={
        {0,1,-1,.6f,1,.6f,5.8f,0,0},{2,3,3,1,5,1,4.65f,0,0},
        {3,4,3,-.4f,3,0,4.65f,0,0},{4,5,-1,-1.4f,-1,-1.3f,4.65f,0,0},
        {6,7,-1,-4.2f,-1,-2.6f,3.5f,0,0},{7,10,0,-3.9f,0,-2.9f,3.5f,0,0},
        {10,11,1.6f,-3.9f,1.6f,-2.9f,2.1f,0,0},
        {11,14,2.6f,-2.1f,3.6f,-2.1f,2.1f,0,0},{14,15,2.6f,-.5f,3.6f,-.5f,.6f,0,0}};
    for(unsigned j=0;j<sizeof(joins)/sizeof(*joins);j++)for(unsigned side=0;side<2;side++)for(unsigned end=0;end<2;end++){
        const float *a=joins[j];const r3_box_t *b=&w->boxes[(unsigned)a[side]];
        r3_vec_t p=uv(a[2+2*end],a[3+2*end],a[6]);
        r3_vec_t d={p.x-b->center.x,p.y-b->center.y,p.z-b->center.z};
        CHECK(fabsf(dot(d,b->axis[2])-b->half.z)<.00005f);
        CHECK(fabsf(dot(d,b->axis[0]))<b->half.x+.00005f&&fabsf(dot(d,b->axis[1]))<b->half.y+.00005f);
    }
    return true;
}
static bool goals(void){
    const r3_world_t *w=r3_m4_world();r3_input_t gravity={{0,0,-9.81f},true};
    for(unsigned i=15;i<20;i++){
        r3_game_t g;CHECK(start(&g));const r3_box_t *b=&w->boxes[i];
        /* Sample the exposed deck beside the chute outlet, not through it. */
        r3_vec_t offset=uv(.8f,0,0);
        g.s.position=(r3_vec_t){b->center.x+offset.x,b->center.y+offset.y,.8f};
        CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_WON);
        CHECK(start(&g));g.s.position=(r3_vec_t){b->center.x,b->center.y,1.8f};
        CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_RUNNING);
        CHECK(start(&g));
        r3_vec_t edge=uv(b->half.x+R3_RADIUS,0,0);
        g.s.position=(r3_vec_t){b->center.x+edge.x,b->center.y+edge.y,.45f};
        CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_RUNNING);
    }
    return true;
}
static float clamp(float a){return fmaxf(-4,fminf(4,a));}
static bool pillars(void){
    const r3_world_t *w=r3_m4_world();
    const unsigned towers[]={0,5,7,8,9,11,12,13,15,16,17,18,19,29,30,31};
    for(unsigned i=0;i<sizeof(towers)/sizeof(*towers);i++){
        const r3_box_t *b=&w->boxes[towers[i]];
        CHECK(fabsf(b->center.z-b->half.z+2)<.00005f);
    }
    const unsigned cases[]={0,5,11,29,30,31};
    const float dirs[][2]={{1,0},{-1,0},{1,0},{1,0},{0,1},{-1,0}};
    const float speeds[]={.5f,2,5,7.5f};
    for(unsigned i=0;i<6;i++)for(unsigned j=0;j<4;j++)for(unsigned a=0;a<2;a++){
        r3_game_t g;CHECK(start(&g));const r3_box_t *b=&w->boxes[cases[i]];
        r3_vec_t c=local(b->center);float du=dirs[i][0],dv=dirs[i][1];
        float half=du?b->half.x:b->half.y;
        g.s.position=uv(c.x+du*(half+R3_RADIUS+.02f),c.y+dv*(half+R3_RADIUS+.02f),cases[i]==11?1:2);
        float side=a?.15f*speeds[j]:0;
        g.s.velocity=uv(-du*speeds[j]-dv*side,-dv*speeds[j]+du*side,0);
        r3_input_t zero={{0,0,0},true};bool hit=false;
        for(unsigned k=0;k<40;k++){
            CHECK(r3_step(&g,&zero,5)==R3_OK&&!g.s.limited);
            r3_vec_t p=local(g.s.position),v=local(g.s.velocity);
            float distance=du*(p.x-c.x)+dv*(p.y-c.y);
            CHECK(distance>=half+R3_RADIUS-.0001f);
            if(du*v.x+dv*v.y>=-.001f)hit=true;
        }
        CHECK(hit);
    }
    return true;
}
static bool fast_bends(void){
    const float speeds[]={.5f,2,5,7.5f};
    for(unsigned bi=0;bi<2;bi++)for(unsigned si=0;si<4;si++)for(int angle=-1;angle<=1;angle++){
        r3_game_t g;CHECK(start(&g));const r3_box_t *b=&g.world->boxes[bi?5:1];
        float a=angle*.261799388f;
        const float offsets[]={b->half.x-.25f,0,b->half.z+R3_RADIUS+.00002f};
        g.s.position=b->center;
        for(unsigned j=0;j<3;j++){
            g.s.position.x+=b->axis[j].x*offsets[j];
            g.s.position.y+=b->axis[j].y*offsets[j];
            g.s.position.z+=b->axis[j].z*offsets[j];
        }
        float forward=speeds[si]*cosf(a),side=speeds[si]*sinf(a);
        g.s.velocity=(r3_vec_t){b->axis[0].x*forward+b->axis[1].x*side,
            b->axis[0].y*forward+b->axis[1].y*side,b->axis[0].z*forward+b->axis[1].z*side};
        if(bi){g.s.position=uv(-2.75f,0,4.85002f);g.s.velocity=uv(-forward,side,0);}
        r3_vec_t n=b->axis[2],v=g.s.velocity;
        g.s.omega=(r3_vec_t){(n.y*v.z-n.z*v.y)/R3_RADIUS,(n.z*v.x-n.x*v.z)/R3_RADIUS,(n.x*v.y-n.y*v.x)/R3_RADIUS};
        r3_input_t gravity={{0,0,-9.81f},true};
        for(unsigned step=0;step<150;step++){
            float before=dot(g.s.velocity,g.s.velocity);
            CHECK(r3_step(&g,&gravity,5)==R3_OK&&!g.s.limited&&g.s.state==R3_RUNNING);
            CHECK(dot(g.s.velocity,g.s.velocity)>=before*.75f*.75f);
        }
    }
    return true;
}
static bool guards(void){
    const r3_world_t *w=r3_m4_world();
    const unsigned indices[]={20,21,22,23,24};
    const float directions[][2]={{0,-1},{1,0},{-1,0},{1,0},{0,1}};
    const float speeds[]={.5f,2,5,7.5f};
    for(unsigned k=0;k<5;k++)for(unsigned speed=0;speed<4;speed++)for(unsigned angle=0;angle<2;angle++){
        r3_game_t g;CHECK(start(&g));r3_world_t test=*w;
        test.goal_box_plus_one=0;test.goal_boxes_mask=0;
        test.goal=(r3_zone_t){{90,90,90},{91,91,91}};g.world=&test;
        const r3_box_t *b=&w->boxes[indices[k]];
        r3_vec_t c=local(b->center);
        float du=directions[k][0],dv=directions[k][1];
        r3_vec_t p={c.x-du*(.06f+R3_RADIUS+.02f),c.y-dv*(.06f+R3_RADIUS+.02f),k<2?2.3f:.8f};
        g.s.position=uv(p.x,p.y,p.z);
        g.s.velocity=uv(du*speeds[speed]-dv*(angle?.15f*speeds[speed]:0),dv*speeds[speed]+du*(angle?.15f*speeds[speed]:0),0);
        bool hit=false;
        r3_input_t gravity={{0,0,-9.81f},true};
        for(unsigned i=0;i<40;i++){
            CHECK(r3_step(&g,&gravity,5)==R3_OK&&!g.s.limited);
            r3_vec_t at=local(g.s.position),velocity=local(g.s.velocity);
            CHECK(du*(at.x-c.x)+dv*(at.y-c.y)<-.06f);
            float outward=du*velocity.x+dv*velocity.y;
            float gap=du*(c.x-at.x)+dv*(c.y-at.y)-.06f;
            /* A later floor contact may replace contact_normal in the same step. */
            if(outward<-.001f||(outward<.001f&&fabsf(gap-R3_RADIUS)<.005f))hit=true;
        }
        CHECK(hit);
    }
    return true;
}
static bool travel(r3_game_t *g,r3_vec_t target,unsigned *ticks){
    for(unsigned i=0;i<12000;i++){
        r3_vec_t p=local(g->s.position),v=local(g->s.velocity);
        float du=target.x-p.x,dv=target.y-p.y;
        if(du*du+dv*dv<.035f&&fabsf(p.z-target.z)<.55f)return true;
        if(g->s.state==R3_WON)return true;
        if(g->s.state!=R3_RUNNING)return false;
        r3_vec_t n=local(g->s.contact_normal);
        float cu=0,cv=0;
        if(g->s.grounded&&n.z>.25f){cu=9.81f*n.x/n.z;cv=9.81f*n.y/n.z;}
        float distance=sqrtf(du*du+dv*dv);
        r3_input_t input={uv(clamp(3*du-3*v.x-cu+1.4f*du/distance),clamp(3*dv-3*v.y-cv+1.4f*dv/distance),-9.81f),true};
        if(r3_step(g,&input,5)!=R3_OK||g->s.limited)return false;
        ++*ticks;
    }return false;
}
static bool route(void){
    r3_game_t g;CHECK(start(&g));unsigned ticks=0;
#define GO(uu,vv,zz) do{if(!travel(&g,(r3_vec_t){uu,vv,zz},&ticks)){printf("M4 route fail target=(%.2f %.2f %.2f) pose=(%.3f %.3f %.3f) state=%d limited=%u\n",(double)(uu),(double)(vv),(double)(zz),(double)local(g.s.position).x,(double)local(g.s.position).y,(double)g.s.position.z,g.s.state,g.s.limited_steps);return false;}}while(0)
    GO(0,2.9f,5.575f);
    for(unsigned b=0;b<2;b++){
        if(b){GO(1,-1,4.85f);GO(-2,-.7f,4.85f);GO(-2,0,4.85f);GO(-2.9f,0,4.85f);}
        const r3_bend_t *bend=&r3_m4_world()->extra_bends[b];
        for(unsigned i=1;i<=18;i++){
            float a=3.14159265359f*i/18;
            r3_vec_t p=local(r3_bend_point(bend,cosf(a),sinf(a),bend->radius,r3_bend_height(bend,cosf(a),sinf(a),bend->radius)+R3_RADIUS));
            GO(p.x,p.y,p.z);
        }
        if(!b){
            GO(4,0,4.85f);CHECK(g.s.checkpoint==1);
            /* Fork from the actually reached save state. Drive off the open
             * side using input only, then verify the real respawn action. */
            r3_game_t fallen=g;r3_input_t off={uv(-4,0,-9.81f),true};
            unsigned fork_ticks=0;
            CHECK(travel(&fallen,(r3_vec_t){1,-1,4.85f},&fork_ticks));
            CHECK(travel(&fallen,(r3_vec_t){-2,-.7f,4.85f},&fork_ticks));
            for(unsigned i=0;i<2000&&fallen.s.state==R3_RUNNING;i++)CHECK(r3_step(&fallen,&off,5)==R3_OK&&!fallen.s.limited);
            CHECK(fallen.s.state==R3_FALLEN&&action(&fallen,R3_RESPAWN)==R3_OK);
            r3_vec_t cp=local(fallen.s.position);CHECK(fabsf(cp.x-4)<.001f&&fabsf(cp.y)<.001f&&fabsf(cp.z-4.85f)<.001f);
        }
    }
    GO(-.35f,-3.4f,3.7f);GO(.8f,-3.4f,3.066f);GO(2.2f,-3.4f,2.3f);
    GO(3.1f,-3.4f,2.3f);GO(3.1f,-1.3f,1.624f);GO(3.1f,.7f,.8f);
    CHECK(g.s.state==R3_WON&&g.s.falls==0&&g.s.limited_steps==0);
    printf("M4 continuous input route: time=%ums falls=%u limited=%u checkpoint=%u\n",ticks*5,g.s.falls,g.s.limited_steps,g.s.checkpoint);
#undef GO
    return true;
}
int r3_m4_selftest(r3_test_report_t report){
    int failures=0;
#define RUN(n,f) do{bool ok=(f);if(!ok)failures++;if(report)report(n,ok);}while(0)
    RUN("TC-528 M4 validation spawn and checkpoint",geometry());
    RUN("TC-528 both curved seam endpoints within 0.1mm",seams());
    RUN("TC-528 five goal decks top contact and airborne guards",goals());
    RUN("TC-528 five real guards at four speeds straight and oblique",guards());
    RUN("TC-528 both bends four speeds and three approach angles",fast_bends());
    RUN("TC-528 solid tower geometry and 48 wall impacts",pillars());
    RUN("TC-528 continuous two bends two chutes checkpoint goal",route());
    return failures;
}
