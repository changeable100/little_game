#ifndef ROLL3_SESSION_H
#define ROLL3_SESSION_H

#include "lab_game.h"
#include "roll3_physics.h"

#define ROLL3_UI_TIMEOUT_MS 750u
#define ROLL3_CATCHUP_MS 250u
#define ROLL3_AUTO_RESPAWN_MS 600u

typedef enum {
    ROLL3_INPUT_FAULT_NONE,
    ROLL3_INPUT_FAULT_CAL_TIMEOUT,
    ROLL3_INPUT_FAULT_SAMPLE_TIMEOUT,
    ROLL3_INPUT_FAULT_INVALID_SAMPLE,
    ROLL3_INPUT_FAULT_SEQUENCE,
    ROLL3_INPUT_FAULT_OWNER_LOST,
    ROLL3_INPUT_FAULT_PHYSICS
} roll3_input_fault_t;

typedef struct {
    r3_game_t game;
    uint32_t anchor, started, sequence, stamp, remainder, paused_at, fallen_at;
    unsigned stable;
    float sum[2], bias[2], previous[3], filtered[2];
    r3_input_t input;
    bool recovery;
    roll3_input_fault_t input_fault;
} roll3_session_t;

r3_result_t roll3_session_init(roll3_session_t *session, const r3_world_t *world);
r3_result_t roll3_session_action(roll3_session_t *session, r3_action_kind_t kind,
                                uint32_t expected_run, uint32_t now);
void roll3_session_update(roll3_session_t *session, uint32_t now,
                          const lab_motion_t *sample, bool ui_alive);
/* Caller gives manual actions and closing the page priority over this. */
bool roll3_session_try_auto_respawn(roll3_session_t *session, uint32_t now);
void roll3_session_fail_input(roll3_session_t *session, roll3_input_fault_t fault);
bool roll3_session_needs_motion(const roll3_session_t *session);

#endif
