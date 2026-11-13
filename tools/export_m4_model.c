/* Host-only geometry export; does not initialize hardware or advance physics. */
#include "roll3_scene.h"
#include <stdio.h>
static void vec(r3_vec_t p){printf("[%.8g,%.8g,%.8g]",-p.x*R3_M1_COS+p.y*R3_M1_SIN,p.x*R3_M1_SIN+p.y*R3_M1_COS,p.z);}
int main(void){
 const r3_world_t *w=r3_m4_world();printf("{\"boxes\":[");
 for(unsigned i=0;i<w->box_count;i++){
  const r3_box_t *b=&w->boxes[i];if(i)printf(",");
  printf("{\"id\":%u,\"center\":",i);vec(b->center);printf(",\"half\":[%.8g,%.8g,%.8g],\"axes\":[",b->half.x,b->half.y,b->half.z);
  for(unsigned j=0;j<3;j++){if(j)printf(",");vec(b->axis[j]);}printf("],\"color\":%u}",b->color);
 }
 printf("],\"bends\":[");
 for(unsigned i=0;i<w->extra_bend_count;i++){
  const r3_bend_t *b=&w->extra_bends[i];if(i)printf(",");printf("{\"origin\":");vec(b->origin);
  printf(",\"radial\":");vec(b->radial_start);printf(",\"sweep\":");vec(b->sweep_axis);
  printf(",\"radius\":%.8g,\"width\":%.8g,\"exitZ\":%.8g,\"trough\":%.8g,\"color\":%u}",b->radius,b->half_width,b->exit_z,b->trough_depth,b->color);
 }
 printf("],\"art\":[");
 for(unsigned i=0;i<w->style->face_count;i++){if(i)printf(",");const r3_art_face_t *f=&w->style->faces[i];printf("{\"v\":[");for(unsigned j=0;j<4;j++){if(j)printf(",");vec(f->vertices[j]);}printf("],\"color\":%u}",f->color);}
 printf("],\"spawn\":");vec(w->spawn);printf(",\"checkpoint\":");vec(w->checkpoint_spawn);printf("}\n");return 0;
}
