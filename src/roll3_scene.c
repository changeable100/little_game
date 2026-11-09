#include "roll3_scene.h"
#define BASIS {{1,0,0},{0,1,0},{0,0,1}}
static const r3_box_t boxes[]={
    {{0,0,-0.2f},{1.05f,1.35f,0.2f},BASIS,0.25f,0.6f,0.08f,0x398fba},
    /* Top surface: x=1..3, z=0..0.5; actual OBB, not a drawn fake slope. */
    {{2.03638f,0,0.10448f},{1.0307764f,1.1f,0.15f},
     {{0.9701425f,0,0.2425356f},{0,1,0},{-0.2425356f,0,0.9701425f}},0.25f,0.6f,0.08f,0x36b49b},
    {{3.5f,0,0.3f},{0.55f,1.1f,0.2f},BASIS,0.25f,0.6f,0.08f,0x398fba},
    {{4.675f,0,0.3f},{0.625f,0.65f,0.2f},BASIS,0.25f,0.6f,0.08f,0xd6ab46},
    {{6,0,0.3f},{0.7f,1.1f,0.2f},BASIS,0.25f,0.6f,0.08f,0x36b49b},
    {{3.5f,1.15f,0.8f},{0.55f,0.05f,0.3f},BASIS,0.35f,0.4f,0.08f,0xb78cca},
    /* Wide north turn then east to the finish, with a 0.5m deck height. */
    {{6,2.3f,0.3f},{0.7f,1.2f,0.2f},BASIS,0.25f,0.6f,0.08f,0x67b7ac},
    {{6,4.2f,0.3f},{1.1f,0.7f,0.2f},BASIS,0.25f,0.6f,0.08f,0x67b7ac},
    {{8.1f,4.2f,0.3f},{1.0f,0.7f,0.2f},BASIS,0.25f,0.6f,0.08f,0xd6ab46},
    {{10,4.2f,0.3f},{0.9f,1.1f,0.2f},BASIS,0.25f,0.6f,0.08f,0x75ba76}
};
static const r3_world_t scene={boxes,sizeof(boxes)/sizeof(boxes[0]),
    {0,0,R3_RADIUS},{3.45f,0,0.5f+R3_RADIUS},
    {{3.1f,-0.4f,0.5f},{3.9f,0.4f,1.3f}},
    {{9.7f,3.8f,0.5f},{10.4f,4.6f,1.3f}},-3};
const r3_world_t *r3_demo_world(void) {return &scene;}
