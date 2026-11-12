#ifndef ROLL3_CONTRACT_H
#define ROLL3_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

#define ROLL3_CONTRACT_VERSION 1u
#define R3_MAX_BOXES 16u
#define R3_MAX_ART_FACES 144u
#define R3_STEP_MS 5u
#define R3_RADIUS 0.20f
#define R3_MAX_SPEED 8.0f
#define R3_MAX_OMEGA 80.0f

/* SI units; right-handed world: x right, y forward, z up. */
typedef struct { float x, y, z; } r3_vec_t;
typedef struct { float w, x, y, z; } r3_quat_t;
typedef struct { r3_vec_t min, max; } r3_zone_t;
typedef struct {
    r3_vec_t center, half, axis[3]; /* Orthogonal unit axes; static OBB. */
    float restitution, friction, rolling_friction;
    uint32_t color; /* RGB888; renderer uses the exact same geometry. */
} r3_box_t;
typedef struct {
    r3_vec_t origin, radial_start, sweep_axis;
    float radius, half_width, exit_z, bank, rail_height;
    float trough_depth; /* Edge rise in metres at the middle of the bend. */
    float longitudinal_scale; /* Zero selects the original circular plan. */
    uint32_t color;
} r3_bend_t; /* Half-annulus; radial_start points to the entry. */

typedef struct {
    r3_vec_t vertices[4];
    uint32_t color;
} r3_art_face_t;

typedef struct {
    const r3_art_face_t *faces;
    unsigned face_count, solid_boxes_from;
    uint32_t background, light_tile;
    float skirt_z;
    uint32_t thin_road_mask;
} r3_course_style_t; /* Immutable scenery and palette, independent of collision. */

typedef struct {
    const r3_box_t *boxes; /* Immutable, borrowed for whole model lifetime. */
    uint32_t box_count;
    r3_vec_t spawn, checkpoint_spawn;
    r3_zone_t checkpoint, goal;
    float fall_z;
    const r3_bend_t *bend; /* Optional; follows the same immutable lifetime. */
    const r3_course_style_t *style;
    uint32_t goal_box_plus_one; /* Zero: legacy goal zone. Otherwise top contact wins. */
    bool finish_rolls; /* Lock score at WON but keep autonomous gravity motion. */
} r3_world_t;
typedef enum { R3_OK=0, R3_INVALID=-1, R3_WRONG_STATE=-2,
               R3_STALE=-3, R3_EXHAUSTED=-4 } r3_result_t;
typedef enum { R3_READY, R3_CALIBRATING, R3_RUNNING, R3_PAUSED,
               R3_FALLEN, R3_WON, R3_ERROR } r3_state_t;
typedef enum { R3_NO_ERROR, R3_INPUT_ERROR } r3_error_t;
typedef enum { R3_START, R3_PAUSE, R3_RESUME, R3_RESTART,
               R3_BACK, R3_RESPAWN } r3_action_kind_t;
typedef struct {
    r3_action_kind_t kind;
    uint32_t expected_run, request;
} r3_action_t;
typedef struct {
    uint32_t version, run, revision;
    r3_state_t state;
    r3_error_t error;
    r3_vec_t position, velocity, omega, contact_normal;
    r3_quat_t orientation;
    float radius;
    uint32_t elapsed_ms, falls, checkpoint, limited_steps;
    uint8_t contacts;
    bool grounded, limited, speed_limited;
} r3_snapshot_t;
typedef struct {
    r3_vec_t acceleration; /* Includes world gravity, m/s^2, norm <=20. */
    bool valid;
} r3_input_t;

#endif
