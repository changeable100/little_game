/* Real runtime/controller/model + real SDK LVGL. Only RT/sensor transport
 * and marked link fault injection are fake. No physical conclusions. */
#include "maze_ui_runtime.h"
#include "maze_runtime.h"
#include "maze_menu.h"
#include "maze_levels.h"
#include "lab_game.h"
#include "rtthread.h"
#include "lvgl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static uint32_t clock_ms, worker_run;
static bool busy, stopping;
static lab_motion_t sample;
static int status_override=-1, read_fail, action_fail, next_fail;
static unsigned actions, allocations, reads, foreign_draws;
static uint32_t seen_run, last_id;
static uint8_t buffer[390*450*4], pixels[390*450*4];
rt_thread_t rt_thread_self(void){return (void *)1;}
uint32_t rt_tick_get_millisecond(void){return clock_ms;}
int rt_kprintf(const char *fmt,...){(void)fmt;return 0;}
rt_err_t rt_mq_init(struct rt_messagequeue *q,const char *n,void *p,size_t z,size_t c,unsigned f)
{(void)n;(void)p;(void)z;(void)c;(void)f;memset(q,0,sizeof(*q));return RT_EOK;}
rt_err_t rt_mq_detach(struct rt_messagequeue *q){(void)q;return RT_EOK;}
rt_err_t rt_mq_send(struct rt_messagequeue *q,void *p,size_t n)
{if(q->count==8||n!=4)return -1;memcpy(&q->values[(q->head+q->count++)%8],p,4);return 0;}
rt_err_t rt_mq_recv(struct rt_messagequeue *q,void *p,size_t n,int w)
{(void)w;if(!q->count||n!=4)return -1;memcpy(p,&q->values[q->head],4);q->head=(q->head+1)%8;--q->count;return 0;}
bool lab_maze_motion_state(uint32_t r,bool *b,bool *o){*b=busy;*o=busy&&r==worker_run;return true;}
bool lab_maze_motion_start(uint32_t r){if(busy)return false;busy=true;worker_run=r;memset(&sample,0,sizeof(sample));return true;}
bool lab_maze_motion_stop(uint32_t r){assert(busy&&r==worker_run);stopping=true;return true;}
bool lab_motion_try_snapshot(lab_motion_t *s){*s=sample;return true;}
extern maze_result_t __real_maze_runtime_snapshot(maze_snapshot_t *);
extern maze_result_t __real_maze_runtime_next_request(uint32_t *);
extern maze_result_t __real_maze_runtime_action(void *,const maze_action_t *);
extern maze_runtime_init_status_t __real_maze_runtime_init_status(void);
maze_result_t __wrap_maze_runtime_snapshot(maze_snapshot_t *s)
{++reads;if(read_fail){--read_fail;return MAZE_WRONG_STATE;}return __real_maze_runtime_snapshot(s);}
maze_result_t __wrap_maze_runtime_next_request(uint32_t *r)
{++allocations;if(next_fail)return (maze_result_t)next_fail;return __real_maze_runtime_next_request(r);}
maze_result_t __wrap_maze_runtime_action(void *c,const maze_action_t *a)
{assert(c==NULL);++actions;seen_run=a->expected_run_id;assert(a->request_id>last_id);last_id=a->request_id;
 return action_fail?(maze_result_t)action_fail:__real_maze_runtime_action(c,a);}
maze_runtime_init_status_t __wrap_maze_runtime_init_status(void)
{return status_override>=0?(maze_runtime_init_status_t)status_override:__real_maze_runtime_init_status();}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p)
{for(int y=a->y1;y<=a->y2;++y){memcpy(pixels+(y*390+a->x1)*4,p,(a->x2-a->x1+1)*4);p+=(a->x2-a->x1+1)*4;}lv_display_flush_ready(d);}
static lv_obj_t *find(lv_obj_t *o,const char *s)
{if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),s))return o;
 for(unsigned i=0;i<lv_obj_get_child_count(o);++i){lv_obj_t *f=find(lv_obj_get_child(o,i),s);if(f)return f;}return NULL;}
