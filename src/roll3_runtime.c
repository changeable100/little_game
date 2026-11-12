#include "roll3_runtime.h"
#include "roll3_session.h"
#include "roll3_scene.h"
#include "rtthread.h"

static roll3_session_t session;
static struct rt_mutex lock;
static struct rt_thread physics_thread;
ALIGN(RT_ALIGN_SIZE) static uint8_t physics_stack[4096];

/* Shared only under lock. The model and IMU ownership belong to the worker. */
static struct {
    r3_snapshot_t snapshot;
    uint32_t heartbeat, action_run, sample_stamp;
    r3_action_kind_t action;
    r3_result_t result;
    bool occupied, page, pending, failed, have_sample;
} mailbox;
static bool ready;
static uint32_t motion_token;

static uint32_t now_ms(void)
{
    return (uint32_t)rt_tick_get_millisecond();
}

static void acquire(void) { (void)rt_mutex_take(&lock, RT_WAITING_FOREVER); }
static void release(void) { (void)rt_mutex_release(&lock); }

static void sync_motion(bool desired)
{
    uint32_t run = session.game.s.run;
    uint32_t wanted = run | LAB_ROLL3_SESSION_BIT;
    bool busy, owned;
    if (!lab_maze_motion_state(motion_token, &busy, &owned)) return;
    if (motion_token && (!desired || motion_token != wanted)) {
        if (owned) {
            (void)lab_maze_motion_stop(motion_token);
            return;
        }
        motion_token = 0;
    }
    if (!desired) return;
    if (motion_token) {
        if (!owned) {
            motion_token = 0;
            roll3_session_fail_input(&session, ROLL3_INPUT_FAULT_OWNER_LOST);
        }
        return;
    }
    if (!busy && lab_roll3_motion_start(run)) motion_token = wanted;
}

static const char *input_fault_name(roll3_input_fault_t fault)
{
    switch (fault) {
    case ROLL3_INPUT_FAULT_CAL_TIMEOUT: return "cal_timeout";
    case ROLL3_INPUT_FAULT_SAMPLE_TIMEOUT: return "sample_timeout";
    case ROLL3_INPUT_FAULT_INVALID_SAMPLE: return "invalid_sample";
    case ROLL3_INPUT_FAULT_SEQUENCE: return "sequence";
    case ROLL3_INPUT_FAULT_OWNER_LOST: return "owner_lost";
    case ROLL3_INPUT_FAULT_PHYSICS: return "physics";
    default: return "unknown";
    }
}

static void run_cycle(void)
{
    acquire();
    bool occupied = mailbox.occupied;
    bool page = mailbox.page;
    bool pending = mailbox.pending;
    r3_action_kind_t action = mailbox.action;
    uint32_t run = mailbox.action_run;
    uint32_t heartbeat = mailbox.heartbeat;
    release();
    if (!occupied) return;

    uint32_t now = now_ms();
    r3_state_t previous_state = session.game.s.state;
    uint32_t previous_run = session.game.s.run;
    r3_result_t result = R3_OK;
    lab_motion_t sample;
    bool received = false;
    if (page) {
        received = motion_token && lab_motion_try_snapshot(&sample);
        now = now_ms();
        /* A UI heartbeat may be newer than a previous worker iteration. */
        roll3_session_update(&session, now, received ? &sample : NULL,
                             now - heartbeat <= ROLL3_UI_TIMEOUT_MS);
        if (pending) result = roll3_session_action(&session, action, run, now);
    } else if (session.game.s.state != R3_READY) {
        result = roll3_session_action(&session, R3_BACK, previous_run, now);
    }
    sync_motion(page && roll3_session_needs_motion(&session));
    bool automatically_respawned = false;
    acquire();
    if (page && mailbox.page && !pending && !mailbox.pending && session.game.s.state == R3_FALLEN) {
        uint32_t respawn_now = now_ms();
        if (respawn_now - mailbox.heartbeat <= ROLL3_UI_TIMEOUT_MS)
            automatically_respawned = roll3_session_try_auto_respawn(&session, respawn_now);
    }
    mailbox.snapshot = session.game.s;
    mailbox.have_sample = session.sequence != 0;
    mailbox.sample_stamp = session.stamp;
    if (pending && mailbox.page) {
        mailbox.result = result;
        mailbox.pending = false;
    }
    if (!page && !motion_token) {
        mailbox.failed = result != R3_OK;
        mailbox.occupied = false;
    }
    release();
    if (automatically_respawned)
        rt_kprintf("[roll3:respawn] auto run=%u checkpoint=%u falls=%u\n",
                   session.game.s.run, session.game.s.checkpoint, session.game.s.falls);
    if (previous_state != session.game.s.state || previous_run != session.game.s.run)
        rt_kprintf("[roll3] run=%u state=%d cal=%u elapsed=%u recovery=%d\n",
                   session.game.s.run, (int)session.game.s.state, session.stable,
                   session.game.s.elapsed_ms, (int)session.recovery);

    if (previous_state != R3_ERROR && session.game.s.state == R3_ERROR)
        rt_kprintf("[roll3:input] run=%u reason=%s accepted_seq=%u age_ms=%u received=%u sample_run=%u sample_seq=%u valid=%u\n",
                   session.game.s.run, input_fault_name(session.input_fault), session.sequence,
                   session.sequence ? now - session.stamp : UINT32_MAX, (unsigned)received,
                   received ? sample.session : 0, received ? sample.sequence : 0,
                   received ? (unsigned)sample.valid : 0);
}

