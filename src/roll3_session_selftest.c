#include "roll3_session.h"
#include "roll3_camera_basis.h"
#include <math.h>
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

static r3_vec_t settled_input(roll3_session_t *s, unsigned axis, float tilt, uint32_t *now, uint32_t *seq)
{
    for(unsigned i=0;i<64;++i){*now+=20;lab_motion_t m=sample(s,++*seq,*now);
        m.a[axis]=tilt;roll3_session_update(s,*now,&m,true);}
    return s->input.acceleration;
}
static bool gentle_input(void)
{
    roll3_session_t s;
    if(!calibrated(&s,100))return false;
    uint32_t now=740,seq=32;
    if(roll3_session_action(&s,R3_PAUSE,s.game.s.run,now)!=R3_OK)return false;
    r3_vec_t small=settled_input(&s,1,.15f,&now,&seq);
    r3_vec_t dead=settled_input(&s,1,.03f,&now,&seq);
    r3_vec_t right=settled_input(&s,1,.45f,&now,&seq);
    r3_vec_t left=settled_input(&s,1,-.45f,&now,&seq);
    return hypotf(small.x,small.y)>=.8f&&hypotf(small.x,small.y)<=1.1f&&
           dead.x==0&&dead.y==0&&hypotf(right.x,right.y)>4.89f&&
           hypotf(left.x,left.y)>4.89f&&left.x<0&&right.x>0;
}
static bool screen_basis(void)
{
    roll3_session_t s;
    if (!calibrated(&s, 100)) return false;
    uint32_t now=740, seq=32;
    if (roll3_session_action(&s, R3_PAUSE, s.game.s.run, now)!=R3_OK) return false;
    r3_vec_t right=settled_input(&s,1,.45f,&now,&seq);
    r3_vec_t up=settled_input(&s,0,.45f,&now,&seq);
    return right.x*R3_CAM_COS-right.y*R3_CAM_SIN>4.89f &&
           fabsf(right.x*R3_CAM_SIN+right.y*R3_CAM_COS)<.001f &&
           up.x*R3_CAM_SIN+up.y*R3_CAM_COS>4.89f &&
           fabsf(up.x*R3_CAM_COS-up.y*R3_CAM_SIN)<.001f;
}
static bool prompt_tilt_response(void)
{
    roll3_session_t s;
    if (!calibrated(&s,100)) return false;
    lab_motion_t m=sample(&s,33,760);
    m.a[1]=.12f;
    roll3_session_update(&s,760,&m,true);
    if(s.input.acceleration.x<=.15f || s.game.s.velocity.x<=0) return false;
    m=sample(&s,34,780);
    m.a[1]=-.12f;
    roll3_session_update(&s,780,&m,true);
    if(s.input.acceleration.x>=0) return false;
    m=sample(&s,35,800);
    roll3_session_update(&s,800,&m,true);
    return s.input.acceleration.x==0 && s.input.acceleration.y==0;
}

static bool diagnose_running_input(void)
{
    roll3_session_t s;
    if (!calibrated(&s,100)) return false;
    roll3_session_update(&s,840,NULL,true);
    if (s.game.s.state!=R3_RUNNING || s.input_fault!=ROLL3_INPUT_FAULT_NONE) return false;
    roll3_session_update(&s,841,NULL,true);
    if (s.game.s.state!=R3_ERROR || s.input_fault!=ROLL3_INPUT_FAULT_SAMPLE_TIMEOUT) return false;
    if (roll3_session_action(&s,R3_RESTART,s.game.s.run,850)!=R3_OK ||
        s.input_fault!=ROLL3_INPUT_FAULT_NONE) return false;

    if (!calibrated(&s,100)) return false;
    lab_motion_t invalid=sample(&s,33,760);
    invalid.valid=false;
    roll3_session_update(&s,760,&invalid,true);
    if (s.game.s.state!=R3_ERROR || s.input_fault!=ROLL3_INPUT_FAULT_INVALID_SAMPLE) return false;

    if (!calibrated(&s,100)) return false;
    lab_motion_t reordered=sample(&s,31,760);
    roll3_session_update(&s,760,&reordered,true);
    return s.game.s.state==R3_ERROR && s.input_fault==ROLL3_INPUT_FAULT_SEQUENCE;
}

