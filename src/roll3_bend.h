#ifndef ROLL3_BEND_H
#define ROLL3_BEND_H

#include "roll3_contract.h"
#include <math.h>

#define R3_BEND_RAIL_WIDTH .12f
#define R3_BEND_DEPTH .34f

typedef struct {
    float height, radial_grade, angular_rise;
} r3_bend_profile_t;

/* sin(angle)^2 flattens both cross sections at the straight joins.
 * The derivatives are with respect to radius and angle, respectively. */
static inline r3_bend_profile_t r3_bend_profile(const r3_bend_t *bend,
    float cosine, float sine, float radius)
{
    float offset=radius-bend->radius;
    float cross_height=bend->bank*offset,cross_grade=bend->bank;
    if(bend->trough_depth>0){
        float lateral=offset/bend->half_width;
        cross_height+=bend->trough_depth*lateral*lateral;
        cross_grade+=2*bend->trough_depth*lateral/bend->half_width;
    }
    float weight=sine*sine,half_drop=.5f*(bend->exit_z-bend->origin.z);
    return (r3_bend_profile_t){
        bend->origin.z+half_drop*(1-cosine)+weight*cross_height,
        weight*cross_grade,half_drop*sine+2*sine*cosine*cross_height
    };
}

static inline float r3_bend_height(const r3_bend_t *bend, float cosine, float sine, float radius)
{
    return r3_bend_profile(bend,cosine,sine,radius).height;
}

static inline float r3_bend_longitudinal_scale(const r3_bend_t *bend)
{
    return bend->longitudinal_scale>0?bend->longitudinal_scale:1;
}

/* Keep each cross section the same width in world space when stretched. */
static inline float r3_bend_radial_scale(float scale, float sine)
{
    return sqrtf(1+(scale*scale-1)*sine*sine);
}

static inline r3_vec_t r3_bend_point(const r3_bend_t *bend, float cosine, float sine,
    float radius, float height)
{
    float scale=r3_bend_longitudinal_scale(bend);
    float along=sine*scale;
    float plan_radius=bend->radius+(radius-bend->radius)/r3_bend_radial_scale(scale,sine);
    return (r3_vec_t){
        bend->origin.x + plan_radius*(cosine*bend->radial_start.x + along*bend->sweep_axis.x),
        bend->origin.y + plan_radius*(cosine*bend->radial_start.y + along*bend->sweep_axis.y),
        height
    };
}

#endif
