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
    assert(roll3_runtime_read(&snapshot, &pending, &result, NULL));
    assert(pending == expected_pending && result == R3_OK);
    return snapshot;
}

static void advance_sample(uint32_t ms)
{
    clock_ms+=ms;
    publication=(lab_motion_t){.a={0,0,1},.sequence=publication.sequence+1,
        .sampled_ms=clock_ms,.session=held_token,.valid=true};
    run_cycle();
}

static void start_calibrated_page(void)
{
    assert(roll3_runtime_open());
    assert(roll3_runtime_submit(R3_START,read_page(false).run));
    run_cycle();
    assert(held_token);
    publication.sequence=0;
    for(unsigned i=0;i<32;++i)advance_sample(20);
    assert(read_page(false).state==R3_RUNNING);
}

static r3_snapshot_t drop_ball(void)
{
    session.game.s.position.z=session.game.world->fall_z-.1f;
    advance_sample(R3_STEP_MS);
    r3_snapshot_t fallen=read_page(false);
    assert(fallen.state==R3_FALLEN);
    return fallen;
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
    held_token = 0;
    assert(roll3_runtime_open());
    assert(roll3_runtime_submit(R3_START, read_page(false).run));
    run_cycle();
    assert(read_page(false).state == R3_CALIBRATING && held_token != 0);
    held_token = 0;
    run_cycle();
    assert(read_page(false).state == R3_ERROR && session.input_fault == ROLL3_INPUT_FAULT_OWNER_LOST);
    puts("TC-511 runtime owner loss diagnosed PASS");
    roll3_runtime_close();
    run_cycle();
    start_calibrated_page();
    r3_snapshot_t fallen=drop_ball();
    advance_sample(ROLL3_UI_TIMEOUT_MS+1);
    assert(read_page(false).state==R3_FALLEN); /* Unresponsive page: wait. */
    run_cycle();
    snapshot=read_page(false);
    assert(snapshot.state==R3_RUNNING&&snapshot.run==fallen.run&&snapshot.falls==fallen.falls);
    assert(snapshot.elapsed_ms==fallen.elapsed_ms);

    fallen=drop_ball();
    clock_ms+=ROLL3_AUTO_RESPAWN_MS;
    publication.sampled_ms=clock_ms;
    ++publication.sequence;
    assert(roll3_runtime_submit(R3_RESTART,fallen.run));
    run_cycle();
    assert(read_page(false).state==R3_CALIBRATING); /* User command wins. */
    roll3_runtime_close();
    run_cycle();
    held_token=0;
    run_cycle();

    start_calibrated_page();
    drop_ball();
    roll3_runtime_close();
    advance_sample(ROLL3_AUTO_RESPAWN_MS);
    assert(session.game.s.state==R3_READY); /* Closing page wins. */
    held_token=0;
    run_cycle();
    assert(!roll3_runtime_busy());
    puts("TC-515 auto respawn requires live page and yields to restart/close PASS");
    start_calibrated_page();
    session.game.s.position=r3_demo_world()->goal_box_plus_one?
        (r3_vec_t){r3_demo_world()->boxes[9].center.x,r3_demo_world()->boxes[9].center.y,1.08f+R3_RADIUS}:session.game.s.position;
    session.game.s.velocity=(r3_vec_t){.6f,0,0};
    unsigned previous_stops=stops;
    advance_sample(5);snapshot=read_page(false);
    assert(snapshot.state==R3_WON&&stops==previous_stops+1);
    /* This fake service acknowledges stop asynchronously, like earlier close
     * cases in this suite. Complete that acknowledgement before the next cycle. */
    held_token=0;
    uint32_t score_time=snapshot.elapsed_ms;float ball_x=snapshot.position.x;
    publication.valid=false;clock_ms+=20;run_cycle();snapshot=read_page(false);
    assert(snapshot.state==R3_WON&&snapshot.elapsed_ms==score_time&&snapshot.position.x>ball_x&&motion_token==0);
    assert(mailbox.steps_max>=4&&session.input_fault==ROLL3_INPUT_FAULT_NONE);
    roll3_runtime_close();run_cycle();assert(!roll3_runtime_busy());
    puts("TC-524 worker releases IMU, rolls behind result and closes cleanly PASS");
    return 0;
}
