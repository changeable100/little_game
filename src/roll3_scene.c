#include "roll3_scene.h"

/* Local coordinates: u across the course, v toward the first slope. */
#define X(u,v) ((u)*R3_M1_COS-(v)*R3_M1_SIN)
#define Y(u,v) (-(u)*R3_M1_SIN-(v)*R3_M1_COS)
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

const r3_world_t *r3_m1_world(void) {return &scene;}

/* The short intro ramp, long ramp, and curve use the same checker road
 * palette. The checkpoint sits on a flat landing before the curve. */
static const r3_box_t m2_boxes[] = {
    DECK(0,0,3.20f,1.25f,1,0x398fba),
    SLOPE(0,2.70f,2.95f,0,1,1.718284028f,1.25f,0.989359135f,0.145493990f,0x36b49b),
    DECK(2.175f,5.60f,2.70f,3.425f,1.20f,0x538cb9),
    SLOPE(4.35f,9.20f,2.125f,0,1,2.467919164f,1.25f,0.972479178f,0.232989803f,0x36b49b),
    DECK(4.35f,12.80f,1.55f,1.25f,1.20f,0xb78cca),
    SLOPE(4.35f,14.70f,1.40f,0,1,0.715891053f,1.25f,0.977802414f,0.209529089f,0x36b49b),
    SLOPE(.10f,14.70f,.97f,0,1,0.708590150f,1.25f,0.987877125f,0.155237834f,0x36b49b),
    DECK(.10f,12.90f,1.08f,1.25f,1.10f,0xd6ab46),
    DECK(.70f,6.75f,3.00f,1.95f,.06f,0xa071bc), /* Guide rail at first turn. */
};
_Static_assert(sizeof(m2_boxes)/sizeof(m2_boxes[0]) <= R3_MAX_BOXES, "M2 exceeds OBB budget");

static const r3_bend_t m2_bend = {
    .origin=LOCAL(2.225f,15.40f,1.25f),
    .radial_start=LOCAL(1,0,0),.sweep_axis=LOCAL(0,1,0),
    .radius=2.125f,.half_width=1.25f,.exit_z=.86f,
    .bank=.23f,.rail_height=.42f,.color=0x7c90cd
};

static const r3_world_t m2_scene = {
    .boxes=m2_boxes,.box_count=sizeof(m2_boxes)/sizeof(m2_boxes[0]),
    .spawn=LOCAL(0,0,3.20f+R3_RADIUS),
    .checkpoint_spawn=LOCAL(4.35f,12.80f,1.55f+R3_RADIUS),
    .checkpoint=ZONE(4.35f,12.80f,1.55f),.goal=ZONE(.10f,12.90f,1.08f),
    .fall_z=-3,.bend=&m2_bend
};

const r3_world_t *r3_m2_world(void) {return &m2_scene;}

/* M3: ten road pieces, two turn guides and three open-side goal guards. */
static const r3_box_t m3_boxes[] = {
    DECK(0,0,3.70f,1.33f,1,0xe399b7),
    SLOPE(0,2.70f,3.375f,0,1,1.730787393f,1.33f,0.982211915f,0.187775807f,0xe399b7),
    DECK(2.20f,5.85f,3.05f,3.53f,1.45f,0xe399b7),
    SLOPE(4.35f,8.45f,2.80f,0,1,1.176860230f,1.28f,0.977176364f,0.212429644f,0xe399b7),
    DECK(4.35f,10.30f,2.55f,1.28f,.70f,0xc48bce),
    SLOPE(4.35f,13.15f,2.05f,0,1,2.207374005f,1.28f,0.974008027f,0.226513495f,0xe399b7),
    DECK(4.35f,16.50f,1.55f,1.28f,1.20f,0xc48bce),
    SLOPE(4.35f,18.40f,1.40f,0,1,0.715891053f,.96f,0.977802414f,0.209529089f,0xe399b7),
    SLOPE(.10f,18.40f,.97f,0,1,0.708590150f,.96f,0.987877125f,0.155237834f,0xe399b7),
    DECK(.10f,16.60f,1.08f,1.28f,1.10f,0x8ac879),
    DECK(.65f,7.24f,3.35f,1.96f,.06f,0xb964d1),
    DECK(2.22f,5.76f,3.29f,1.03f,.07f,0xb964d1),
    DECK(-1.22f,16.60f,1.38f,.04f,1.10f,0x4a9f66),
    DECK(1.42f,16.60f,1.38f,.04f,1.10f,0x4a9f66),
    DECK(.10f,15.46f,1.38f,1.36f,.04f,0x4a9f66),
};
_Static_assert(sizeof(m3_boxes)/sizeof(*m3_boxes)<=R3_MAX_BOXES,"M3 exceeds OBB budget");

static const r3_bend_t m3_bend = {
    .origin=LOCAL(2.225f,19.10f,1.25f),
    .radial_start=LOCAL(1,0,0),.sweep_axis=LOCAL(0,1,0),
    .radius=2.125f,.half_width=.96f,.exit_z=.86f,
    .trough_depth=.32f,.longitudinal_scale=1.55f,.color=0xe399b7
};

#include "roll3_stage_art.h"

static const r3_world_t m3_scene = {
    .boxes=m3_boxes,.box_count=sizeof(m3_boxes)/sizeof(*m3_boxes),
    .spawn=LOCAL(0,0,3.70f+R3_RADIUS),
    .checkpoint_spawn=LOCAL(4.35f,16.50f,1.55f+R3_RADIUS),
    .checkpoint=ZONE(4.35f,16.50f,1.55f),.goal=ZONE(.10f,16.60f,1.08f),
    .fall_z=-3,.bend=&m3_bend,.style=&m3_style,.goal_box_plus_one=10
};

const r3_world_t *r3_demo_world(void) {return &m3_scene;}
