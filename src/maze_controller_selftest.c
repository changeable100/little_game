#include "maze_controller_selftest.h"
#include "maze_controller.h"
#include <math.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) return false; } while (0)
static const uint8_t cells[] = {
    1,1,1,1,1,1, 1,0,0,0,0,1, 1,0,0,0,0,1,
    1,0,0,0,0,1, 1,0,0,0,0,1, 1,1,1,1,1,1
};
static const maze_level_t level = {401,6,6,36,cells,2,2,4,4};

static maze_result_t action(maze_controller_t *c, maze_action_kind_t kind, uint32_t now)
{
    maze_action_t a;
    maze_result_t result = maze_controller_next_request(c, &a.request_id);
    if (result != MAZE_OK) return result;
    a.action = kind;
    a.expected_run_id = c->game.snapshot.run_id;
    return maze_controller_action(c, &a, now);
}

static maze_motion_t sample(maze_controller_t *c, uint32_t seq, uint32_t time)
{
    maze_motion_t s = {{0.1f,-0.1f,1.0f},{0,0,0},seq,time,0,true};
    s.run_id = c->game.snapshot.run_id;
    return s;
}

static bool calibrated(maze_controller_t *c, uint32_t start)
{
    unsigned i;
    CHECK(maze_controller_init(c, &level, start) == MAZE_OK);
    CHECK(action(c, MAZE_ACTION_START, start) == MAZE_OK);
    for (i = 1; i <= MAZE_CALIBRATION_SAMPLES; ++i)
    {
        maze_motion_t s = sample(c, i, start+i*20);
        CHECK(maze_controller_tick(c, s.sampled_ms, &s) == MAZE_OK);
    }
    CHECK(c->game.snapshot.state == MAZE_RUNNING);
    CHECK(c->game.snapshot.elapsed_ms == 0);
    return true;
}

static bool calibration_window(void)
{
    maze_controller_t c;
    maze_motion_t s;
    unsigned i;
    CHECK(maze_controller_init(&c, &level, 0) == MAZE_OK);
    CHECK(action(&c, MAZE_ACTION_START, 0) == MAZE_OK);
    for (i=1; i<=31; ++i)
    {
        s = sample(&c, i, i*20);
        CHECK(maze_controller_tick(&c, i*20, &s) == MAZE_OK);
        CHECK(c.game.snapshot.state == MAZE_CALIBRATING);
    }
    CHECK(maze_controller_tick(&c, 621, &s) == MAZE_OK);
    CHECK(c.calibration_count == 31);
    s = sample(&c, 32, 640);
    CHECK(maze_controller_tick(&c, 640, &s) == MAZE_OK);
    CHECK(c.game.snapshot.state == MAZE_RUNNING && c.game.snapshot.elapsed_ms == 0);
    CHECK(fabsf(c.bias[0]-0.1f)<0.00001f && fabsf(c.bias[1]+0.1f)<0.00001f);
    return true;
}

static bool unstable_reset(void)
{
    maze_controller_t c;
    unsigned i;
    CHECK(maze_controller_init(&c, &level, 0) == MAZE_OK);
    CHECK(action(&c, MAZE_ACTION_START, 0) == MAZE_OK);
    for (i=1; i<=10; ++i)
    {
        maze_motion_t s = sample(&c, i, i*20);
        CHECK(maze_controller_tick(&c, i*20, &s) == MAZE_OK);
    }
    {
        maze_motion_t s = sample(&c, 11, 220); s.g[0]=6;
        CHECK(maze_controller_tick(&c, 220, &s) == MAZE_OK);
        CHECK(c.calibration_count == 0 && c.game.snapshot.state == MAZE_CALIBRATING);
    }
    for (i=12; i<=43; ++i)
    {
        maze_motion_t s = sample(&c, i, i*20);
        CHECK(maze_controller_tick(&c, i*20, &s) == MAZE_OK);
    }
    CHECK(c.game.snapshot.state == MAZE_RUNNING);
    return true;
}

