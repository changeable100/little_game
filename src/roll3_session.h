#ifndef ROLL3_SESSION_H
#define ROLL3_SESSION_H

#include "lab_game.h"
#include "roll3_physics.h"

#define ROLL3_UI_TIMEOUT_MS 750u
#define ROLL3_CATCHUP_MS 250u

typedef struct {
    r3_game_t game;
    uint32_t anchor, started, sequence, stamp, remainder, paused_at;
    unsigned stable;
    float sum[2], bias[2], previous[3], filtered[2];
    r3_input_t input;
    bool recovery;
} roll3_session_t;

r3_result_t roll3_session_init(roll3_session_t *session, const r3_world_t *world);
r3_result_t roll3_session_action(roll3_session_t *session, r3_action_kind_t kind,
                                uint32_t expected_run, uint32_t now);
void roll3_session_update(roll3_session_t *session, uint32_t now,
                          const lab_motion_t *sample, bool ui_alive);
bool roll3_session_needs_motion(const roll3_session_t *session);

#endif
