/* Deterministic service-boundary tests using the production runtime.c.
 * RT/LVGL and sensor transport are fakes; no claim about actual hardware. */
#include "maze_runtime.h"
#include "lab_game.h"
#include "rtthread.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { printf("FAIL line %d: %s\n",__LINE__,#expr); return 1; } } while (0)
static uint32_t clock_ms, worker_session;
static bool busy, stop_requested;
static unsigned starts, stops, wrong_stops, timers, draws;
static unsigned truncated_logs, complete_input_logs;
static unsigned timing_logs, render_cost;
static char timing_line[128];
static char loop_line[128];
static bool fail_queue, fail_timer;
static unsigned queue_inits, queue_detaches, timer_attempts;
static rt_thread_t current_thread=(void *)1;
static lv_timer_t ui_timer;
static lab_motion_t motion;
static struct rt_messagequeue *queue;
extern void (*test_export_maze_diag)(int, char **);

rt_thread_t rt_thread_self(void) { return current_thread; }
uint32_t rt_tick_get_millisecond(void) { return clock_ms; }
int rt_kprintf(const char *format, ...)
{
    int result;
    char output[128];
    va_list args;
    va_start(args,format);
    result=vsnprintf(output,sizeof(output)-1,format,args);
    va_end(args);
    if (result >= (int)sizeof(output)-1) ++truncated_logs;
    if (strstr(output,"tilt_milli=") && strstr(output,"pos_milli=")) ++complete_input_logs;
    if (strstr(output,"[maze:timing]")) {
        ++timing_logs; memcpy(timing_line,output,sizeof(output));
    }
    if (strstr(output,"[maze:loop]")) memcpy(loop_line,output,sizeof(output));
    fputs(output,stdout);
    return result;
}
rt_err_t rt_mq_init(struct rt_messagequeue *q, const char *name, void *pool,
                    size_t size, size_t pool_size, unsigned flag)
{
    (void)name; (void)pool; (void)size; (void)pool_size; (void)flag;
    ++queue_inits;
    if (fail_queue) return -1;
    memset(q,0,sizeof(*q)); queue=q; return RT_EOK;
}
rt_err_t rt_mq_detach(struct rt_messagequeue *q) { (void)q; ++queue_detaches; return RT_EOK; }
rt_err_t rt_mq_send(struct rt_messagequeue *q, void *data, size_t size)
{
    if (q->count==8 || size!=sizeof(uint32_t)) return -1;
    memcpy(&q->values[(q->head+q->count)%8],data,size); ++q->count; return RT_EOK;
}
rt_err_t rt_mq_recv(struct rt_messagequeue *q, void *out, size_t size, int wait)
{
    (void)wait;
    if (!q->count || size!=sizeof(uint32_t)) return -1;
    memcpy(out,&q->values[q->head],size); q->head=(q->head+1)%8; --q->count;
    return RT_EOK;
}
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), uint32_t period, void *data)
{
    (void)data;
    ++timer_attempts;
    if (fail_timer) return NULL;
    if (period!=20) return NULL;
    ++timers; ui_timer.callback=callback; return &ui_timer;
}
void lv_timer_delete(lv_timer_t *t) { if (t==&ui_timer && timers) --timers; }
bool lab_maze_motion_state(uint32_t session, bool *is_busy, bool *owned)
{
    *is_busy=busy; *owned=busy && worker_session==session && session!=0; return true;
}
bool lab_maze_motion_start(uint32_t session)
{
    if (busy) return false;
    busy=true; worker_session=session; stop_requested=false; ++starts;
    memset(&motion,0,sizeof(motion)); return true;
}
bool lab_maze_motion_stop(uint32_t session)
{
    if (!busy || !session || worker_session!=session) { ++wrong_stops; return false; }
    stop_requested=true; ++stops; return true;
}
bool lab_motion_try_snapshot(lab_motion_t *out) { *out=motion; return true; }

static void render(void *context, const maze_snapshot_t *s, const maze_level_t *level)
{
    if (context==(void *)7 && level && s) ++draws;
    clock_ms+=render_cost;
}
static void pump(unsigned ms, bool publish)
{
    clock_ms+=ms;
    if (publish)
    {
        motion.a[0]=0.1f; motion.a[1]=-0.1f; motion.a[2]=1;
        motion.valid=true; motion.session=worker_session; ++motion.sequence;
        motion.sampled_ms=clock_ms;
    }
    ui_timer.callback(&ui_timer);
}
static maze_result_t action(maze_action_kind_t kind)
{
    maze_snapshot_t s;
    maze_action_t a;
    maze_result_t result=maze_runtime_snapshot(&s);
    if (result!=MAZE_OK) return result;
    result=maze_runtime_next_request(&a.request_id);
    if (result!=MAZE_OK) return result;
    a.action=kind; a.expected_run_id=s.run_id;
    return maze_runtime_action(NULL,&a);
}