static bool calibration_timeout(void)
{
    maze_controller_t c;
    CHECK(maze_controller_init(&c, &level, 0) == MAZE_OK);
    CHECK(action(&c, MAZE_ACTION_START, 0) == MAZE_OK);
    CHECK(maze_controller_tick(&c, 4999, NULL) == MAZE_OK);
    CHECK(maze_controller_tick(&c, 5000, NULL) == MAZE_INVALID);
    CHECK(c.game.snapshot.state == MAZE_ERROR && c.game.snapshot.error_code == MAZE_ERROR_INPUT);
    return true;
}

static bool axes(unsigned axis, float sign)
{
    maze_controller_t c;
    unsigned i;
    CHECK(calibrated(&c, 0));
    for (i=33; i<=52; ++i)
    {
        maze_motion_t s = sample(&c, i, i*20);
        /* Lower right screen edge: +ay; lower top edge: +ax. */
        s.a[1u-axis] += (axis ? -sign : sign)*0.5f;
        CHECK(maze_controller_tick(&c, i*20, &s) == MAZE_OK);
    }
    CHECK(axis ? c.input.tilt_y*sign>0.9f : c.input.tilt_x*sign>0.9f);
    CHECK(fabsf(c.input.tilt_x)<=1 && fabsf(c.input.tilt_y)<=1);
    return true;
}

static bool deadband_filter(void)
{
    maze_controller_t c;
    maze_motion_t s;
    CHECK(calibrated(&c, 0));
    s = sample(&c, 33, 660); s.a[1]+=0.02f;
    CHECK(maze_controller_tick(&c, 660, &s) == MAZE_OK && c.input.tilt_x == 0);
    s = sample(&c, 34, 680); s.a[1]+=0.35f;
    CHECK(maze_controller_tick(&c, 680, &s) == MAZE_OK);
    CHECK(c.input.tilt_x>0 && c.input.tilt_x<0.5f);
    CHECK(c.input.tilt_y == 0);
    return true;
}

static bool board_direction_replay(void)
{
    /* run05 seq424 right-edge down; run06 seq244 top-edge down.
     * Use each recording's native calibration bias, not run04 labels. */
    /* After 20 filter steps: ((0.304*(1-0.75^20))-0.03)/0.32 = 0.85324.
     * The top vector saturates at -1; synthetic full-scale tests stay >0.9. */
    static const struct { float a[3]; float bias[2]; unsigned axis; float expected; } cases[] = {
        {{-0.042f,0.309f,-0.895f},{-0.013f,0.005f},0, 0.85324f},
        {{ 0.511f,-0.039f,-0.803f},{-0.005f,-0.062f},1,-1.0f}
    };
    unsigned k,i;
    for (k=0; k<sizeof(cases)/sizeof(cases[0]); ++k)
    {
        maze_controller_t c;
        CHECK(maze_controller_init(&c,&level,0)==MAZE_OK);
        CHECK(action(&c,MAZE_ACTION_START,0)==MAZE_OK);
        for (i=1; i<=52; ++i)
        {
            maze_motion_t s=sample(&c,i,i*20);
            if (i<=32) { s.a[0]=cases[k].bias[0]; s.a[1]=cases[k].bias[1]; s.a[2]=-0.963f; }
            else memcpy(s.a,cases[k].a,sizeof(s.a));
            CHECK(maze_controller_tick(&c,i*20,&s)==MAZE_OK);
        }
        CHECK(c.game.snapshot.state==MAZE_RUNNING);
        CHECK(fabsf((cases[k].axis ? c.input.tilt_y : c.input.tilt_x)-cases[k].expected)<0.001f);
    }
    return true;
}

