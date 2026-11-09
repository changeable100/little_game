#include "roll3_session.h"
#include "roll3_scene.h"
#include "roll3_selftest.h"
#include <stddef.h>

static lab_motion_t sample(const roll3_session_t *s, uint32_t seq, uint32_t now)
{
    return (lab_motion_t){
        .a = {0, 0, 1}, .sequence = seq, .sampled_ms = now,
        .session = s->game.s.run | LAB_ROLL3_SESSION_BIT, .valid = true
    };
}

static bool start(roll3_session_t *s, uint32_t now)
{
    return roll3_session_init(s, r3_demo_world()) == R3_OK &&
           roll3_session_action(s, R3_START, 0, now) == R3_OK;
}

static bool calibrated(roll3_session_t *s, uint32_t start_ms)
{
    if (!start(s, start_ms)) return false;
    for (unsigned i = 1; i <= 32; ++i) {
        uint32_t now = start_ms + i * 20;
        lab_motion_t m = sample(s, i, now);
        m.session = s->game.s.run;
        roll3_session_update(s, now, &m, true);
        if (s->stable != i - 1) return false;
        m.session |= LAB_ROLL3_SESSION_BIT;
        roll3_session_update(s, now, &m, true);
        roll3_session_update(s, now, &m, true);
        if (s->stable != i) return false;
    }
    return s->game.s.state == R3_RUNNING && s->game.s.elapsed_ms == 0;
}

static bool axes(void)
{
    roll3_session_t s;
    if (!calibrated(&s, 100)) return false;
    lab_motion_t m = sample(&s, 33, 745);
    m.a[0] = 0.3f;
    m.a[1] = -0.3f;
    roll3_session_update(&s, 745, &m, true);
    return s.input.acceleration.x < 0 && s.input.acceleration.y > 0 &&
           s.game.s.elapsed_ms == 5;
}

static bool timing(void)
{
    roll3_session_t s;
    if (!calibrated(&s, UINT32_MAX - 640)) return false;
    lab_motion_t m = sample(&s, 33, 3);
    roll3_session_update(&s, 3, &m, true);
    if (s.game.s.elapsed_ms != 0 || s.remainder != 4) return false;
    m = sample(&s, 34, 253);
    roll3_session_update(&s, 253, &m, true);
    if (s.game.s.elapsed_ms != 250 || s.remainder != 4) return false;
    m = sample(&s, 35, 504);
    roll3_session_update(&s, 504, &m, true);
    if (s.game.s.state != R3_PAUSED || s.game.s.elapsed_ms != 250 || s.remainder != 0)
        return false;
    if (roll3_session_action(&s, R3_RESUME, s.game.s.run, 504) != R3_WRONG_STATE)
        return false;
    m = sample(&s, 36, 509);
    roll3_session_update(&s, 509, &m, true);
    if (roll3_session_action(&s, R3_RESUME, s.game.s.run, 509) != R3_OK) return false;
    roll3_session_update(&s, 514, &m, false);
    return s.game.s.state == R3_PAUSED && s.game.s.elapsed_ms == 250;
}

static bool lifecycle(void)
{
    roll3_session_t s;
    if (!start(&s, 100)) return false;
    roll3_session_update(&s, 5100, NULL, true);
    uint32_t old = s.game.s.run;
    if (s.game.s.state != R3_ERROR || roll3_session_needs_motion(&s)) return false;
    if (roll3_session_action(&s, R3_BACK, old, 5105) != R3_OK) return false;
    if (roll3_session_action(&s, R3_START, old, 5105) != R3_STALE) return false;
    return s.game.s.state == R3_READY && !roll3_session_needs_motion(&s);
}

static float settled_input(roll3_session_t *s,float tilt,uint32_t *now,uint32_t *seq)
{
    for(unsigned i=0;i<64;++i){*now+=20;lab_motion_t m=sample(s,++*seq,*now);
        m.a[1]=tilt;roll3_session_update(s,*now,&m,true);}
    return s->input.acceleration.x;
}
static bool gentle_input(void)
{
    roll3_session_t s;
    if(!calibrated(&s,100))return false;
    uint32_t now=740,seq=32;
    if(roll3_session_action(&s,R3_PAUSE,s.game.s.run,now)!=R3_OK)return false;
    float small=settled_input(&s,.15f,&now,&seq);
    float dead=settled_input(&s,.03f,&now,&seq);
    float right=settled_input(&s,.45f,&now,&seq);
    float left=settled_input(&s,-.45f,&now,&seq);
    return small>=.8f&&small<=1.1f&&dead==0&&right>4.89f&&left<-4.89f;
}
int roll3_session_selftest(maze_test_report_t report)
{
    bool axis = axes(), clock = timing(), life = lifecycle(), gentle=gentle_input();
    if (report) {
        report("TC-504 calibration-session-and-axes", axis);
        report("TC-504 clock-wrap-catchup-heartbeat", clock);
        report("TC-504 input-timeout-and-old-run", life);
        report("TC-506 gentle-input-and-full-uphill-range",gentle);
    }
    return !axis + !clock + !life + !gentle;
}
