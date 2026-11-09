#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "../src/roll3_runtime.c"

static uint32_t clock_ms, held_token;
static unsigned stops;
static lab_motion_t publication;

uint32_t rt_tick_get_millisecond(void) { return clock_ms; }
void rt_thread_mdelay(int ms) { clock_ms += ms; }
int rt_kprintf(const char *format, ...) { (void)format; return 0; }
rt_err_t rt_mutex_init(struct rt_mutex *m, const char *name, unsigned flags)
{ (void)name; (void)flags; m->held = 0; return 0; }
rt_err_t rt_mutex_take(struct rt_mutex *m, int timeout)
{ (void)timeout; assert(!m->held); m->held = 1; return 0; }
rt_err_t rt_mutex_release(struct rt_mutex *m)
{ assert(m->held); m->held = 0; return 0; }
rt_err_t rt_mutex_detach(struct rt_mutex *m)
{ assert(!m->held); return 0; }
rt_err_t rt_thread_init(struct rt_thread *t, const char *name, void (*entry)(void *),
                        void *arg, void *stack, size_t bytes, unsigned priority, unsigned ticks)
{
    (void)name; (void)arg; (void)ticks;
    assert(entry && stack && bytes && priority < RT_MAIN_THREAD_PRIORITY);
    t->initialized = 1;
    return 0;
}
rt_err_t rt_thread_startup(struct rt_thread *t) { assert(t->initialized); return 0; }
rt_err_t rt_thread_detach(struct rt_thread *t) { t->initialized = 0; return 0; }
bool lab_maze_motion_state(uint32_t token, bool *busy, bool *owned)
{ assert(!lock.held); *busy = held_token != 0; *owned = *busy && token == held_token; return true; }
bool lab_roll3_motion_start(uint32_t run)
{ assert(!lock.held && !held_token); held_token = run | LAB_ROLL3_SESSION_BIT; return true; }
bool lab_maze_motion_stop(uint32_t token)
{ assert(!lock.held && held_token == token); ++stops; return true; }
bool lab_motion_try_snapshot(lab_motion_t *out)
{ assert(!lock.held); *out = publication; return true; }

static r3_snapshot_t read_page(bool expected_pending)
{
    r3_snapshot_t snapshot;
    bool pending;
    r3_result_t result;
    assert(roll3_runtime_read(&snapshot, &pending, &result));
    assert(pending == expected_pending && result == R3_OK);
    return snapshot;
}

int main(void)
{
    assert(!roll3_runtime_init(false) && !roll3_runtime_open());
    assert(roll3_runtime_init(true) && roll3_runtime_open());
    assert(roll3_runtime_submit(R3_START, 0) && !roll3_runtime_submit(R3_START, 0));
    assert(read_page(true).state == R3_READY);
    run_cycle();
    r3_snapshot_t snapshot = read_page(false);
    assert(snapshot.state == R3_CALIBRATING && held_token == (1 | LAB_ROLL3_SESSION_BIT));
    assert(!roll3_runtime_submit(R3_RESTART, 0));
    for (unsigned i = 1; i <= 32; ++i) {
        clock_ms += 20;
        publication = (lab_motion_t){.a = {0, 0, 1}, .sequence = i,
            .sampled_ms = clock_ms, .session = held_token, .valid = true};
        run_cycle();
    }
    snapshot = read_page(false);
    assert(snapshot.state == R3_RUNNING && snapshot.elapsed_ms == 0);
    puts("TC-504 runtime owner, single slot, calibration and stale run PASS");

    assert(roll3_runtime_submit(R3_RESTART, snapshot.run));
    roll3_runtime_close();
    run_cycle();
    assert(stops == 1 && roll3_runtime_busy() && !roll3_runtime_open());
    held_token = 0;
    run_cycle();
    assert(!roll3_runtime_busy() && roll3_runtime_open());
    snapshot = read_page(false);
    assert(snapshot.state == R3_READY && snapshot.run == 2);
    held_token = 4; /* An unrelated 2D session. */
    assert(roll3_runtime_submit(R3_START, snapshot.run));
    run_cycle();
    assert(read_page(false).state == R3_CALIBRATING && held_token == 4);
    roll3_runtime_close();
    run_cycle();
    assert(held_token == 4 && stops == 1 && !roll3_runtime_busy());
    puts("TC-504 close discards pending action and waits for own IMU cleanup PASS");
    return 0;
}
