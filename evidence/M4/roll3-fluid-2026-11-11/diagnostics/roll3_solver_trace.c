#include <stdio.h>
static int trace_solver;
#include "../build/physics_trace.c"
#include "roll3_scene.h"
int main(void)
{
    r3_game_t g;r3_init(&g,r3_demo_world());g.s.state=R3_RUNNING;
    const r3_box_t *b=&g.world->boxes[7];
    g.s.position=add(b->center,add(mul(b->axis[0],b->half.x-.25f),mul(b->axis[2],b->half.z+R3_RADIUS+2*SKIN)));
    g.s.velocity=add(mul(b->axis[0],7.5f*cosf(-.261799388f)),mul(b->axis[1],7.5f*sinf(-.261799388f)));g.s.omega=mul(cross(b->axis[2],g.s.velocity),1/R3_RADIUS);
    for(unsigned i=0;i<125;++i){
        trace_solver=i>=119;
        if(trace_solver)printf("STEP %u p=(%g,%g,%g) v=(%g,%g,%g)\n",i,g.s.position.x,g.s.position.y,g.s.position.z,g.s.velocity.x,g.s.velocity.y,g.s.velocity.z);
        r3_input_t in={{0,0,-9.81f},true};r3_step(&g,&in,5);
        if(g.s.limited){puts("LIMITED");break;}
    }
}