static bool light_tilt_progression(void)
{
    roll3_session_t s;
    if(!calibrated(&s,100))return false;
    uint32_t now=760,seq=33;
    lab_motion_t m=sample(&s,seq,now);
    m.a[1]=.075f;
    roll3_session_update(&s,now,&m,true);
    float first=hypotf(s.input.acceleration.x,s.input.acceleration.y);
    if(first<=.08f||first>.16f)return false;
    if(roll3_session_action(&s,R3_PAUSE,s.game.s.run,now)!=R3_OK)return false;
    r3_vec_t medium=settled_input(&s,1,.30f,&now,&seq);
    r3_vec_t full=settled_input(&s,1,.45f,&now,&seq);
    float middle=hypotf(medium.x,medium.y);
    return middle>=2.3f&&middle<=2.6f&&hypotf(full.x,full.y)>=4.89f;
}

static bool h1_tilt_curve_and_diagonal(void)
{
    roll3_session_t s;
    if (!calibrated(&s, 100)) return false;
    uint32_t now=740,seq=32;
    if (roll3_session_action(&s,R3_PAUSE,s.game.s.run,now)!=R3_OK) return false;
    r3_vec_t fine=settled_input(&s,1,.15f,&now,&seq);
    r3_vec_t middle=settled_input(&s,1,.30f,&now,&seq);
    float fine_acc=hypotf(fine.x,fine.y),mid_acc=hypotf(middle.x,middle.y);
    if (fine_acc<.80f||fine_acc>.83f||mid_acc<2.30f||mid_acc>2.40f) return false;
    for(unsigned i=0;i<64;++i){
        now+=20;
        lab_motion_t m=sample(&s,++seq,now);
        m.a[0]=m.a[1]=.45f;
        roll3_session_update(&s,now,&m,true);
    }
    r3_vec_t a=s.input.acceleration;
    float right=a.x*R3_CAM_COS-a.y*R3_CAM_SIN;
    float forward=a.x*R3_CAM_SIN+a.y*R3_CAM_COS;
    float length=hypotf(a.x,a.y);
    return length>=4.89f&&length<=4.9001f&&right>0&&fabsf(right-forward)<.001f;
}

static bool h1_neutral_brake_on_flat(void)
{
    roll3_session_t s;
    if (!calibrated(&s,100)) return false;
    r3_vec_t start=s.game.s.position;
    s.game.s.velocity=(r3_vec_t){1,0,0};
    s.game.s.omega=(r3_vec_t){0,1/R3_RADIUS,0};
    s.game.s.grounded=true;
    s.game.s.contact_normal=(r3_vec_t){0,0,1};
    r3_game_t coast=s.game;
    for(unsigned step=1;step<=800/R3_STEP_MS;++step){
        uint32_t now=740+step*R3_STEP_MS;
        lab_motion_t neutral=sample(&s,32+step/4,now);
        roll3_session_update(&s,now,step%4==0?&neutral:NULL,true);
        if(s.game.s.state!=R3_RUNNING||s.game.s.velocity.x<-.0001f||
           r3_step(&coast,&s.input,R3_STEP_MS)!=R3_OK) return false;
    }
    float speed=hypotf(s.game.s.velocity.x,s.game.s.velocity.y);
    float distance=hypotf(s.game.s.position.x-start.x,s.game.s.position.y-start.y);
    return speed<=.30f&&distance<=.55f&&s.game.s.velocity.x<coast.s.velocity.x&&
           s.game.s.position.x<coast.s.position.x;
}

static bool h1_brake_scope(void)
{
    for(unsigned mode=0;mode<3;++mode){
        roll3_session_t s;
        if (!calibrated(&s,100)) return false;
        s.game.s.velocity=(r3_vec_t){.8f,0,0};
        if(mode==0){
            s.game.s.position.z+=1; /* Airborne. */
        }else if(mode==1){
            const r3_box_t *ramp=&r3_demo_world()->boxes[1];
            r3_vec_t normal=ramp->axis[2];
            float clearance=ramp->half.z+R3_RADIUS;
            s.game.s.position=(r3_vec_t){ramp->center.x+clearance*normal.x,
                ramp->center.y+clearance*normal.y,ramp->center.z+clearance*normal.z};
            s.game.s.contact_normal=normal;
            s.game.s.grounded=true;
        }else{
            s.game.s.grounded=true;
            s.game.s.contact_normal=(r3_vec_t){0,0,1};
            s.input.acceleration.x=-1; /* Active counter-tilt. */
        }
        r3_game_t expected=s.game;
        if(r3_step(&expected,&s.input,R3_STEP_MS)!=R3_OK)return false;
        roll3_session_update(&s,740+R3_STEP_MS,NULL,true);
        r3_vec_t actual=s.game.s.velocity,reference=expected.s.velocity;
        if(s.game.s.state!=expected.s.state||actual.x!=reference.x||
           actual.y!=reference.y||actual.z!=reference.z)return false;
    }
    return true;
}

static bool same_position(r3_vec_t a,r3_vec_t b)
{
    return a.x==b.x&&a.y==b.y&&a.z==b.z;
}

