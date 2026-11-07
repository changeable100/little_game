/* Exercise the production entry point; service calls are host fakes. */
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define main maze_firmware_main
#include "../src/main.c"
#undef main

#define CHECK(e) do { if (!(e)) { printf("FAIL line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static jmp_buf loop;
static unsigned failed_test;
static int lab_result, display_result, menu_result;
static maze_result_t init_result;
static maze_runtime_init_status_t status;
static char calls[16], output[4096];
static unsigned count, menus, runtime_calls;
static bool received_tests, menu_saw_status;
static void call(char c) { calls[count++]=c; calls[count]=0; }
int maze_selftest_run(maze_test_report_t cb) { (void)cb; return failed_test==1; }
int maze_levels_selftest_run(maze_test_report_t cb) { (void)cb; return failed_test==2; }
int maze_controller_selftest_run(maze_test_report_t cb) { (void)cb; return failed_test==3; }
int lab_init(void) { call('L'); return lab_result; }
int littlevgl2rtt_init(const char *name) { (void)name; call('D'); return display_result; }
maze_result_t maze_runtime_init(bool passed)
{
    call('R'); ++runtime_calls; received_tests=passed;
    status=!passed ? MAZE_RUNTIME_SELFTEST_FAILED :
        (init_result==MAZE_OK ? MAZE_RUNTIME_READY : MAZE_RUNTIME_TIMER_FAILED);
    return passed ? init_result : MAZE_WRONG_STATE;
}
maze_runtime_init_status_t maze_runtime_init_status(void) { return status; }
maze_result_t maze_runtime_cancel_startup(void) { call('C'); status=MAZE_RUNTIME_NOT_INITIALIZED; return MAZE_OK; }
int maze_menu_create(void)
{
    call('M'); ++menus;
    menu_saw_status=status!=MAZE_RUNTIME_NOT_INITIALIZED;
    return menu_result;
}
uint32_t lv_timer_handler(void) { call('H'); return 1000; }
void rt_thread_mdelay(int ms) { if (ms==20) longjmp(loop,1); longjmp(loop,2); }
int rt_kprintf(const char *fmt,...)
{
    int n; size_t used=strlen(output); va_list args;
    va_start(args,fmt); n=vsnprintf(output+used,sizeof(output)-used,fmt,args); va_end(args); return n;
}
static int boot(void)
{
    int jumped; count=menus=runtime_calls=0; calls[0]=output[0]=0;
    status=MAZE_RUNTIME_NOT_INITIALIZED; menu_saw_status=false; received_tests=false;
    jumped=setjmp(loop);
    return jumped ? (jumped==1 ? 0 : -2) : maze_firmware_main();
}
int main(void)
{
    unsigned i;
    lab_result=-1; CHECK(boot()==-1 && strcmp(calls,"L")==0 && runtime_calls==0);
    lab_result=0; display_result=-1;
    CHECK(boot()==-1 && strcmp(calls,"LD")==0 && menus==0);
    printf("TC-408 main-platform-failure-gates PASS\n");
    display_result=0;
    CHECK(boot()==0 && strcmp(calls,"LDRMH")==0 && received_tests && menu_saw_status);
    CHECK(strstr(output,"[maze:boot] runtime=1 result=0") && strstr(output,"[maze:boot] menu ready"));
    printf("TC-408 main-runtime-before-menu PASS\n");
    for (i=1;i<=3;++i)
    {
        failed_test=i;
        CHECK(boot()==0 && !received_tests && menu_saw_status && runtime_calls==1);
        CHECK(status==MAZE_RUNTIME_SELFTEST_FAILED && strstr(output,"production entry must stay disabled"));
    }
    failed_test=0;
    printf("TC-408 main-three-selftest-failure-gates PASS\n");
    init_result=MAZE_INVALID;
    CHECK(boot()==0 && menu_saw_status && status==MAZE_RUNTIME_TIMER_FAILED);
    CHECK(strcmp(calls,"LDRMH")==0 && strstr(output,"runtime=5 result=-1"));
    printf("TC-408 main-runtime-failure-visible PASS\n");
    init_result=MAZE_OK; menu_result=MAZE_UI_ERR_DISPLAY;
    CHECK(boot()==-1 && strcmp(calls,"LDRMC")==0 && status==MAZE_RUNTIME_NOT_INITIALIZED);
    CHECK(strstr(output,"menu initialization failed") && !strstr(output,"menu ready"));
    printf("TC-408 main-menu-failure-stops-loop PASS\nsummary PASS failures=0\n");
    return 0;
}