static bool bad_sample(unsigned kind)
{
    maze_controller_t c;
    maze_motion_t s;
    float x;
    CHECK(calibrated(&c, 0));
    x = c.game.snapshot.ball_x;
    s = sample(&c, 33, 660);
    switch (kind)
    {
    case 0: s.valid=false; break;
    case 1: s.a[0]=NAN; break;
    case 2: s.g[1]=INFINITY; break;
    case 3: s.sampled_ms=500; break;
    case 4: s.sequence=31; break;
    case 5: s.sampled_ms=700; break;
    default: s.a[2]=5; break;
    }
    CHECK(maze_controller_tick(&c, 660, &s) == MAZE_INVALID);
    CHECK(c.game.snapshot.state == MAZE_ERROR && c.game.snapshot.error_code == MAZE_ERROR_INPUT);
    CHECK(c.game.snapshot.elapsed_ms==0 && c.game.snapshot.ball_x==x);
    return true;
}

static bool pause_clock(void)
{
    maze_controller_t c;
    maze_motion_t s;
    CHECK(calibrated(&c, 0));
    s=sample(&c,33,660);
    CHECK(maze_controller_tick(&c,660,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_PAUSE,670)==MAZE_OK && c.game.snapshot.elapsed_ms==30);
    CHECK(maze_controller_tick(&c,10000,NULL)==MAZE_OK && c.game.snapshot.elapsed_ms==30);
    s=sample(&c,34,10020);
    CHECK(maze_controller_tick(&c,10020,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_RESUME,10020)==MAZE_OK);
    s=sample(&c,35,10040);
    CHECK(maze_controller_tick(&c,10040,&s)==MAZE_OK && c.game.snapshot.elapsed_ms==50);
    return true;
}

static bool long_delay(void)
{
    maze_controller_t c;
    maze_motion_t s;
    CHECK(calibrated(&c, 0));
    s=sample(&c,33,891);
    CHECK(maze_controller_tick(&c,891,&s)==MAZE_OK);
    CHECK(c.game.snapshot.state==MAZE_PAUSED && c.game.snapshot.error_code==MAZE_ERROR_NONE && c.game.snapshot.elapsed_ms==0);
    CHECK(c.game.snapshot.pause_reason==MAZE_PAUSE_DELAY && c.game.snapshot.interrupted_ms==251);
    CHECK(calibrated(&c, 0));
    s=sample(&c,32,640); /* Tolerated UI gap must not accept an expired input. */
    CHECK(maze_controller_tick(&c,741,&s)==MAZE_INVALID);
    CHECK(c.game.snapshot.error_code==MAZE_ERROR_INPUT && c.game.snapshot.elapsed_ms==0);
    return true;
}

static bool short_delays(void)
{
    static const uint32_t gaps[]={100,104,108,200,250};
    for(unsigned i=0;i<sizeof(gaps)/sizeof(gaps[0]);++i) {
        maze_controller_t c;maze_motion_t s;
        CHECK(calibrated(&c,0));s=sample(&c,33,640+gaps[i]);
        CHECK(maze_controller_tick(&c,640+gaps[i],&s)==MAZE_OK);
        CHECK(c.game.snapshot.state==MAZE_RUNNING && c.game.snapshot.error_code==MAZE_ERROR_NONE);
        CHECK(c.game.snapshot.elapsed_ms==gaps[i]-gaps[i]%5);
    }
    return true;
}

static bool fractional_clock(void)
{
    maze_controller_t c;maze_motion_t s;
    CHECK(calibrated(&c,0));s=sample(&c,33,642);
    CHECK(maze_controller_tick(&c,642,&s)==MAZE_OK && c.game.snapshot.elapsed_ms==0 && c.pending_ms==2);
    s=sample(&c,34,646);CHECK(maze_controller_tick(&c,646,&s)==MAZE_OK);
    CHECK(c.game.snapshot.elapsed_ms==5 && c.pending_ms==1);
    CHECK(action(&c,MAZE_ACTION_PAUSE,648)==MAZE_OK && c.pending_ms==3);
    CHECK(c.game.snapshot.pause_reason==MAZE_PAUSE_USER && c.game.snapshot.elapsed_ms==5);
    CHECK(maze_controller_reconnect(&c,900)==MAZE_OK && c.pending_ms==3);
    s=sample(&c,35,1000);CHECK(maze_controller_tick(&c,1000,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_RESUME,1000)==MAZE_OK);
    s=sample(&c,36,1002);CHECK(maze_controller_tick(&c,1002,&s)==MAZE_OK);
    CHECK(c.game.snapshot.elapsed_ms==10 && c.pending_ms==0);
    return true;
}