static void worker(void *unused)
{
    (void)unused;
    for (;;) {
        run_cycle();
        rt_thread_mdelay(R3_STEP_MS);
    }
}

bool roll3_runtime_init(bool selftests_passed)
{
    if (ready) return true;
    if (!selftests_passed || roll3_session_init(&session, r3_demo_world()) != R3_OK)
        return false;
    mailbox.snapshot = session.game.s;
    mailbox.have_sample = session.sequence != 0;
    mailbox.sample_stamp = session.stamp;
    if (rt_mutex_init(&lock, "r3_lock", RT_IPC_FLAG_PRIO) != RT_EOK) return false;
    if (rt_thread_init(&physics_thread, "roll3", worker, RT_NULL, physics_stack,
                       sizeof(physics_stack), RT_MAIN_THREAD_PRIORITY - 1, 5) != RT_EOK) {
        rt_mutex_detach(&lock);
        return false;
    }
    if (rt_thread_startup(&physics_thread) != RT_EOK) {
        rt_thread_detach(&physics_thread);
        rt_mutex_detach(&lock);
        return false;
    }
    ready = true;
    return true;
}

bool roll3_runtime_busy(void)
{
    if (!ready) return false;
    acquire();
    bool occupied = mailbox.occupied;
    release();
    return occupied;
}

bool roll3_runtime_open(void)
{
    if (!ready) return false;
    acquire();
    bool available = !mailbox.occupied && !mailbox.failed;
    if (available) {
        mailbox.occupied = mailbox.page = true;
        mailbox.pending = false;
        mailbox.result = R3_OK;
        mailbox.heartbeat = now_ms();
    }
    release();
    return available;
}

void roll3_runtime_close(void)
{
    if (!ready) return;
    acquire();
    mailbox.page = mailbox.pending = false;
    release();
}

bool roll3_runtime_read(r3_snapshot_t *snapshot, bool *pending, r3_result_t *result,
                        uint32_t *sample_age_ms)
{
    if (!ready || !snapshot || !pending || !result) return false;
    acquire();
    bool open = mailbox.page;
    if (open) {
        mailbox.heartbeat = now_ms();
        *snapshot = mailbox.snapshot;
        *pending = mailbox.pending;
        *result = mailbox.result;
        if (sample_age_ms) *sample_age_ms = mailbox.have_sample ?
            mailbox.heartbeat - mailbox.sample_stamp : UINT32_MAX;
    }
    release();
    return open;
}

void roll3_runtime_report_frame(uint32_t frames, uint32_t sample_age_ms,
                                uint32_t draw_ms, uint32_t frame_gap_ms)
{
    rt_kprintf("[roll3:frame] frames=%u sample_age_max_ms=%u draw_max_ms=%u gap_max_ms=%u\n",
               frames, sample_age_ms, draw_ms, frame_gap_ms);
}

void roll3_runtime_report_open(int result,unsigned depth_rows)
{
    rt_kprintf("[roll3:open] result=%d depth_rows=%u\n",result,depth_rows);
}

bool roll3_runtime_submit(r3_action_kind_t kind, uint32_t run)
{
    if (!ready || kind < R3_START || kind > R3_RESPAWN || kind == R3_BACK)
        return false;
    acquire();
    bool accept = mailbox.page && !mailbox.pending && run == mailbox.snapshot.run;
    if (accept) {
        mailbox.pending = true;
        mailbox.action = kind;
        mailbox.action_run = run;
        mailbox.heartbeat = now_ms();
    }
    release();
    return accept;
}
