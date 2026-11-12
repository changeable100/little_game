/* Diagnostic sweep of the unmodified production solver; no device access. */
#include "../src/roll3_physics.c"
#include "roll3_scene.h"
#include <stdio.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static r3_vec_t along(const r3_box_t *b,float x,float y,float z)
{return add(b->center,add(mul(b->axis[0],x),add(mul(b->axis[1],y),mul(b->axis[2],z))));}

int main(void)
{
    const r3_world_t *original=r3_demo_world();
    const float speeds[]={.5f,2,5,7.5f},angles[]={0,-.087266463f,.087266463f,-.261799388f,.261799388f};
    unsigned cases=0,stops=0,limited_cases=0,printed=0,isolated_stops=0;
    for(unsigned isolated=0;isolated<2;++isolated)
    for(unsigned index=1;index<=8;++index){
        const r3_box_t *b=&original->boxes[index];
        if(b->axis[0].z>=0)continue;
        for(unsigned edge_axis=0;edge_axis<2;++edge_axis)
        for(int side=-1;side<=1;side+=2)
        for(unsigned ai=0;ai<5;++ai)for(unsigned si=0;si<4;++si){
            r3_world_t w=*original;
            if(isolated){w.boxes=b;w.box_count=1;w.bend=NULL;w.style=NULL;}
            /* r3_init validates original spawn, so start before narrowing world. */
            r3_game_t g;
            if(r3_init(&g,original)!=R3_OK)return 2;
            g.world=&w;g.s.state=R3_RUNNING;
            float coordinate[2]={0,0};
            coordinate[edge_axis]=side*((edge_axis?b->half.y:b->half.x)-.25f);
            g.s.position=along(b,coordinate[0],coordinate[1],b->half.z+R3_RADIUS+2*SKIN);
            r3_vec_t dir=add(mul(b->axis[edge_axis],side*cosf(angles[ai])),mul(b->axis[1-edge_axis],sinf(angles[ai])));
            g.s.velocity=mul(dir,speeds[si]);g.s.omega=mul(cross(b->axis[2],g.s.velocity),1/R3_RADIUS);
            bool stopped=false,was_limited=false;
            for(unsigned step=0;step<160&&g.s.state==R3_RUNNING;++step){
                r3_snapshot_t before=g.s;
                r3_input_t in={{0,0,-9.81f},true};
                if(r3_step(&g,&in,R3_STEP_MS)!=R3_OK)return 3;
                float old_speed=length(before.velocity),speed=length(g.s.velocity);
                bool drop=g.s.state==R3_RUNNING&&old_speed>.3f&&speed<old_speed*.75f;
                stopped|=drop;was_limited|=g.s.limited;
                if((drop||g.s.limited)&&printed++<24)
                    printf("isolated=%u box=%u axis=%u side=%d angle=%g v0=%g step=%u speed=%g->%g limited=%u contacts=%u pos=(%.9g,%.9g,%.9g) before=(%.9g,%.9g,%.9g) velocity=(%.9g,%.9g,%.9g) omega=(%.9g,%.9g,%.9g)\n",
                        isolated,index,edge_axis,side,angles[ai],speeds[si],step,old_speed,speed,g.s.limited,g.s.contacts,
                        g.s.position.x,g.s.position.y,g.s.position.z,before.position.x,before.position.y,before.position.z,
                        before.velocity.x,before.velocity.y,before.velocity.z,before.omega.x,before.omega.y,before.omega.z);
                if(g.s.limited)break;
            }
            ++cases;stops+=stopped;limited_cases+=was_limited;isolated_stops+=isolated&&stopped;
        }
    }
    printf("edge probe cases=%u sudden_drop_cases=%u limited_cases=%u (includes real joins/obstacles in non-isolated sweep)\n",cases,stops,limited_cases);
    printf("TC-521 isolated open edges sudden_drop_cases=%u\n",isolated_stops);
    assert(limited_cases==0&&isolated_stops==0);
    return 0;
}