static bool delay_recovery(void)
{
    maze_controller_t c;maze_motion_t s;uint32_t run;
    CHECK(calibrated(&c,0));run=c.game.snapshot.run_id;
    s=sample(&c,33,644);CHECK(maze_controller_tick(&c,644,&s)==MAZE_OK && c.pending_ms==4);
    s=sample(&c,34,895);CHECK(maze_controller_tick(&c,895,&s)==MAZE_OK);
    CHECK(c.game.snapshot.state==MAZE_PAUSED && c.game.snapshot.interrupted_ms==255 && c.pending_ms==0);
    CHECK(c.game.snapshot.elapsed_ms==0 && c.game.snapshot.run_id==run);
    CHECK(action(&c,MAZE_ACTION_RESUME,895)==MAZE_WRONG_STATE);
    CHECK(maze_controller_tick(&c,895,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_RESUME,895)==MAZE_WRONG_STATE);
    s=sample(&c,35,915);CHECK(maze_controller_tick(&c,915,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_RESUME,915)==MAZE_OK && c.game.snapshot.pause_reason==MAZE_PAUSE_NONE);
    s=sample(&c,36,935);CHECK(maze_controller_tick(&c,935,&s)==MAZE_OK);
    CHECK(c.game.snapshot.elapsed_ms==20 && c.game.snapshot.interrupted_ms==255);
    CHECK(action(&c,MAZE_ACTION_RESTART,935)==MAZE_OK && c.game.snapshot.interrupted_ms==0);
    return true;
}

static bool delay_saturation(void)
{
    maze_controller_t c;maze_motion_t s;
    CHECK(calibrated(&c,0));c.game.snapshot.interrupted_ms=UINT32_MAX-1;
    s=sample(&c,33,891);CHECK(maze_controller_tick(&c,891,&s)==MAZE_OK);
    CHECK(c.game.snapshot.interrupted_ms==UINT32_MAX);
    s=sample(&c,34,911);CHECK(maze_controller_tick(&c,911,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_RESUME,911)==MAZE_OK);
    s=sample(&c,35,1162);CHECK(maze_controller_tick(&c,1162,&s)==MAZE_OK);
    CHECK(c.game.snapshot.interrupted_ms==UINT32_MAX && c.game.snapshot.elapsed_ms==0);
    return true;
}

static bool delay_wrap(void)
{
    maze_controller_t c;maze_motion_t s;uint32_t start=UINT32_MAX-700;
    CHECK(calibrated(&c,start));s=sample(&c,33,start+748u);
    CHECK(maze_controller_tick(&c,s.sampled_ms,&s)==MAZE_OK && c.game.snapshot.elapsed_ms==105 && c.pending_ms==3);
    s=sample(&c,34,start+999u);CHECK(maze_controller_tick(&c,s.sampled_ms,&s)==MAZE_OK);
    CHECK(c.game.snapshot.pause_reason==MAZE_PAUSE_DELAY && c.game.snapshot.interrupted_ms==254);
    return true;
}

static bool action_publication_guard(void)
{
    maze_controller_t c;maze_motion_t s;maze_action_t a;maze_snapshot_t before;
    CHECK(calibrated(&c,0));before=c.game.snapshot;
    CHECK(maze_controller_next_request(&c,&a.request_id)==MAZE_OK);
    a.action=MAZE_ACTION_PAUSE;a.expected_run_id=0;s=sample(&c,33,840);s.a[0]=NAN;
    CHECK(maze_controller_action_with_sample(&c,&a,840,&s)==MAZE_STALE);
    CHECK(!memcmp(&before,&c.game.snapshot,sizeof(before)) && c.anchor_ms==640);
    CHECK(maze_game_pause_for_delay(&c.game,0,251)==MAZE_STALE);
    CHECK(maze_game_pause_for_delay(&c.game,c.game.snapshot.run_id,0)==MAZE_INVALID);
    return true;
}