static bool auto_respawn_case(bool checkpoint)
{
    roll3_session_t s;
    uint32_t start=checkpoint?UINT32_MAX-652:100;
    if (!calibrated(&s,start)) return false;
    uint32_t now=start+640,seq=32;
    if(checkpoint){
        s.game.s.position=s.game.world->checkpoint_spawn;
        now+=R3_STEP_MS;
        lab_motion_t m=sample(&s,++seq,now);
        roll3_session_update(&s,now,&m,true);
        if(s.game.s.checkpoint!=1) return false;
    }
    s.game.s.position.z=s.game.world->fall_z-.1f;
    now+=R3_STEP_MS+2;
    lab_motion_t dropped=sample(&s,++seq,now);
    roll3_session_update(&s,now,&dropped,true);
    if(s.game.s.state!=R3_FALLEN||s.game.s.falls!=1||s.remainder||s.fallen_at!=now)return false;
    r3_snapshot_t fallen=s.game.s;
    uint32_t due=now+ROLL3_AUTO_RESPAWN_MS;
    lab_motion_t early=sample(&s,++seq,due-1);
    roll3_session_update(&s,due-1,checkpoint?NULL:&early,true);
    if(roll3_session_try_auto_respawn(&s,due-1)||s.game.s.state!=R3_FALLEN)return false;
    if(checkpoint){
        if(roll3_session_try_auto_respawn(&s,due)||s.game.s.state!=R3_FALLEN)return false;
        due+=20; /* Wait for a fresh publication across the tick wrap. */
    }
    lab_motion_t fresh_sample=sample(&s,++seq,due);
    roll3_session_update(&s,due,&fresh_sample,true);
    if(!roll3_session_try_auto_respawn(&s,due)||roll3_session_try_auto_respawn(&s,due))return false;
    const r3_snapshot_t *respawn=&s.game.s;
    r3_vec_t expected=checkpoint?s.game.world->checkpoint_spawn:s.game.world->spawn;
    const r3_vec_t zero={0,0,0};
    return respawn->state==R3_RUNNING&&same_position(respawn->position,expected)&&
           respawn->run==fallen.run&&respawn->checkpoint==fallen.checkpoint&&
           respawn->falls==fallen.falls&&respawn->elapsed_ms==fallen.elapsed_ms&&
           same_position(respawn->velocity,zero)&&same_position(respawn->omega,zero)&&
           !s.remainder&&s.anchor==due;
}

static bool manual_respawn_before_timeout(void)
{
    roll3_session_t s;
    if (!calibrated(&s,100)) return false;
    s.game.s.position.z=s.game.world->fall_z-.1f;
    roll3_session_update(&s,745,NULL,true);
    if (s.game.s.state!=R3_FALLEN) return false;
    return roll3_session_action(&s,R3_RESPAWN,s.game.s.run,760)==R3_OK&&
           s.game.s.state==R3_RUNNING&&!roll3_session_try_auto_respawn(&s,1345);
}

int roll3_session_selftest(r3_test_report_t report)
{
    bool axis = axes(), clock = timing(), life = lifecycle(), gentle=gentle_input(), basis=screen_basis();
    bool response=prompt_tilt_response(),diagnostics=diagnose_running_input();
    bool light=light_tilt_progression();
    bool curve=h1_tilt_curve_and_diagonal(),brake=h1_neutral_brake_on_flat(),scope=h1_brake_scope();
    bool auto_start=auto_respawn_case(false),auto_checkpoint=auto_respawn_case(true);
    bool manual=manual_respawn_before_timeout();
    if (report) {
        report("TC-504 calibration-session-and-axes", axis);
        report("TC-504 clock-wrap-catchup-heartbeat", clock);
        report("TC-504 input-timeout-and-old-run", life);
        report("TC-506 gentle-input-and-full-uphill-range",gentle);
        report("TC-507 oblique-right-up-input",basis);
        report("TC-508 first-sample-tilt-reverse-neutral",response);
        report("TC-511 running-input-faults-and-restart",diagnostics);
        report("TC-512 light-first-sample-medium-full-tilt",light);
        report("TC-514 midrange-and-diagonal-cap",curve);
        report("TC-514 flat-neutral-brake",brake);
        report("TC-514 slope-air-and-counter-tilt",scope);
        report("TC-515 auto-start-at-600ms",auto_start);
        report("TC-515 auto-checkpoint-fresh-wrap-and-counters",auto_checkpoint);
        report("TC-515 manual-respawn-still-available",manual);
    }
    return !axis + !clock + !life + !gentle + !basis + !response + !diagnostics + !light + !curve + !brake + !scope + !auto_start + !auto_checkpoint + !manual;
}
