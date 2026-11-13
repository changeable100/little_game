#include "roll3_level2.h"
#include "roll3_scene.h"
#include "roll3_physics.h"
#include "roll3_mesh.h"
#include <math.h>
#include <stdio.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
static r3_vec_t world(float x,float y,float z){const float c=.953665f,s=.300871f;return (r3_vec_t){x*c-y*s,x*s+y*c,z};}
static r3_vec_t local(r3_vec_t p){const float c=.953665f,s=.300871f;return (r3_vec_t){p.x*c+p.y*s,-p.x*s+p.y*c,p.z};}
static void pose(FILE *f,const r3_snapshot_t *s){fprintf(f,"%u,%u,%u,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g\n",s->elapsed_ms,s->state,s->checkpoint,s->position.x,s->position.y,s->position.z,s->orientation.w,s->orientation.x,s->orientation.y,s->orientation.z);}
static void report(const char *s,bool ok){printf("%s %s\n",s,ok?"PASS":"FAIL");}
static void start(r3_game_t *g){assert(r3_init(g,r3_level2_world())==R3_OK);assert(r3_action(g,&(r3_action_t){R3_START,0,1})==R3_OK);assert(r3_calibrated(g,g->s.run)==R3_OK);}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);assert(r3_level2_selftest(report)==0);
 const float speeds[]={.5f,2,5,7.5f};
 for(unsigned i=0;i<4;++i)for(unsigned oblique=0;oblique<2;++oblique){r3_game_t g;start(&g);g.s.position=world(-6.22f,-1.2f,.8f);g.s.velocity=world(speeds[i],oblique?(.15f*speeds[i]):0,0);bool hit=false;
  for(unsigned k=0;k<100;++k){assert(r3_step(&g,&(r3_input_t){{0,0,0},true},5)==R3_OK&&!g.s.limited);r3_vec_t p=local(g.s.position),v=local(g.s.velocity);assert(p.x<=-6.2f+.0001f);if(v.x<=.000001f)hit=true;}
  if(!hit){r3_vec_t p=local(g.s.position),v=local(g.s.velocity);printf("WALL FAIL speed=%g oblique=%u x=%.9g vx=%.9g\n",speeds[i],oblique,p.x,v.x);}assert(hit);
 }
 puts("TC-533 3.8m relay wall CCD, four speeds straight/oblique PASS");
 r3_game_t g;start(&g);FILE *poses=fopen("build/level2-physics-poses.csv","w");assert(poses);pose(poses,&g.s);unsigned saved_cp=0;unsigned count=0,ticks=0,visited=0;const r3_vec_t *route=r3_level2_route(&count);
 for(unsigned at=1;at<count&&g.s.state!=R3_WON;++at){bool reached=false;
  for(unsigned i=0;i<12000;++i){
   float x=route[at].x-g.s.position.x,y=route[at].y-g.s.position.y,d=sqrtf(x*x+y*y);
   if(d<.15f&&fabsf(route[at].z-g.s.position.z)<.6f){reached=true;break;}
   float tx=route[at].x-route[at-1].x,ty=route[at].y-route[at-1].y,len=sqrtf(tx*tx+ty*ty);
   float along=len>1e-6f?(-x*tx-y*ty)/len:-1;
   if(along>=0&&d*d-along*along<.4f*.4f&&fabsf(route[at].z-g.s.position.z)<.6f){reached=true;break;}
   if(g.s.state==R3_WON){reached=true;break;}
   if(g.s.state!=R3_RUNNING||g.s.limited_steps)break;
   r3_vec_t n=g.s.contact_normal;float cx=0,cy=0;
   if(g.s.grounded&&n.z>.25f){cx=9.81f*n.x/n.z;cy=9.81f*n.y/n.z;}
   float ax=3*x-3*g.s.velocity.x-cx+1.4f*x/d,ay=3*y-3*g.s.velocity.y-cy+1.4f*y/d,norm=sqrtf(ax*ax+ay*ay);
   if(norm>4.9f){ax*=4.9f/norm;ay*=4.9f/norm;}
   r3_input_t input={{ax,ay,-9.81f},true};
   assert(r3_step(&g,&input,5)==R3_OK);++ticks;
   if(ticks%600==0||g.s.checkpoint!=saved_cp){pose(poses,&g.s);}
   if(g.s.checkpoint!=saved_cp){
    saved_cp=g.s.checkpoint;r3_game_t fork=g;r3_input_t off={world(0,4.9f,-9.81f),true};
    for(unsigned k=0;k<6000&&fork.s.state==R3_RUNNING;++k)assert(r3_step(&fork,&off,5)==R3_OK&&!fork.s.limited);
    assert(fork.s.state==R3_FALLEN);
    assert(r3_action(&fork,&(r3_action_t){R3_RESPAWN,fork.s.run,fork.last_request+1})==R3_OK);
    r3_vec_t expected=fork.world->checkpoints[saved_cp-1].spawn;
    assert(fabsf(fork.s.position.x-expected.x)<.00001f&&fabsf(fork.s.position.y-expected.y)<.00001f&&fabsf(fork.s.position.z-expected.z)<.00001f);
    for(unsigned k=0;k<100;++k)assert(r3_step(&fork,&(r3_input_t){{0,0,-9.81f},true},5)==R3_OK&&!fork.s.limited&&fork.s.state==R3_RUNNING);
    printf("TC-533 reached checkpoint %u forked input-driven fall / exact respawn PASS\n",saved_cp);
   }
   if(g.s.grounded){r3_mesh_hit_t h=r3_mesh_nearest(g.world->mesh,g.s.position,R3_RADIUS+.001f);if(h.found&&(h.flags&R3_MESH_FLOOR))visited|=h.flags;}
  }
  if(!reached){r3_vec_t p=local(g.s.position);printf("ROUTE FAIL at=%u target=(%.4f %.4f %.4f) actual=(%.4f %.4f %.4f) local=(%.4f %.4f %.4f) state=%u limited=%u cp=%u tick=%u\n",at,route[at].x,route[at].y,route[at].z,g.s.position.x,g.s.position.y,g.s.position.z,p.x,p.y,p.z,g.s.state,g.s.limited_steps,g.s.checkpoint,ticks);return 1;}
 }
 assert(g.s.state==R3_WON&&g.s.checkpoint==2&&g.s.falls==0&&g.s.limited_steps==0);
 assert((visited&(R3_MESH_CHUTE1|R3_MESH_CHUTE2))==(R3_MESH_CHUTE1|R3_MESH_CHUTE2));
 pose(poses,&g.s);fclose(poses);
 printf("TC-533 full current level2 only-input route PASS time=%ums checkpoints=%u falls=%u limited=%u\n",ticks*5,g.s.checkpoint,g.s.falls,g.s.limited_steps);return 0;
}