static bool clock_wrap(void)
{
    maze_controller_t c;
    maze_motion_t s;
    uint32_t start=UINT32_MAX-650u;
    CHECK(calibrated(&c,start));
    s=sample(&c,33,start+660u);
    CHECK(maze_controller_tick(&c,start+660u,&s)==MAZE_OK);
    CHECK(c.game.snapshot.elapsed_ms==20);
    return true;
}

static bool requests(void)
{
    maze_controller_t c;
    maze_action_t a;
    maze_snapshot_t before;
    CHECK(calibrated(&c,0));
    CHECK(maze_controller_next_request(&c,&a.request_id)==MAZE_OK && a.request_id==2);
    a.action=MAZE_ACTION_PAUSE; a.expected_run_id=0;
    before=c.game.snapshot;
    CHECK(maze_controller_action(&c,&a,660)==MAZE_STALE);
    CHECK(memcmp(&before,&c.game.snapshot,sizeof(before))==0);
    a.expected_run_id=before.run_id;
    CHECK(maze_controller_action(&c,&a,660)==MAZE_OK);
    CHECK(maze_controller_action(&c,&a,670)==MAZE_STALE);
    CHECK(action(&c,MAZE_ACTION_RESTART,680)==MAZE_OK);
    CHECK(c.allocated_request==3 && c.game.snapshot.run_id==2);
    c.allocated_request=UINT32_MAX;
    CHECK(maze_controller_next_request(&c,&a.request_id)==MAZE_EXHAUSTED);
    return true;
}

static bool old_run(void)
{
    maze_controller_t c;
    maze_motion_t s;
    CHECK(calibrated(&c,0));
    s=sample(&c,33,700);
    CHECK(action(&c,MAZE_ACTION_BACK,680)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_START,700)==MAZE_OK);
    CHECK(maze_controller_tick(&c,720,&s)==MAZE_OK && c.calibration_count==0);
    s.run_id=c.game.snapshot.run_id;
    s.sampled_ms=699;
    CHECK(maze_controller_tick(&c,720,&s)==MAZE_OK && c.calibration_count==0);
    CHECK(maze_controller_input_failed(&c,1)==MAZE_STALE);
    CHECK(c.game.snapshot.state==MAZE_CALIBRATING);
    return true;
}

static bool missing_sample(void)
{
    maze_controller_t c;
    CHECK(calibrated(&c,0));
    CHECK(maze_controller_tick(&c,740,NULL)==MAZE_OK);
    CHECK(maze_controller_tick(&c,741,NULL)==MAZE_INVALID);
    CHECK(c.game.snapshot.error_code==MAZE_ERROR_INPUT);
    return true;
}

static bool reconnect(void)
{
    maze_controller_t c;
    maze_motion_t s;
    uint32_t allocated;
    CHECK(calibrated(&c,0));
    CHECK(maze_controller_reconnect(&c,650)==MAZE_WRONG_STATE);
    CHECK(action(&c,MAZE_ACTION_PAUSE,660)==MAZE_OK);
    allocated=c.allocated_request;
    CHECK(maze_controller_reconnect(&c,10000)==MAZE_OK);
    CHECK(c.game.snapshot.state==MAZE_PAUSED && c.game.snapshot.elapsed_ms==20);
    CHECK(c.game.snapshot.run_id==1 && c.allocated_request==allocated);
    CHECK(action(&c,MAZE_ACTION_RESUME,10000)==MAZE_WRONG_STATE);
    s=sample(&c,1,10020); /* A new acquisition may restart its local sequence. */
    CHECK(maze_controller_tick(&c,10020,&s)==MAZE_OK);
    CHECK(action(&c,MAZE_ACTION_RESUME,10020)==MAZE_OK);
    s=sample(&c,2,10040);
    CHECK(maze_controller_tick(&c,10040,&s)==MAZE_OK && c.game.snapshot.elapsed_ms==40);
    return true;
}

