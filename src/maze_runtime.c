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
static maze_runtime_init_status_t init_status = MAZE_RUNTIME_NOT_INITIALIZED;
static uint32_t sampling_run, last_report, last_revision;
static lab_motion_t latest;
static uint32_t last_render_ms, last_handler_ms, last_wait_ms;
static bool render_measured, loop_measured;
static struct {
    uint32_t run, gap, render, prep, handler, wait;
    bool valid, rv, present;
} timing_fault;
static struct rt_messagequeue commands;
static uint32_t command_pool[8 * (sizeof(uint32_t) + sizeof(void *)) / sizeof(uint32_t)];

static uint32_t now_ms(void) { return (uint32_t)rt_tick_get_millisecond(); }
static bool is_owner(void) { return initialized && rt_thread_self() == owner; }
void maze_runtime_note_loop(uint32_t handler_ms, uint32_t wait_ms)
{
    if (!is_owner()) return;
    last_handler_ms=handler_ms; last_wait_ms=wait_ms; loop_measured=true;
}

static void print_timing(void)
{
    /* Two records fit RT_CONSOLEBUF_SIZE even at UINT32_MAX. Frozen at fault. */
    rt_kprintf("[maze:timing] run=%u gap=%u render=%u prep=%u\n",
        timing_fault.run,timing_fault.gap,timing_fault.render,timing_fault.prep);
    rt_kprintf("[maze:loop] run=%u handler=%u wait=%u valid=%u rv=%u\n",
        timing_fault.run,timing_fault.handler,timing_fault.wait,
        (unsigned)timing_fault.valid,(unsigned)timing_fault.rv);
}

static void record_timing(maze_state_t before, uint32_t gap, uint32_t prep)
{
    const maze_snapshot_t *s=&controller.game.snapshot;
    if(before!=MAZE_RUNNING || s->state!=MAZE_ERROR || s->error_code!=MAZE_ERROR_TIMING)return;
    timing_fault.run=s->run_id;timing_fault.gap=gap;timing_fault.prep=prep;
    timing_fault.render=last_render_ms;timing_fault.handler=last_handler_ms;
    timing_fault.wait=last_wait_ms;timing_fault.valid=loop_measured;
    timing_fault.rv=render_measured;timing_fault.present=true;
    print_timing();
}
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
    uint32_t now=now_ms(), gap=now-controller.anchor_ms;
    maze_state_t before=controller.game.snapshot.state;
    result = maze_controller_action(&controller, action, now);
    record_timing(before,gap,0);
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
    /* RT_CONSOLEBUF_SIZE is 128 on this board. Each call must fit, including
     * its newline. run/seq associate the short records without a large buffer. */
    rt_kprintf("[maze:diag] run=%u rev=%u state=%d error=%d elapsed=%u cal=%u\n",
        s->run_id, s->revision, s->state, s->error_code, s->elapsed_ms,
        controller.calibration_count);
    rt_kprintf("[maze:imu] run=%u seq=%u age=%u a_mg=%d,%d,%d\n",
        s->run_id, latest.sequence, now-latest.sampled_ms,
        milli(latest.a[0]), milli(latest.a[1]), milli(latest.a[2]));
    rt_kprintf("[maze:gyro] seq=%u g_mdps=%d,%d,%d\n",
        latest.sequence, milli(latest.g[0]), milli(latest.g[1]), milli(latest.g[2]));
    rt_kprintf("[maze:input] seq=%u bias_mg=%d,%d tilt_milli=%d,%d pos_milli=%d,%d\n",
        latest.sequence,
        milli(controller.bias[0]), milli(controller.bias[1]),
        milli(controller.input.tilt_x), milli(controller.input.tilt_y),
        milli(s->ball_x), milli(s->ball_y));
}

static void tick(lv_timer_t *unused)
{
    uint32_t command, now, entered=now_ms();
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
    if (!suspended) {
        maze_state_t before=controller.game.snapshot.state;
        uint32_t gap=now-controller.anchor_ms;
        (void)maze_controller_tick(&controller, now,
            sampling && !stopping && latest.session == sampling_run ? &sample : NULL);
        record_timing(before,gap,now-entered);
    }
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
        else {
            last_report = now - 500;
            if(timing_fault.present)print_timing();
        }
    }
    sync_sampler();
    if (renderer && maze_controller_snapshot(&controller, &snapshot) == MAZE_OK &&
        snapshot.revision != last_revision)
    {
        last_revision = snapshot.revision;
        uint32_t started=now_ms();
        renderer(render_context, &snapshot, maze_levels_default());
        last_render_ms=now_ms()-started;
        render_measured=true;
    }
    if (diagnostics) report(now);
}

maze_runtime_init_status_t maze_runtime_init_status(void)
{
    return init_status;
}

maze_result_t maze_runtime_cancel_startup(void)
{
    if (!is_owner() || renderer || sampling || stopping ||
        controller.game.snapshot.run_id != 0) return MAZE_WRONG_STATE;
    initialized = false;
    lv_timer_delete(timer);
    timer = NULL;
    rt_mq_detach(&commands);
    owner = NULL;
    init_status = MAZE_RUNTIME_NOT_INITIALIZED;
    return MAZE_OK;
}

maze_result_t maze_runtime_init(bool selftests_passed)
{
    if (initialized) return MAZE_WRONG_STATE;
    if (!selftests_passed)
    {
        init_status = MAZE_RUNTIME_SELFTEST_FAILED;
        return MAZE_WRONG_STATE;
    }
    if (maze_controller_init(&controller, maze_levels_default(), now_ms()) != MAZE_OK)
    {
        init_status = MAZE_RUNTIME_LEVEL_FAILED;
        return MAZE_INVALID;
    }
    if (rt_mq_init(&commands, "maze_cmd", command_pool, sizeof(uint32_t),
                   sizeof(command_pool), RT_IPC_FLAG_FIFO) != RT_EOK)
    {
        init_status = MAZE_RUNTIME_QUEUE_FAILED;
        return MAZE_INVALID;
    }
    timer = lv_timer_create(tick, 20, NULL);
    if (!timer)
    {
        rt_mq_detach(&commands);
        init_status = MAZE_RUNTIME_TIMER_FAILED;
        return MAZE_INVALID;
    }
    owner = rt_thread_self();
    last_render_ms=last_handler_ms=last_wait_ms=0;
    render_measured=loop_measured=false;
    memset(&timing_fault,0,sizeof(timing_fault));
    initialized = true;
    init_status = MAZE_RUNTIME_READY;
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
