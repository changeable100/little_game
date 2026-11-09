#ifndef ROLL3_PHYSICS_H
#define ROLL3_PHYSICS_H
#include "roll3_contract.h"

/* Private model fields, one owner only. No RT/LVGL/I2C or allocation here. */
typedef struct {
    const r3_world_t *world;
    r3_snapshot_t s;
    uint32_t last_request;
} r3_game_t;
r3_result_t r3_world_validate(const r3_world_t *world);
r3_result_t r3_init(r3_game_t *game, const r3_world_t *world);
r3_result_t r3_snapshot(const r3_game_t *game, r3_snapshot_t *out);
r3_result_t r3_action(r3_game_t *game, const r3_action_t *action);
/* Only real calibration owner may call in production; P1 tests use synthetic input. */
r3_result_t r3_calibrated(r3_game_t *game, uint32_t run);
r3_result_t r3_input_failed(r3_game_t *game, uint32_t run);
/* Exactly 5ms. Owner clock handles accumulation/catchup; no second clock here. */
r3_result_t r3_step(r3_game_t *game, const r3_input_t *input, uint32_t dt_ms);
#endif
