#include "roll3_level2.h"
#include "roll3_scene.h"
#include "roll3_mesh.h"
#include "roll3_physics.h"
#include <math.h>
#include <stdio.h>
static const r3_input_t gravity={{0,0,-9.81f},true};
#define CHECK(x) do{if(!(x)){printf("level2 check line %d FAIL\n",__LINE__);return false;}}while(0)
static r3_result_t action(r3_game_t *g,r3_action_kind_t kind){r3_action_t a={kind,g->s.run,g->last_request+1};return r3_action(g,&a);}
static bool start(r3_game_t *g){return r3_init(g,r3_level2_world())==R3_OK&&action(g,R3_START)==R3_OK&&r3_calibrated(g,g->s.run)==R3_OK;}
static bool scene(void){
 const r3_world_t *w=r3_level2_world();CHECK(w!=r3_m4_world()&&r3_world_validate(w)==R3_OK&&w->mesh&&w->checkpoint_count==2);
 r3_game_t g;CHECK(start(&g));for(unsigned i=0;i<100;++i)CHECK(r3_step(&g,&gravity,5)==R3_OK&&!g.s.limited&&g.s.state==R3_RUNNING);
 CHECK(fabsf(g.s.position.z-w->spawn.z)<.001f&&g.s.checkpoint==0);
 r3_world_t bad=*w;bad.checkpoint_count=3;CHECK(r3_world_validate(&bad)==R3_INVALID);return true;
}
static bool checkpoints(void){
 const r3_world_t *w=r3_level2_world();r3_game_t g;CHECK(start(&g));
 for(unsigned i=0;i<2;++i){g.s.position=w->checkpoints[i].spawn;g.s.velocity=(r3_vec_t){0,0,0};CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.checkpoint==i+1&&!g.s.limited);
  g.s.position.z=-3.1f;CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_FALLEN);CHECK(action(&g,R3_RESPAWN)==R3_OK);
  CHECK(fabsf(g.s.position.x-w->checkpoints[i].spawn.x)<.00001f&&fabsf(g.s.position.y-w->checkpoints[i].spawn.y)<.00001f&&fabsf(g.s.position.z-w->checkpoints[i].spawn.z)<.00001f);
 }
 return true;
}
static bool goal(void){
 const r3_world_t *w=r3_level2_world();r3_game_t g;CHECK(start(&g));
 g.s.position=(r3_vec_t){(w->goal.min.x+w->goal.max.x)*.5f,(w->goal.min.y+w->goal.max.y)*.5f,.8f};
 CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_WON&&!g.s.limited);uint32_t ms=g.s.elapsed_ms;
 CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.elapsed_ms==ms);
 CHECK(start(&g));g.s.position=(r3_vec_t){(w->goal.min.x+w->goal.max.x)*.5f,(w->goal.min.y+w->goal.max.y)*.5f,1.6f};
 CHECK(r3_step(&g,&gravity,5)==R3_OK&&g.s.state==R3_RUNNING);return true;
}
int r3_level2_selftest(void(*report)(const char*,bool)){
 unsigned failures=0;bool ok=scene();if(report)report("TC-533 current PC r4 mesh / BVH / stationary spawn",ok);failures+=!ok;
 ok=checkpoints();if(report)report("TC-533 both checkpoint contacts and indexed respawn",ok);failures+=!ok;
 ok=goal();if(report)report("TC-533 real green top wins / airborne excluded / score frozen",ok);failures+=!ok;return (int)failures;
}
