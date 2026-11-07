#include "maze_controller.h"

#include <math.h>
#include <string.h>

static bool active(maze_state_t state)
{
    return state == MAZE_CALIBRATING || state == MAZE_RUNNING || state == MAZE_PAUSED;
}

maze_result_t maze_controller_init(maze_controller_t *c,
                                    const maze_level_t *level, uint32_t now_ms)
{
    if (!c) return MAZE_INVALID;
    memset(c, 0, sizeof(*c));
    c->anchor_ms = now_ms;
    return maze_game_init(&c->game, level);
}

maze_result_t maze_controller_next_request(maze_controller_t *c, uint32_t *id)
{
    if (!c || !id || !c->game.level) return MAZE_INVALID;
    if (c->allocated_request == UINT32_MAX) return MAZE_EXHAUSTED;
    *id = ++c->allocated_request;
    return MAZE_OK;
}

maze_result_t maze_controller_snapshot(const maze_controller_t *c, maze_snapshot_t *out)
{
    return c ? maze_game_snapshot(&c->game, out) : MAZE_INVALID;
}

maze_result_t maze_controller_input_failed(maze_controller_t *c, uint32_t run_id)
{
    if (!c) return MAZE_INVALID;
    return maze_game_input_failed(&c->game, run_id);
}

maze_result_t maze_controller_reconnect(maze_controller_t *c, uint32_t now_ms)
{
    if (!c || !c->game.level) return MAZE_INVALID;
    if (c->game.snapshot.state == MAZE_RUNNING) return MAZE_WRONG_STATE;
    c->anchor_ms = c->started_ms = now_ms;
    c->have_sample = false;
    c->input.valid = false;
    c->calibration_count = 0;
    c->sum[0] = c->sum[1] = 0;
    return MAZE_OK;
}

static bool finite_sample(const maze_motion_t *s)
{
    unsigned i;
    if (!s->valid || s->sequence == 0) return false;
    for (i = 0; i < 3; ++i)
        if (!isfinite(s->a[i]) || !isfinite(s->g[i]) ||
            fabsf(s->a[i]) > 4.0f || fabsf(s->g[i]) > 300.0f)
            return false;
    return true;
}

static float normalize(float x)
{
    float magnitude = fabsf(x);
    if (magnitude <= 0.03f) return 0;
    magnitude = (magnitude - 0.03f) / (0.35f - 0.03f);
    if (magnitude > 1) magnitude = 1;
    return x < 0 ? -magnitude : magnitude;
}

static maze_result_t consume(maze_controller_t *c, uint32_t now, const maze_motion_t *s)
{
    unsigned i;
    bool stable = true;
    float norm;
    maze_state_t state = c->game.snapshot.state;
    if (!s || !active(state) || s->run_id != c->game.snapshot.run_id)
        return MAZE_OK;
    /* A previous session or a future timestamp must never calibrate this run. */
    if (state == MAZE_CALIBRATING &&
        ((uint32_t)(s->sampled_ms - c->started_ms) == 0 ||
         (uint32_t)(s->sampled_ms - c->started_ms) >= 0x80000000u))
        return MAZE_OK;
    if (!finite_sample(s) || (uint32_t)(now - s->sampled_ms) > MAZE_SAMPLE_MAX_AGE_MS)
        return maze_controller_input_failed(c, s->run_id);
    if (c->have_sample && s->sequence == c->sample_sequence)
        return MAZE_OK;
    if (c->have_sample &&
        (s->sequence < c->sample_sequence ||
         (uint32_t)(s->sampled_ms - c->sample_ms) >= 0x80000000u ||
         s->sampled_ms == c->sample_ms))
        return maze_controller_input_failed(c, s->run_id);

    norm = s->a[0]*s->a[0] + s->a[1]*s->a[1] + s->a[2]*s->a[2];
    if (state == MAZE_CALIBRATING)
    {
        stable = norm >= 0.85f*0.85f && norm <= 1.15f*1.15f &&
                 fabsf(s->a[0]) <= 0.25f && fabsf(s->a[1]) <= 0.25f &&
                 fabsf(s->a[2]) >= 0.85f;
        for (i = 0; i < 3; ++i)
            if (fabsf(s->g[i]) > 5.0f || (c->calibration_count &&
                fabsf(s->a[i] - c->previous[i]) > 0.04f)) stable = false;
        if (!stable)
        {
            c->calibration_count = 0;
            c->sum[0] = c->sum[1] = 0;
        }
        else
        {
            c->sum[0] += s->a[0]; c->sum[1] += s->a[1];
            if (++c->calibration_count == MAZE_CALIBRATION_SAMPLES)
            {
                c->bias[0] = c->sum[0] / MAZE_CALIBRATION_SAMPLES;
                c->bias[1] = c->sum[1] / MAZE_CALIBRATION_SAMPLES;
                c->input.valid = true;
                c->anchor_ms = now;
                if (maze_game_calibrated(&c->game, s->run_id) != MAZE_OK)
                    return MAZE_EXHAUSTED;
            }
        }
    }
    else
    {
        for (i = 0; i < 2; ++i)
            c->filtered[i] += 0.25f * (s->a[i] - c->bias[i] - c->filtered[i]);
        /* Full board-run04 + confirmed unchanged right/left/up/down:
         * screen x=-sensor ay; screen y=-sensor ax. Bias stays sensor-native. */
        c->input.tilt_x = normalize(-c->filtered[1]);
        c->input.tilt_y = normalize(-c->filtered[0]);
        c->input.valid = true;
    }
    for (i = 0; i < 3; ++i) c->previous[i] = s->a[i];
    c->sample_sequence = s->sequence;
    c->sample_ms = s->sampled_ms;
    c->have_sample = true;
    return MAZE_OK;
}

