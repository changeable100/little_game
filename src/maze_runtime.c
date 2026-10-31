#include "maze_runtime.h"
#include "maze_controller.h"
#include "maze_levels.h"
#include "lab.h"
#include "lab_game.h"
#include "lvgl.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static maze_controller_t controller;
static lv_timer_t *timer;
static rt_thread_t owner;
static maze_render_t renderer;
static void *render_context;
static bool initialized, sampling, stopping, suspended, diagnostics;
static uint32_t sampling_run, last_report, last_revision;
static lab_motion_t latest;
static struct rt_messagequeue commands;
static uint32_t command_pool[8 * (sizeof(uint32_t) + sizeof(void *)) / sizeof(uint32_t)];

static uint32_t now_ms(void) { return (uint32_t)rt_tick_get_millisecond(); }
static bool is_owner(void) { return initialized && rt_thread_self() == owner; }
static bool wants_samples(void)
{
    maze_state_t s = controller.game.snapshot.state;
    return !suspended && (s == MAZE_CALIBRATING || s == MAZE_RUNNING || s == MAZE_PAUSED);
}

static void sync_sampler(void)
{
    uint32_t run = controller.game.snapshot.run_id;
    bool busy, owned;
    if (!lab_maze_motion_state(sampling_run, &busy, &owned)) return;
    if (sampling && (stopping || !wants_samples() || sampling_run != run))
    {
        if (owned)
        {
            if (!stopping) stopping = lab_maze_motion_stop(sampling_run);
            return;
        }
        sampling = stopping = false;
        memset(&latest, 0, sizeof(latest));
    }
    if (!wants_samples()) return;
    if (sampling)
    {
        if (!owned)
        {
            (void)maze_controller_input_failed(&controller, run);
            sampling = false;
        }
        return;
    }
    if (!lab_maze_motion_start(run))
    {
        rt_kprintf("[maze] sampling slot unavailable\n");
        (void)maze_controller_input_failed(&controller, run);
        return;
    }
    sampling = true;
    stopping = false;
    sampling_run = run;
    memset(&latest, 0, sizeof(latest));
}

maze_result_t maze_runtime_snapshot(maze_snapshot_t *out)
{
    return is_owner() ? maze_controller_snapshot(&controller, out) : MAZE_WRONG_STATE;
}

maze_result_t maze_runtime_next_request(uint32_t *id)
{
    return is_owner() ? maze_controller_next_request(&controller, id) : MAZE_WRONG_STATE;
}

maze_result_t maze_runtime_action(void *context, const maze_action_t *action)
{
    maze_result_t result;
    if (!is_owner()) return MAZE_WRONG_STATE;
    if (context) return MAZE_INVALID;
    result = maze_controller_action(&controller, action, now_ms());
    if (result == MAZE_OK && (action->action == MAZE_ACTION_START ||
                              action->action == MAZE_ACTION_RESTART))
        suspended = false;
    sync_sampler();
    return result;
}

maze_result_t maze_runtime_attach(maze_render_t render, void *context)
{
    if (!is_owner()) return MAZE_WRONG_STATE;
    if (!render) return MAZE_INVALID;
    if (renderer) return MAZE_WRONG_STATE;
    renderer = render;
    render_context = context;
    last_revision = 0;
    if (suspended)
    {
        maze_result_t result = maze_controller_reconnect(&controller, now_ms());
        if (result != MAZE_OK)
        {
            renderer = NULL;
            render_context = NULL;
            return result;
        }
        suspended = false;
    }
    return MAZE_OK;
}

maze_result_t maze_runtime_detach(void)
{
    maze_action_t action;
    maze_result_t result = MAZE_OK;
    if (!is_owner()) return MAZE_WRONG_STATE;
    /* Clear borrowed UI references before any lifecycle work. */
    renderer = NULL;
    render_context = NULL;
    if (controller.game.snapshot.state == MAZE_RUNNING)
    {
        result = maze_runtime_next_request(&action.request_id);
        if (result == MAZE_OK)
        {
            action.expected_run_id = controller.game.snapshot.run_id;
            action.action = MAZE_ACTION_PAUSE;
            result = maze_runtime_action(NULL, &action);
        }
    }
    suspended = true;
    if (sampling && !stopping) stopping = lab_maze_motion_stop(sampling_run);
    return result;
}

static int milli(float value)
{
    return isfinite(value) && fabsf(value) < 10000 ? (int)(value * 1000) : 0;
}

