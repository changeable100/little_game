#ifndef ROLL3_BEND_H
#define ROLL3_BEND_H

#include "roll3_contract.h"

#define R3_BEND_RAIL_WIDTH .12f
#define R3_BEND_DEPTH .34f

/* sin(angle)^2 gives a flat cross section at both joins. */
static inline float r3_bend_height(const r3_bend_t *bend, float cosine, float sine, float radius)
{
    return bend->origin.z + (bend->exit_z - bend->origin.z) * (.5f - .5f*cosine)
        + bend->bank*sine*sine*(radius - bend->radius);
}

static inline r3_vec_t r3_bend_point(const r3_bend_t *bend, float cosine, float sine,
    float radius, float height)
{
    return (r3_vec_t){
        bend->origin.x + radius*(cosine*bend->radial_start.x + sine*bend->sweep_axis.x),
        bend->origin.y + radius*(cosine*bend->radial_start.y + sine*bend->sweep_axis.y),
        height
    };
}

#endif