static lv_obj_t *button(const char *s)
{lv_obj_t *l=find(lv_layer_top(),s);if(!l)l=find(lv_screen_active(),s);assert(l);return lv_obj_get_parent(l);}
static void click(const char *s){lv_obj_send_event(button(s),LV_EVENT_CLICKED,NULL);}
static unsigned count(lv_obj_t *o){unsigned n=1;for(unsigned i=0;i<lv_obj_get_child_count(o);++i)n+=count(lv_obj_get_child(o,i));return n;}
static unsigned timers(void){unsigned n=0;for(lv_timer_t *t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static void pump(unsigned n)
{while(n--){clock_ms+=20;if(stopping){busy=stopping=false;sample.valid=false;}
 if(busy){sample.a[2]=1;sample.valid=true;sample.session=worker_run;sample.sampled_ms=clock_ms;++sample.sequence;}
 lv_tick_inc(20);lv_timer_handler();}}
static maze_snapshot_t snapshot(void){maze_snapshot_t s;assert(__real_maze_runtime_snapshot(&s)==MAZE_OK);return s;}
static void shot(const char *name)
{lv_refr_now(NULL);FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n390 450\n255\n");
 for(unsigned i=0;i<sizeof(pixels);i+=4){fputc(pixels[i+2],f);fputc(pixels[i+1],f);fputc(pixels[i],f);}fclose(f);}
static void foreign(void *c,const maze_snapshot_t *s,const maze_level_t *l){(void)c;(void)s;(void)l;++foreign_draws;}
int main(void)
{
 puts("HOST CONNECTION TEST: production runtime.c + adapter + SDK LVGL; RT/sensors fake; marked faults injected");
 lv_init();lv_display_t *d=lv_display_create(390,450);lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);
 lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
 unsigned screen_base=count(lv_screen_active()),top_base=count(lv_layer_top());
 for(int status=0;status<=6;++status){if(status==MAZE_RUNTIME_READY)continue;status_override=status;
  assert(maze_menu_create()==0);assert(lv_obj_has_state(button("ENTER MAZE"),LV_STATE_DISABLED));
  assert(find(lv_screen_active(),maze_ui_runtime_startup_text()));click("HOW TO PLAY");click("BACK TO MENU");
  assert(maze_ui_runtime_open()==MAZE_UI_ERR_NOT_CONNECTED);maze_menu_destroy();}
 status_override=-1;assert(maze_runtime_init(true)==MAZE_OK);unsigned clock_base=timers();
 assert(maze_runtime_attach(foreign,NULL)==0);assert(maze_ui_runtime_open()!=0);pump(1);assert(foreign_draws>0);
 assert(maze_runtime_detach()==0);read_fail=1;assert(maze_ui_runtime_open()!=0);
 assert(maze_runtime_attach(foreign,NULL)==0);assert(maze_runtime_detach()==0);
 puts("TC-408 startup gates and TC-206 attach rejection/owned rollback PASS");
 assert(maze_menu_create()==0);shot("production-home.ppm");click("ENTER MAZE");
 assert(snapshot().state==MAZE_READY&&snapshot().run_id==0);assert(!find(lv_layer_top(),"PREVIEW / SIMULATED DATA"));
 shot("production-ready.ppm");
 /* Duplicate menu event must report this UI error, not old runtime OK. */
 click("ENTER MAZE");assert(find(lv_screen_active(),"Cannot open game (UI -100)"));
 /* Synthetic control-matrix probes only; runtime lifecycle above/below is real. */
 maze_snapshot_t matrix=snapshot();
 for(unsigned st=0;st<6;++st){matrix.state=(maze_state_t)st;matrix.error_code=st==5?MAZE_ERROR_INPUT:MAZE_ERROR_NONE;
  maze_ui_render(&matrix,maze_levels_default());
  assert(!lv_obj_has_state(button("BACK"),LV_STATE_DISABLED));
  assert(lv_obj_has_state(button("START"),LV_STATE_DISABLED)==(st!=0));
  assert(lv_obj_has_state(button(st==3?"RESUME":"PAUSE"),LV_STATE_DISABLED)==(st!=2&&st!=3));
  assert(lv_obj_has_state(button("RESTART"),LV_STATE_DISABLED)==(st<2));
  if(st==4)shot("matrix-WON-synthetic.ppm");if(st==5)shot("matrix-ERROR-synthetic.ppm");}
 matrix.state=MAZE_ERROR;matrix.error_code=MAZE_ERROR_LEVEL;maze_ui_render(&matrix,maze_levels_default());
 assert(lv_obj_has_state(button("RESTART"),LV_STATE_DISABLED));
 matrix=snapshot();maze_ui_render(&matrix,maze_levels_default());
 assert(lv_obj_has_state(button("PAUSE"),LV_STATE_DISABLED));
 click("START");assert(snapshot().state==MAZE_CALIBRATING);shot("production-calibrating.ppm");
 pump(34);assert(snapshot().state==MAZE_RUNNING);
 sample.a[1]=0.2f;pump(70);matrix=snapshot();assert(matrix.ball_x>1.5f);
 lv_obj_update_layout(lv_layer_top());
 lv_obj_t *moving_ball=lv_obj_get_child(lv_obj_get_child(lv_layer_top(),-1),260);
 assert(lv_obj_get_x(moving_ball)==35+(int)(matrix.ball_x*20+0.5f)-4);
 shot("production-running.ppm");
 /* PAUSE settlement can fail and change state. Always reread that state. */
 clock_ms+=200;lv_tick_inc(200);click("PAUSE");
 assert(snapshot().state==MAZE_ERROR&&snapshot().error_code==MAZE_ERROR_TIMING);
 assert(find(lv_layer_top(),"TIMING ERROR: Invalid action; see current state"));
 shot("production-timing-error.ppm");click("RESTART");pump(35);assert(snapshot().state==MAZE_RUNNING);
 assert(maze_ui_runtime_open()==MAZE_UI_ERR_EXISTS);
 unsigned r=reads;click("PAUSE");assert(reads>r&&snapshot().state==MAZE_PAUSED);
 uint32_t elapsed=snapshot().elapsed_ms;pump(20);assert(snapshot().elapsed_ms==elapsed);
 shot("production-paused.ppm");
 assert(maze_ui_runtime_release()==0);pump(1);assert(maze_ui_runtime_open()==0);
 click("RESUME");assert(maze_ui_runtime_last_result()==MAZE_WRONG_STATE);
 assert(find(lv_layer_top(),"Not ready; see current state"));pump(2);click("RESUME");assert(snapshot().state==MAZE_RUNNING);
 uint32_t old=snapshot().run_id;click("RESTART");assert(snapshot().run_id==old+1&&snapshot().state==MAZE_CALIBRATING);
 pump(35);assert(snapshot().state==MAZE_RUNNING);
 for(int fail=-1;fail>=-3;--fail){action_fail=fail;unsigned before=actions;r=reads;click("PAUSE");
  assert(actions==before+1&&reads>r&&snapshot().state==MAZE_RUNNING);assert(maze_ui_runtime_last_result()==fail);}
 action_fail=0;
 maze_snapshot_t stale=snapshot();stale.run_id--;maze_ui_render(&stale,maze_levels_default());
 unsigned before=actions;r=reads;click("PAUSE");assert(actions==before+1&&seen_run==stale.run_id&&reads>r);
 assert(maze_ui_runtime_last_result()==MAZE_STALE);assert(find(lv_layer_top(),"Old intent discarded; not retried"));
 pump(1);assert(find(lv_layer_top(),"Old intent discarded; not retried"));
 action_fail=MAZE_STALE;click("BACK");assert(count(lv_layer_top())>top_base);action_fail=0;
 click("BACK");assert(count(lv_layer_top())==top_base&&snapshot().state==MAZE_READY);pump(2);
 for(unsigned i=0;i<20;++i){click("ENTER MAZE");assert(find(lv_screen_active(),maze_ui_runtime_startup_text()));click("START");pump(35);assert(snapshot().state==MAZE_RUNNING);
  lv_obj_delete(lv_obj_get_child(lv_layer_top(),-1));assert(snapshot().state==MAZE_PAUSED);
  assert(maze_ui_runtime_open()==0);assert(snapshot().state==MAZE_PAUSED);pump(2);click("BACK");pump(2);
  assert(timers()==clock_base&&count(lv_layer_top())==top_base);}
 puts("TC-202/205/209 real actions, freshness, stale/no-retry, external deletion, 20 cycles PASS");
 click("ENTER MAZE");lv_event_dsc_t *desc=lv_obj_get_event_dsc(button("START"),0);
 lv_event_cb_t old_cb=lv_event_dsc_get_cb(desc);void *old_ctx=lv_event_dsc_get_user_data(desc);
 maze_ui_runtime_release();assert(maze_ui_runtime_open()==0);lv_obj_t *new_button=button("START");
 lv_obj_remove_event_cb(new_button,old_cb);lv_obj_add_event_cb(new_button,old_cb,LV_EVENT_CLICKED,old_ctx);
 before=actions;lv_obj_send_event(new_button,LV_EVENT_CLICKED,NULL);assert(actions==before);maze_ui_runtime_release();
 assert(maze_ui_runtime_open()==0);click("START");pump(35);maze_menu_destroy();pump(2);
 assert(snapshot().state==MAZE_PAUSED&&count(lv_layer_top())==top_base&&count(lv_screen_active())==screen_base);
 assert(maze_menu_create()==0);click("ENTER MAZE");read_fail=1;click("RESTART");
 assert(lv_obj_has_state(button("BACK"),LV_STATE_DISABLED));maze_ui_runtime_release();pump(2);
 assert(maze_ui_runtime_open()==0);pump(35);next_fail=MAZE_EXHAUSTED;click("PAUSE");
 assert(find(lv_layer_top(),"Requests exhausted; restart device"));before=allocations;click("PAUSE");assert(allocations==before);
 next_fail=0;maze_ui_runtime_release();assert(maze_ui_runtime_open()==0);
 assert(lv_obj_has_state(button("BACK"),LV_STATE_DISABLED));maze_menu_destroy();pump(2);
 assert(timers()==clock_base&&count(lv_layer_top())==top_base&&count(lv_screen_active())==screen_base);
 puts("TC-206 old page event, action reread failure, request exhaustion and menu deletion PASS");
 puts("summary PASS; no physical screen/IMU/WON or heap acceptance claimed");
 return 0;
}
