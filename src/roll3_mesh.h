#ifndef ROLL3_MESH_H
#define ROLL3_MESH_H
#include "roll3_contract.h"
enum {R3_MESH_DRAW=1,R3_MESH_SOLID=2,R3_MESH_FLOOR=4,R3_MESH_GOAL=8,R3_MESH_CP1=16,R3_MESH_CP2=32,R3_MESH_CHUTE1=64,R3_MESH_CHUTE2=128};
typedef struct {uint16_t v[3],flags;uint32_t color;r3_vec_t normal;} r3_mesh_face_t;
typedef struct {r3_vec_t min,max;uint16_t first,count,next,pad;} r3_mesh_node_t;
typedef struct {r3_vec_t min,max;uint16_t first,count;} r3_mesh_group_t;
typedef struct r3_mesh {
    const r3_vec_t *vertices;
    const r3_mesh_face_t *faces;
    const r3_mesh_node_t *nodes;
    const uint16_t *collision_ids;
    const r3_mesh_group_t *groups;
    uint16_t vertex_count,face_count,node_count,collision_count,group_count;
} r3_mesh_t;
typedef struct {bool found;float gap,t;r3_vec_t normal;uint16_t flags;} r3_mesh_hit_t;
bool r3_mesh_validate(const r3_mesh_t *mesh);
r3_mesh_hit_t r3_mesh_nearest(const r3_mesh_t *mesh,r3_vec_t point,float reach);
r3_mesh_hit_t r3_mesh_sweep(const r3_mesh_t *mesh,r3_vec_t start,r3_vec_t delta,bool skip_floor);
bool r3_mesh_support_move(const r3_mesh_t *mesh,r3_vec_t start,r3_vec_t velocity,float *duration);
#endif
