#include "roll3_session.h"
#include "roll3_camera_basis.h"
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

void roll3_session_fail_input(roll3_session_t *s, roll3_input_fault_t fault)
{
    if (r3_input_failed(&s->game, s->game.s.run) == R3_OK)
        s->input_fault = fault;
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
    } else if (kind == R3_RESPAWN) {
        s->fallen_at = 0;
        s->remainder = 0;
    }
    return R3_OK;
}

bool roll3_session_try_auto_respawn(roll3_session_t *s, uint32_t now)
{
    if (s->game.s.state != R3_FALLEN || now - s->fallen_at < ROLL3_AUTO_RESPAWN_MS)
        return false;
    return roll3_session_action(s,R3_RESPAWN,s->game.s.run,now) == R3_OK;
}

/* Gameplay limits, independent of the core world's gravity and contact model. */
#define ROLL3_TILT_MAX_ACCEL 4.9f
#define ROLL3_NEUTRAL_BRAKE_GAIN 3.0f
#define ROLL3_NEUTRAL_BRAKE_MAX 2.0f
#define ROLL3_NEUTRAL_FLAT_Z .995f

static float normalized(float value)
{
    float magnitude = (fabsf(value) - 0.03f) / (0.45f - 0.03f);
    float u = fminf(1.0f, fmaxf(0.0f, magnitude));
    /* Reduce the middle of the curve while keeping both endpoints. */
    float middle = .26f * u * u * (1-u);
    return copysignf(u * (.43f + .57f * u - middle), value);
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
    if (!valid) {
        roll3_session_fail_input(s, ROLL3_INPUT_FAULT_INVALID_SAMPLE);
        return;
    }
    if (m->sequence < s->sequence ||
        (m->sequence > s->sequence && s->sequence && !newer(m->sampled_ms, s->stamp))) {
        roll3_session_fail_input(s, ROLL3_INPUT_FAULT_SEQUENCE);
        return;
    }
    if (m->sequence == s->sequence) return;
    if (s->game.s.state == R3_CALIBRATING) {
        calibrate(s, m, now);
    } else {
        for (unsigned i = 0; i < 2; ++i) {
            float tilt = m->a[i] - s->bias[i];
            if (fabsf(tilt) <= .02f) {
                s->filtered[i] = 0;
            } else {
                float delta = tilt - s->filtered[i];
                float gain = .25f;
                if (fabsf(delta) >= .02f)
                    gain = tilt * s->filtered[i] < 0 ? .75f : .65f;
                s->filtered[i] = fabsf(delta)<1e-6f ? tilt : s->filtered[i]+gain*delta;
            }
        }
        /* ADR-0009: map screen-right and screen-up into the ground plane. */
        float right = ROLL3_TILT_MAX_ACCEL * normalized(s->filtered[1]);
        float forward = ROLL3_TILT_MAX_ACCEL * normalized(s->filtered[0]);
        float length_squared = right*right + forward*forward;
        if (length_squared > ROLL3_TILT_MAX_ACCEL*ROLL3_TILT_MAX_ACCEL) {
            float scale = ROLL3_TILT_MAX_ACCEL / sqrtf(length_squared);
            right *= scale;
            forward *= scale;
        }
        s->input.acceleration.x = R3_CAM_COS * right + R3_CAM_SIN * forward;
        s->input.acceleration.y = -R3_CAM_SIN * right + R3_CAM_COS * forward;
    }
    memcpy(s->previous, m->a, sizeof(s->previous));
    s->sequence = m->sequence;
    s->stamp = m->sampled_ms;
}

static r3_input_t input_for_step(const roll3_session_t *s)
{
    r3_input_t input = s->input;
    const r3_snapshot_t *ball = &s->game.s;
    if (input.acceleration.x != 0 || input.acceleration.y != 0 ||
        !ball->grounded || ball->contact_normal.z < ROLL3_NEUTRAL_FLAT_Z)
        return input;

    float vx = ball->velocity.x, vy = ball->velocity.y;
    float speed_squared = vx*vx + vy*vy;
    float gain = ROLL3_NEUTRAL_BRAKE_GAIN;
    const float capped_speed = ROLL3_NEUTRAL_BRAKE_MAX / ROLL3_NEUTRAL_BRAKE_GAIN;
    if (speed_squared > capped_speed*capped_speed)
        gain = ROLL3_NEUTRAL_BRAKE_MAX / sqrtf(speed_squared);

    /* At the 5 ms fixed step, proportional braking removes at most 1.5%
     * of velocity per step. The capped term can only remove less. */
    input.acceleration.x = -gain*vx;
    input.acceleration.y = -gain*vy;
    return input;
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
        roll3_session_fail_input(s, ROLL3_INPUT_FAULT_CAL_TIMEOUT);
        return;
    }
    consume(s, m, now);
    if (before != R3_RUNNING || s->game.s.state != R3_RUNNING) return;
    if (!fresh(s, now)) {
        roll3_session_fail_input(s, ROLL3_INPUT_FAULT_SAMPLE_TIMEOUT);
        return;
    }
    s->remainder += gap;
    while (s->remainder >= R3_STEP_MS && s->game.s.state == R3_RUNNING) {
        r3_input_t input = input_for_step(s);
        if (r3_step(&s->game, &input, R3_STEP_MS) != R3_OK) {
            if (s->game.s.state == R3_ERROR) s->input_fault = ROLL3_INPUT_FAULT_PHYSICS;
            break;
        }
        s->remainder -= R3_STEP_MS;
    }
    if (s->game.s.state == R3_FALLEN) {
        s->fallen_at = now;
        s->remainder = 0;
    }
}