maze_result_t maze_controller_tick(maze_controller_t *c, uint32_t now,
                                    const maze_motion_t *s)
{
    maze_result_t result;
    uint32_t dt;
    if (!c || !c->game.level) return MAZE_INVALID;
    if (c->game.snapshot.state == MAZE_CALIBRATING &&
        (uint32_t)(now - c->started_ms) >= MAZE_CALIBRATION_TIMEOUT_MS)
        return maze_controller_input_failed(c, c->game.snapshot.run_id);
    /* A delayed UI callback is a timing fault even if its sample is also old. */
    if (c->game.snapshot.state == MAZE_RUNNING &&
        (uint32_t)(now - c->anchor_ms) > MAZE_MAX_DT_MS)
    {
        const maze_input_t neutral = {0, 0, true};
        dt = now - c->anchor_ms;
        c->anchor_ms = now;
        return maze_game_step(&c->game, &neutral, dt);
    }
    result = consume(c, now, s);
    if (result != MAZE_OK) return result;
    if (c->game.snapshot.state == MAZE_CALIBRATING)
    {
        c->anchor_ms = now;
        if ((uint32_t)(now - c->started_ms) >= MAZE_CALIBRATION_TIMEOUT_MS)
            return maze_controller_input_failed(c, c->game.snapshot.run_id);
        return MAZE_OK;
    }
    dt = now - c->anchor_ms;
    c->anchor_ms = now;
    if (c->game.snapshot.state != MAZE_RUNNING) return MAZE_OK;
    if (!c->have_sample || (uint32_t)(now - c->sample_ms) > MAZE_SAMPLE_MAX_AGE_MS)
        return maze_controller_input_failed(c, c->game.snapshot.run_id);
    return maze_game_step(&c->game, &c->input, dt);
}

maze_result_t maze_controller_action(maze_controller_t *c,
                                      const maze_action_t *a, uint32_t now)
{
    maze_game_t probe;
    maze_result_t result;
    uint32_t old_run;
    if (!c || !a || a->request_id > c->allocated_request) return MAZE_INVALID;
    /* Reject stale/invalid actions before any time settlement or mutation. */
    probe = c->game;
    result = maze_game_action(&probe, a);
    if (result != MAZE_OK) return result;
    if (a->action == MAZE_ACTION_RESUME && (!c->have_sample ||
        (uint32_t)(now - c->sample_ms) > MAZE_SAMPLE_MAX_AGE_MS))
        return MAZE_WRONG_STATE;
    if (a->action == MAZE_ACTION_PAUSE)
    {
        result = maze_controller_tick(c, now, NULL);
        if (result != MAZE_OK) return result;
    }
    old_run = c->game.snapshot.run_id;
    result = maze_game_action(&c->game, a);
    if (result != MAZE_OK) return result;
    c->anchor_ms = now;
    if (c->game.snapshot.run_id != old_run)
    {
        c->started_ms = now;
        c->calibration_count = 0;
        c->sum[0] = c->sum[1] = c->bias[0] = c->bias[1] = 0;
        c->filtered[0] = c->filtered[1] = 0;
        c->have_sample = false;
        memset(&c->input, 0, sizeof(c->input));
    }
    return MAZE_OK;
}
