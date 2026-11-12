#ifndef ROLL3_SCENE_H
#define ROLL3_SCENE_H
#include "roll3_contract.h"

/* M1 course yaw (60 degrees), independent of the camera and tilt axes. */
#define R3_M1_COS .5f
#define R3_M1_SIN .8660254038f
const r3_world_t *r3_demo_world(void);
const r3_world_t *r3_m1_world(void);
const r3_world_t *r3_m2_world(void);
#endif
