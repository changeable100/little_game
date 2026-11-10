#include "roll3_scene.h"
#include "roll3_camera_basis.h"

/* Local coordinates: u right, v down. These rotate with the existing camera
 * basis, so the world geometry and the player's tilt directions agree. */
#define X(u,v) ((u)*R3_CAM_COS-(v)*R3_CAM_SIN)
#define Y(u,v) (-(u)*R3_CAM_SIN-(v)*R3_CAM_COS)
#define LOCAL(u,v,z) {X(u,v),Y(u,v),(z)}
#define TOP_HALF .15f
#define MATERIAL(color) .25f,.6f,.08f,(color)
#define DECK(u,v,z,hu,hv,color) \
    {LOCAL(u,v,(z)-TOP_HALF),{hu,hv,TOP_HALF}, \
     {LOCAL(1,0,0),LOCAL(0,-1,0),LOCAL(0,0,1)},MATERIAL(color)}
/* d=(du,dv) points downhill; the world XY perpendicular (-d.y,d.x)
 * corresponds to (dv,-du) in the screen-down local basis.
 * Top center minus half thickness along the upward normal is the box center. */
#define SLOPE(u,v,z,du,dv,half_length,half_width,c,s,color) \
    {LOCAL((u)-TOP_HALF*(s)*(du),(v)-TOP_HALF*(s)*(dv),(z)-TOP_HALF*(c)), \
     {half_length,half_width,TOP_HALF}, \
     {LOCAL((du)*(c),(dv)*(c),-(s)),LOCAL(dv,-(du),0),LOCAL((du)*(s),(dv)*(s),c)}, \
     MATERIAL(color)}

static const r3_box_t boxes[] = {
    DECK(0,0,1.50f,1.1f,1,0x398fba),                                /* S */
    SLOPE(0,2.75f,1.275f,0,1,1.7644050f,1.1f,.991835775f,.127521743f,0x36b49b), /* R1 */
    DECK(0,5.5f,1.05f,1.1f,1,0xb78cca),                             /* C1 */
    SLOPE(2.7f,5.5f,.825f,1,0,1.6157429f,1,.990256579f,.139254831f,0x398fba),  /* R2 */
    DECK(5.5f,5.5f,.60f,1.2f,1,0x398fba),                          /* C2 */
    SLOPE(5.5f,8.25f,.35f,0,1,1.7677670f,1.1f,.989949494f,.141421356f,0x36b49b), /* R3 */
    DECK(5.5f,11,.10f,1.1f,1,0xd6ab46)                             /* G */
};
_Static_assert(sizeof(boxes)/sizeof(boxes[0]) <= R3_MAX_BOXES, "M1 exceeds OBB budget");

/* Axis-aligned in world XY, not in local (u,v). A 0.42m half-extent
 * projects to less than 0.574m along either deck axis. */
#define ZONE(u,v,z) {{X(u,v)-.42f,Y(u,v)-.42f,(z)}, \
                      {X(u,v)+.42f,Y(u,v)+.42f,(z)+.60f}}
static const r3_world_t scene = {
    .boxes=boxes,.box_count=sizeof(boxes)/sizeof(boxes[0]),
    .spawn=LOCAL(0,0,1.50f+R3_RADIUS),
    .checkpoint_spawn=LOCAL(0,5.5f,1.05f+R3_RADIUS),
    .checkpoint=ZONE(0,5.5f,1.05f),.goal=ZONE(5.5f,11,.10f),.fall_z=-3
};

const r3_world_t *r3_demo_world(void) {return &scene;}
