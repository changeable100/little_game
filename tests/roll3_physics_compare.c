/* Compare one identical pre-stall state, not two diverging playthroughs. */
#include "roll3_physics.h"
#include "roll3_scene.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
r3_result_t baseline_r3_step(r3_game_t *,const r3_input_t *,uint32_t);
static double now(void){struct timespec ts;timespec_get(&ts,TIME_UTC);return ts.tv_sec*1000.0+ts.tv_nsec/1e6;}
static float speed(r3_vec_t v){return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);}
int main(void)
{
    r3_game_t before;assert(r3_init(&before,r3_demo_world())==R3_OK);before.s.state=R3_RUNNING;
    before.s.position=(r3_vec_t){-16.3191166f,-14.4330492f,1.44221234f};
    before.s.velocity=(r3_vec_t){-4.06930923f,-2.23306346f,.170459092f};
    before.s.omega=(r3_vec_t){11.2264566f,-20.4604053f,-.0320382901f};
    r3_game_t old_game=before,new_game=before;
    const r3_input_t gravity={{0,0,-9.81f},true};
    assert(baseline_r3_step(&old_game,&gravity,5)==R3_OK);
    assert(r3_step(&new_game,&gravity,5)==R3_OK);
    printf("TC-521 identical snapshot speed %.6f old=%.6f limited=%u new=%.6f limited=%u\n",
        speed(before.s.velocity),speed(old_game.s.velocity),old_game.s.limited,speed(new_game.s.velocity),new_game.s.limited);
    assert(old_game.s.limited&&speed(old_game.s.velocity)==0&&!new_game.s.limited&&speed(new_game.s.velocity)>4.6f);
    const unsigned rounds=20000;volatile unsigned sink=0;
    double t=now();
    for(unsigned k=0;k<rounds;++k){old_game=before;assert(baseline_r3_step(&old_game,&gravity,5)==R3_OK);sink+=old_game.s.contacts;}
    double old_ms=(now()-t)/rounds;t=now();
    for(unsigned k=0;k<rounds;++k){new_game=before;assert(r3_step(&new_game,&gravity,5)==R3_OK);sink+=new_game.s.contacts;}
    double new_ms=(now()-t)/rounds;
    printf("TC-521 same-state host physics us old=%.3f new=%.3f ratio=%.3f sink=%u; NOT board timing\n",old_ms*1000,new_ms*1000,new_ms/old_ms,sink);
}
