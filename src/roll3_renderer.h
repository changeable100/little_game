#ifndef ROLL3_RENDERER_H
#define ROLL3_RENDERER_H
#include "roll3_contract.h"
#define R3_VIEW_W 180
#define R3_VIEW_H 150
#define R3_VIEW_PIXELS (R3_VIEW_W * R3_VIEW_H)
/* RGB565 + uint16 depth: 108000 caller-owned bytes, no allocation.
 * Fixed orientation, eye=focus+(0,-3.2,2.6), FOV55. Right +x, up +y. */
typedef struct {
    r3_vec_t focus;
    uint32_t run, elapsed_ms;
    r3_state_t state;
    bool valid;
} r3_camera_follow_t;
/* View-owned follow state; rendering never changes the physical snapshot. */
int roll3_renderer_render_follow(uint16_t *color, uint16_t *depth,
    const r3_snapshot_t *snapshot, const r3_world_t *world, r3_camera_follow_t *follow);
r3_vec_t roll3_renderer_focus(const r3_snapshot_t *snapshot, const r3_world_t *world);
bool roll3_renderer_project_focus(r3_vec_t p, r3_vec_t focus, float *x, float *y, float *depth);
int roll3_renderer_render(uint16_t *color, uint16_t *depth,
                         const r3_snapshot_t *snapshot, const r3_world_t *world);
bool roll3_renderer_project(r3_vec_t p, float *x, float *y, float *depth);
/* Allocation-free math selfcheck, callable by A's future startup integration.
 * Host and board use identical checks; B does not change main/selftest owner. */
int roll3_renderer_selftest(void (*report)(const char *, bool));
#endif
