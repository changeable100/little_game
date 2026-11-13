#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define main game_main
#include "../src/main.c"
#undef main
static jmp_buf done;static char calls[32];static unsigned n,fail;static bool ready;
static void call(char c){calls[n++]=c;calls[n]=0;}
int r3_selftest_run(r3_test_report_t cb){(void)cb;call('K');return fail==1;}
int roll3_renderer_selftest(r3_test_report_t cb){(void)cb;return 0;}
int roll3_session_selftest(r3_test_report_t cb){(void)cb;return 0;}
int r3_records_selftest(void(*cb)(const char*,bool)){(void)cb;call('T');return fail==4;}
int lab_init(void){call('L');return fail==2?-1:0;}
int littlevgl2rtt_init(const char *s){assert(!strcmp(s,"lcd"));call('D');return fail==3?-1:0;}
bool roll3_records_init(void){call('S');return true;}
bool roll3_runtime_init(bool passed){call('R');ready=passed;return passed;}
int roll3_menu_create(bool passed){call('M');assert(passed==ready);return fail==5?-1:0;}
uint32_t lv_timer_handler(void){longjmp(done,1);}
void rt_thread_mdelay(int n){(void)n;assert(false);}
int rt_kprintf(const char *s,...){(void)s;return 0;}
int main(void){
 for(fail=0;fail<=5;++fail){n=0;calls[0]=0;if(!setjmp(done)){int result=game_main();assert(result==-1&&(fail==2||fail==3||fail==5));}
  if(fail==0||fail==1||fail==5)assert(!strcmp(calls,"KLDTSRM"));
  if(fail==4)assert(!strcmp(calls,"KLDTRM"));
  if(fail==1)assert(!ready);
 }
 puts("TC-532 actual main: 3D-only startup, selftest/display failures, records init and game menu PASS");return 0;
}