static void report(uint32_t now)
{
    const maze_snapshot_t *s = &controller.game.snapshot;
    if (now - last_report < 500) return;
    last_report = now;
    /* Fixed point output works even if rt_kprintf has no float support. */
    rt_kprintf("[maze:diag] run=%u rev=%u state=%d error=%d elapsed=%u cal=%u seq=%u age=%u a_mg=%d,%d,%d g_mdps=%d,%d,%d bias_mg=%d,%d tilt_milli=%d,%d pos_milli=%d,%d\n",
        s->run_id, s->revision, s->state, s->error_code, s->elapsed_ms,
        controller.calibration_count, latest.sequence, now-latest.sampled_ms,
        milli(latest.a[0]), milli(latest.a[1]), milli(latest.a[2]),
        milli(latest.g[0]), milli(latest.g[1]), milli(latest.g[2]),
        milli(controller.bias[0]), milli(controller.bias[1]),
        milli(controller.input.tilt_x), milli(controller.input.tilt_y),
        milli(s->ball_x), milli(s->ball_y));
}

static void tick(lv_timer_t *unused)
{
    uint32_t command, now;
    maze_motion_t sample;
    maze_snapshot_t snapshot;
    unsigned drained;
    (void)unused;
    sync_sampler();
    (void)lab_motion_try_snapshot(&latest);
    now = now_ms(); /* Same epoch, observed AFTER copying the publication. */
    memset(&sample, 0, sizeof(sample));
    memcpy(sample.a, latest.a, sizeof(sample.a));
    memcpy(sample.g, latest.g, sizeof(sample.g));
    sample.sequence = latest.sequence;
    sample.sampled_ms = latest.sampled_ms;
    sample.run_id = latest.session;
    sample.valid = latest.valid;
    /* Before the first publication there is no sample, not an invalid sample. */
    if (!suspended)
        (void)maze_controller_tick(&controller, now,
            sampling && !stopping && latest.session == sampling_run ? &sample : NULL);
    /* Bound command work even if the shell keeps producing messages. */
    for (drained = 0; drained < 8 &&
        rt_mq_recv(&commands, &command, sizeof(command), 0) == RT_EOK; ++drained)
    {
        diagnostics = true;
        if (command <= MAZE_ACTION_BACK)
        {
            maze_action_t action;
            maze_result_t result = maze_runtime_next_request(&action.request_id);
            action.action = (maze_action_kind_t)command;
            action.expected_run_id = controller.game.snapshot.run_id;
            if (result == MAZE_OK) result = maze_runtime_action(NULL, &action);
            rt_kprintf("[maze:command] action=%u result=%d run=%u\n", command, result, controller.game.snapshot.run_id);
        }
        else last_report = now - 500;
    }
    sync_sampler();
    if (renderer && maze_controller_snapshot(&controller, &snapshot) == MAZE_OK &&
        snapshot.revision != last_revision)
    {
        last_revision = snapshot.revision;
        renderer(render_context, &snapshot, maze_levels_default());
    }
    if (diagnostics) report(now);
}

maze_result_t maze_runtime_init(bool selftests_passed)
{
    if (initialized) return MAZE_WRONG_STATE;
    if (!selftests_passed) return MAZE_WRONG_STATE;
    if (maze_controller_init(&controller, maze_levels_default(), now_ms()) != MAZE_OK)
        return MAZE_INVALID;
    if (rt_mq_init(&commands, "maze_cmd", command_pool, sizeof(uint32_t),
                   sizeof(command_pool), RT_IPC_FLAG_FIFO) != RT_EOK) return MAZE_INVALID;
    owner = rt_thread_self();
    timer = lv_timer_create(tick, 20, NULL);
    if (!timer) { rt_mq_detach(&commands); return MAZE_INVALID; }
    initialized = true;
    return MAZE_OK;
}

static void maze_diag(int argc, char **argv)
{
    static const char *names[] = {"start", "pause", "resume", "restart", "back", "status"};
    uint32_t command;
    if (argc != 2) { rt_kprintf("maze_diag start|pause|resume|restart|back|status\n"); return; }
    if (!initialized) { rt_kprintf("[maze] runtime unavailable (initialization/selftests)\n"); return; }
    for (command = 0; command < sizeof(names)/sizeof(names[0]); ++command)
        if (strcmp(argv[1], names[command]) == 0)
        {
            rt_err_t result = rt_mq_send(&commands, &command, sizeof(command));
            rt_kprintf("[maze:command] queued=%u result=%d\n", command, result);
            return;
        }
    rt_kprintf("[maze] unknown command\n");
}
MSH_CMD_EXPORT(maze_diag, Maze motion diagnostics via UI owner);