int main(void)
{
    maze_snapshot_t s, paused;
    unsigned i, detached_draws;
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_NOT_INITIALIZED);
    CHECK(maze_runtime_snapshot(&s)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_attach(render,(void *)7)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_detach()==MAZE_WRONG_STATE);
    CHECK(maze_runtime_action(NULL,NULL)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_next_request(&s.run_id)==MAZE_WRONG_STATE);
    CHECK(queue_inits==0 && timers==0 && starts==0);
    printf("TC-408 cold-status-and-unavailable-guards PASS\n");
    CHECK(maze_runtime_init(false)==MAZE_WRONG_STATE && timers==0);
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_SELFTEST_FAILED && queue_inits==0);
    printf("TC-408 selftest-failure-no-resources PASS\n");
    fail_queue=true;
    CHECK(maze_runtime_init(true)==MAZE_INVALID);
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_QUEUE_FAILED);
    CHECK(queue_inits==1 && queue_detaches==0 && timer_attempts==0);
    CHECK(maze_runtime_snapshot(&s)==MAZE_WRONG_STATE && maze_runtime_detach()==MAZE_WRONG_STATE);
    printf("TC-408 queue-failure-no-detach PASS\n");
    fail_queue=false; fail_timer=true;
    CHECK(maze_runtime_init(true)==MAZE_INVALID);
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_TIMER_FAILED);
    CHECK(queue_inits==2 && queue_detaches==1 && timers==0);
    CHECK(maze_runtime_attach(render,(void *)7)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_action(NULL,NULL)==MAZE_WRONG_STATE);
    fail_timer=false;
    CHECK(maze_runtime_init(true)==MAZE_OK && timers==1);
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_READY && queue_inits==3 && queue_detaches==1);
    printf("TC-408 timer-cleanup-and-retry PASS\n");
    CHECK(maze_runtime_init(true)==MAZE_WRONG_STATE && timers==1);
    CHECK(maze_runtime_init(false)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_READY && queue_inits==3 && timer_attempts==2);
    printf("TC-408 ready-not-overwritten PASS\n");
    CHECK(maze_runtime_cancel_startup()==MAZE_OK && timers==0 && queue_detaches==2);
    CHECK(maze_runtime_init_status()==MAZE_RUNTIME_NOT_INITIALIZED);
    CHECK(maze_runtime_snapshot(&s)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_init(true)==MAZE_OK && timers==1);
    printf("TC-408 startup-cancel-cleanup-retry PASS\n");
    CHECK(maze_runtime_attach(render,(void *)7)==MAZE_OK);
    CHECK(maze_runtime_cancel_startup()==MAZE_WRONG_STATE && timers==1);
    CHECK(maze_runtime_attach(render,(void *)7)==MAZE_WRONG_STATE);
    current_thread=(void *)2;
    CHECK(maze_runtime_snapshot(&s)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_next_request(&s.run_id)==MAZE_WRONG_STATE);
    CHECK(maze_runtime_action(NULL,NULL)==MAZE_WRONG_STATE);
    current_thread=(void *)1;
    CHECK(action(MAZE_ACTION_START)==MAZE_OK && starts==1);
    CHECK(maze_runtime_cancel_startup()==MAZE_WRONG_STATE && timers==1);
    for (i=0;i<32;++i) pump(20,true);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_RUNNING && s.elapsed_ms==0);
    printf("TC-406 runtime-single-owner-and-timer PASS\n");
    pump(20,true);
    CHECK(maze_runtime_detach()==MAZE_OK && stop_requested);
    CHECK(maze_runtime_snapshot(&paused)==MAZE_OK && paused.state==MAZE_PAUSED);
    detached_draws=draws;
    CHECK(maze_runtime_attach(render,(void *)7)==MAZE_OK);
    pump(20,true); /* Reopen before old worker has observed Stop. */
    CHECK(starts==1 && maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_PAUSED);
    CHECK(maze_runtime_detach()==MAZE_OK);
    detached_draws=draws;
    busy=false; motion.valid=false; /* Worker has restored its settings. */
    pump(500,false);
    CHECK(draws==detached_draws);
    CHECK(maze_runtime_attach(render,(void *)7)==MAZE_OK);
    pump(20,false); /* Start a new acquisition, clear old publication. */
    CHECK(starts==2);
    CHECK(action(MAZE_ACTION_RESUME)==MAZE_WRONG_STATE);
    pump(20,true);
    CHECK(action(MAZE_ACTION_RESUME)==MAZE_OK);
    pump(20,true);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.run_id==paused.run_id);
    CHECK(s.elapsed_ms==paused.elapsed_ms+20 && timers==1);
    printf("TC-406 detach-reopen-no-reset-no-callback PASS\n");
    CHECK(action(MAZE_ACTION_BACK)==MAZE_OK && stop_requested);
    busy=true; worker_session=0; motion.valid=false; /* Another job acquired the slot. */
    pump(20,false);
    CHECK(wrong_stops==0 && busy);
    CHECK(action(MAZE_ACTION_START)==MAZE_OK);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_ERROR && s.error_code==MAZE_ERROR_INPUT);
    CHECK(wrong_stops==0 && busy);
    printf("TC-405 busy-slot-and-stop-ownership PASS\n");
    {
        char *argv[]={"maze_diag","status"};
        current_thread=(void *)2;
        for(i=0;i<9;++i) test_export_maze_diag(2,argv);
        CHECK(queue->count==8);
        current_thread=(void *)1;
        pump(20,false);
        CHECK(queue->count==0);
    }
    CHECK(timers==1 && starts==2 && stops==2);
    CHECK(truncated_logs==0 && complete_input_logs>0);
    printf("TC-406 board-console-no-truncation PASS\n");
    printf("TC-406 shell-bounded-ui-queue PASS\n");
    busy=false;motion.valid=false;
    CHECK(action(MAZE_ACTION_RESTART)==MAZE_OK);
    for(i=0;i<32;++i)pump(20,true);
    pump(101,true);
    CHECK(timing_logs==1 && strstr(loop_line,"valid=0 rv=1"));
    CHECK(strstr(timing_line,"gap=101"));
    printf("TC-405 timing-unmeasured-loop-explicitly-marked PASS\n");
    unsigned faults=timing_logs;
    busy=false; motion.valid=false;
    CHECK(action(MAZE_ACTION_RESTART)==MAZE_OK);
    for(i=0;i<32;++i)pump(20,true);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_RUNNING);
    pump(100,true); /* Original inclusive dt boundary is retained. */
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_RUNNING && s.elapsed_ms==100);
    render_cost=130; pump(20,true); render_cost=0;
    maze_runtime_note_loop(135,2);
    pump(2,true);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_ERROR && s.error_code==MAZE_ERROR_TIMING);
    CHECK(s.elapsed_ms==120 && timing_logs==faults+1);
    CHECK(strstr(timing_line,"gap=132") && strstr(timing_line,"render=130"));
    CHECK(strstr(loop_line,"handler=135 wait=2") && strstr(loop_line,"valid=1 rv=1"));
    pump(500,false); CHECK(timing_logs==faults+1); /* No print per error tick. */
    CHECK(truncated_logs==0);
    printf("TC-404 timing-render-cost-and-unchanged-100ms-limit PASS\n");
    busy=false; motion.valid=false;
    CHECK(action(MAZE_ACTION_RESTART)==MAZE_OK);
    for(i=0;i<32;++i)pump(20,true);
    maze_runtime_note_loop(3,101);
    current_thread=(void *)2; maze_runtime_note_loop(999,999); current_thread=(void *)1;
    clock_ms+=101;
    CHECK(action(MAZE_ACTION_PAUSE)==MAZE_INVALID);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.error_code==MAZE_ERROR_TIMING && s.elapsed_ms==0);
    CHECK(strstr(timing_line,"gap=101") && strstr(loop_line,"handler=3 wait=101"));
    CHECK(timing_logs==faults+2);
    char saved[128],saved_loop[128]; memcpy(saved,timing_line,sizeof(saved));
    memcpy(saved_loop,loop_line,sizeof(saved_loop));
    maze_runtime_note_loop(UINT32_MAX,UINT32_MAX);
    pump(1000,false);
    CHECK(timing_logs==faults+2 && !strcmp(saved,timing_line) && !strcmp(saved_loop,loop_line));
    { char *argv[]={"maze_diag","status"};test_export_maze_diag(2,argv);pump(20,false); }
    CHECK(timing_logs==faults+3 && !strcmp(saved,timing_line) && !strcmp(saved_loop,loop_line));
    CHECK(truncated_logs==0);
    printf("TC-405 timing-pause-fault-frozen-status-and-owner-guard PASS\n");
    busy=false; motion.valid=false;clock_ms=UINT32_MAX-400;
    CHECK(action(MAZE_ACTION_RESTART)==MAZE_OK);
    for(i=0;i<32;++i)pump(20,true);
    CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.state==MAZE_RUNNING);
    pump(20,true);CHECK(maze_runtime_snapshot(&s)==MAZE_OK && s.elapsed_ms==20);
    CHECK(action(MAZE_ACTION_BACK)==MAZE_OK && truncated_logs==0);
    printf("TC-404 timing-wraparound-regression PASS\nsummary PASS failures=0\n");
    return 0;
}
