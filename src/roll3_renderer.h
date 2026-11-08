#ifndef ROLL3_RENDERER_H
#define ROLL3_RENDERER_H
#include "roll3_contract.h"
#define R3_VIEW_W 160
#define R3_VIEW_H 180
#define R3_VIEW_PIXELS (R3_VIEW_W * R3_VIEW_H)
/* RGB565 + uint16 depth: exactly 115200 caller-owned bytes. No allocation.
 * Camera (3,-8,6), target (3,0,.2), vertical FOV55, near .15, far30.
 * Screen right +world x, horizontal screen up +world y. */
int roll3_renderer_render(uint16_t *color, uint16_t *depth,
                         const r3_snapshot_t *snapshot, const r3_world_t *world);
bool roll3_renderer_project(r3_vec_t p, float *x, float *y, float *depth);
/* Allocation-free math selfcheck, callable by A's future startup integration.
 * Host and board use identical checks; B does not change main/selftest owner. */
int roll3_renderer_selftest(void (*report)(const char *, bool));
#endif
