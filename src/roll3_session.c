#include "roll3_session.h"
#include "maze_controller.h" /* Shared timing and calibration bounds. */
#include <math.h>
#include <string.h>

static bool newer(uint32_t value, uint32_t old)
{
    return value != old && (uint32_t)(value - old) < UINT32_C(0x80000000);
}

static bool fresh(const roll3_session_t *s, uint32_t now)
{
    return s->sequence && now - s->stamp <= MAZE_SAMPLE_MAX_AGE_MS;
}

bool roll3_session_needs_motion(const roll3_session_t *s)
{
    r3_state_t state = s->game.s.state;
    return state == R3_CALIBRATING || state == R3_RUNNING ||
           state == R3_PAUSED || state == R3_FALLEN;
}

r3_result_t roll3_session_init(roll3_session_t *s, const r3_world_t *world)
{
    memset(s, 0, sizeof(*s));
    return r3_init(&s->game, world);
}

r3_result_t roll3_session_action(roll3_session_t *s, r3_action_kind_t kind,
                                uint32_t run, uint32_t now)
{
    if (run != s->game.s.run) return R3_STALE;
    if (s->game.last_request == UINT32_MAX) return R3_EXHAUSTED;
    if ((kind == R3_START || kind == R3_RESTART) && run >= LAB_ROLL3_SESSION_BIT - 1)
        return R3_EXHAUSTED;
    if ((kind == R3_RESUME || kind == R3_RESPAWN) &&
        (!fresh(s, now) || (s->recovery && !newer(s->stamp, s->paused_at))))
        return R3_WRONG_STATE;

    const r3_action_t action = {kind, run, s->game.last_request + 1};
    r3_result_t result = r3_action(&s->game, &action);
    if (result != R3_OK) return result;
    s->anchor = now;
    if (kind == R3_START || kind == R3_RESTART || kind == R3_BACK) {
        r3_game_t game = s->game;
        memset(s, 0, sizeof(*s));
        s->game = game;
        s->started = s->anchor = now;
    } else if (kind == R3_RESUME) {
        s->recovery = false;
    }
    return R3_OK;
}

static float normalized(float value)
{
    float magnitude = (fabsf(value) - 0.03f) / (0.35f - 0.03f);
    return copysignf(fminf(1.0f, fmaxf(0.0f, magnitude)), value);
}

static void calibrate(roll3_session_t *s, const lab_motion_t *m, uint32_t now)
{
    float norm = m->a[0]*m->a[0] + m->a[1]*m->a[1] + m->a[2]*m->a[2];
    bool stable = norm >= 0.85f*0.85f && norm <= 1.15f*1.15f &&
        fabsf(m->a[0]) <= 0.25f && fabsf(m->a[1]) <= 0.25f && fabsf(m->a[2]) >= 0.85f;
    for (unsigned i = 0; i < 3; ++i) {
        if (fabsf(m->g[i]) > 5.0f ||
            (s->stable && fabsf(m->a[i] - s->previous[i]) > 0.04f)) stable = false;
    }
    if (!stable) {
        s->stable = 0;
        s->sum[0] = s->sum[1] = 0;
        return;
    }
    for (unsigned i = 0; i < 2; ++i) s->sum[i] += m->a[i];
    if (++s->stable == MAZE_CALIBRATION_SAMPLES) {
        for (unsigned i = 0; i < 2; ++i) s->bias[i] = s->sum[i] / s->stable;
        s->input = (r3_input_t){{0, 0, -9.81f}, true};
        s->anchor = now;
        (void)r3_calibrated(&s->game, s->game.s.run);
    }
}

static void consume(roll3_session_t *s, const lab_motion_t *m, uint32_t now)
{
    uint32_t run = s->game.s.run;
    if (!m || m->session != (run | LAB_ROLL3_SESSION_BIT) ||
        !newer(m->sampled_ms, s->started) || now - m->sampled_ms > MAZE_SAMPLE_MAX_AGE_MS)
        return;
    bool valid = m->valid && m->sequence;
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(m->a[i]) || !isfinite(m->g[i]) ||
            fabsf(m->a[i]) > 4.0f || fabsf(m->g[i]) > 300.0f) valid = false;
    }
    if (!valid || m->sequence < s->sequence ||
        (m->sequence > s->sequence && s->sequence && !newer(m->sampled_ms, s->stamp))) {
        (void)r3_input_failed(&s->game, run);
        return;
    }
    if (m->sequence == s->sequence) return;
    if (s->game.s.state == R3_CALIBRATING) {
        calibrate(s, m, now);
    } else {
        for (unsigned i = 0; i < 2; ++i)
            s->filtered[i] += 0.25f * (m->a[i] - s->bias[i] - s->filtered[i]);
        /* ADR-0007: right is +world x, screen up is +world y. */
        s->input.acceleration.x = 4.9f * normalized(s->filtered[1]);
        s->input.acceleration.y = 4.9f * normalized(s->filtered[0]);
    }
    memcpy(s->previous, m->a, sizeof(s->previous));
    s->sequence = m->sequence;
    s->stamp = m->sampled_ms;
}

void roll3_session_update(roll3_session_t *s, uint32_t now, const lab_motion_t *m,
                          bool ui_alive)
{
    uint32_t gap = now - s->anchor;
    s->anchor = now;
    r3_state_t before = s->game.s.state;
    if (!roll3_session_needs_motion(s)) return;
    if (before == R3_RUNNING && (!ui_alive || gap > ROLL3_CATCHUP_MS)) {
        if (roll3_session_action(s, R3_PAUSE, s->game.s.run, now) == R3_OK) {
            s->paused_at = now;
            s->recovery = true;
            s->remainder = 0;
        }
    }
    if (before == R3_CALIBRATING && now - s->started >= MAZE_CALIBRATION_TIMEOUT_MS) {
        (void)r3_input_failed(&s->game, s->game.s.run);
        return;
    }
    consume(s, m, now);
    if (before != R3_RUNNING || s->game.s.state != R3_RUNNING) return;
    if (!fresh(s, now)) {
        (void)r3_input_failed(&s->game, s->game.s.run);
        return;
    }
    s->remainder += gap;
    while (s->remainder >= R3_STEP_MS && s->game.s.state == R3_RUNNING) {
        if (r3_step(&s->game, &s->input, R3_STEP_MS) != R3_OK) break;
        s->remainder -= R3_STEP_MS;
    }
}
