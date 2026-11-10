#ifndef ROLL3_RENDERER_H
#define ROLL3_RENDERER_H
#include "roll3_contract.h"
#define R3_VIEW_W 240
#define R3_VIEW_H 200
#define R3_VIEW_PIXELS (R3_VIEW_W * R3_VIEW_H)
#define R3_VIEW_BUFFER_BYTES (R3_VIEW_PIXELS * sizeof(uint16_t) * 2u)
#define R3_EDGE_WORDS ((R3_VIEW_W + 31) / 32)
typedef struct {
    uint32_t mask[R3_VIEW_H][R3_EDGE_WORDS];
    uint16_t offset[R3_VIEW_H][R3_EDGE_WORDS];
} r3_edge_aa_t;

#define R3_VIEW_COLOR_BYTES (R3_VIEW_PIXELS * sizeof(uint16_t))
#define R3_VIEW_DEFAULT_DEPTH_ROWS 100u
#define R3_VIEW_LOW_DEPTH_ROWS 50u
#define R3_VIEW_WORKSPACE_BYTES(rows) (sizeof(r3_edge_aa_t) + (rows) * R3_VIEW_W * sizeof(uint16_t))

/* Fixed oblique perspective, with RGB565 and depth planes. Optional AA scratch is caller-owned. */
typedef struct {
    r3_vec_t focus;
    uint32_t run, elapsed_ms;
    r3_state_t state;
    bool valid;
} r3_camera_follow_t;
/* View-owned follow state; rendering never changes the physical snapshot. */
int roll3_renderer_render_follow(uint16_t *color, uint16_t *depth,
    const r3_snapshot_t *snapshot, const r3_world_t *world, r3_camera_follow_t *follow, uint16_t *aa_color);
/* Color holds a full frame. Depth holds depth_rows scanlines and is reused
 * for edge coverage after the center pass. Pass NULL edge_aa for center sampling.
 * Only color is retained on return. */
int roll3_renderer_render_edges(uint16_t *color, uint16_t *depth, unsigned depth_rows,
    const r3_snapshot_t *snapshot, const r3_world_t *world, r3_camera_follow_t *follow,
    r3_edge_aa_t *edge_aa);
r3_vec_t roll3_renderer_focus(const r3_snapshot_t *snapshot, const r3_world_t *world);
bool roll3_renderer_project_focus(r3_vec_t p, r3_vec_t focus, float *x, float *y, float *depth);
int roll3_renderer_render(uint16_t *color, uint16_t *depth,
                         const r3_snapshot_t *snapshot, const r3_world_t *world);
bool roll3_renderer_project(r3_vec_t p, float *x, float *y, float *depth);
/* Allocation-free math selfcheck, callable by A's future startup integration.
 * Host and board use identical checks; B does not change main/selftest owner. */
int roll3_renderer_selftest(void (*report)(const char *, bool));
#endif