static bool duplicate_filter(void)
{
    maze_controller_t c;
    maze_motion_t s;
    float filtered;
    CHECK(calibrated(&c,0));
    s=sample(&c,33,660); s.a[0]+=0.35f;
    CHECK(maze_controller_tick(&c,660,&s)==MAZE_OK);
    filtered=c.filtered[0];
    CHECK(maze_controller_tick(&c,680,&s)==MAZE_OK);
    CHECK(c.filtered[0]==filtered && c.game.snapshot.elapsed_ms==40);
    return true;
}

static bool guards(void)
{
    maze_controller_t c;
    uint32_t id;
    CHECK(maze_controller_init(NULL,&level,0)==MAZE_INVALID);
    CHECK(maze_controller_next_request(NULL,&id)==MAZE_INVALID);
    CHECK(maze_controller_snapshot(NULL,NULL)==MAZE_INVALID);
    CHECK(maze_controller_tick(NULL,0,NULL)==MAZE_INVALID);
    CHECK(maze_controller_init(&c,&level,0)==MAZE_OK);
    CHECK(maze_controller_action(&c,NULL,0)==MAZE_INVALID);
    CHECK(maze_controller_next_request(&c,NULL)==MAZE_INVALID);
    CHECK(maze_controller_input_failed(&c,0)==MAZE_WRONG_STATE);
    CHECK(action(&c,MAZE_ACTION_START,0)==MAZE_OK);
    CHECK(maze_controller_input_failed(&c,1)==MAZE_INVALID);
    CHECK(c.game.snapshot.state==MAZE_ERROR);
    return true;
}

int maze_controller_selftest_run(maze_test_report_t report)
{
    int failures=0;
#define RUN(name, expression) do { bool ok=(expression); if(!ok) ++failures; if(report) report(name,ok); } while(0)
    RUN("TC-401 request-stale-exhaustion",requests());
    RUN("TC-401 old-run-and-sample",old_run());
    RUN("TC-402 calibration-32-unique",calibration_window());
    RUN("TC-402 unstable-restarts-window",unstable_reset());
    RUN("TC-402 timeout",calibration_timeout());
    RUN("TC-403 right",axes(0,1));
    RUN("TC-403 left",axes(0,-1));
    RUN("TC-403 down",axes(1,1));
    RUN("TC-403 up",axes(1,-1));
    RUN("TC-403 deadband-filter",deadband_filter());
    RUN("TC-403 board-edge-direction-replay",board_direction_replay());
    RUN("TC-403 duplicate-not-refiltered",duplicate_filter());
    RUN("TC-403 invalid",bad_sample(0));
    RUN("TC-403 nan",bad_sample(1));
    RUN("TC-403 infinity",bad_sample(2));
    RUN("TC-403 stale-sample",bad_sample(3));
    RUN("TC-403 reversed-sequence",bad_sample(4));
    RUN("TC-403 future-timestamp",bad_sample(5));
    RUN("TC-403 range",bad_sample(6));
    RUN("TC-403 missing-sample",missing_sample());
    RUN("TC-404 pause-resume-clock",pause_clock());
    RUN("TC-503 short-delays-bounded-catchup",short_delays());
    RUN("TC-503 long-delay-recoverable-pause",long_delay());
    RUN("TC-503 fractional-time-survives-pause-reconnect",fractional_clock());
    RUN("TC-503 fresh-sample-resume-and-run-reset",delay_recovery());
    RUN("TC-503 interruption-counter-saturation",delay_saturation());
    RUN("TC-503 delay-clock-wrap",delay_wrap());
    RUN("TC-503 stale-intent-before-publication",action_publication_guard());
    RUN("TC-404 clock-wrap",clock_wrap());
    RUN("TC-406 api-and-calibration-failure",guards());
    RUN("TC-406 reconnect-preserves-game",reconnect());
#undef RUN
    return failures;
}
