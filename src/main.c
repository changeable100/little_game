/*
 * main.c — 3D游戏菜单、后台排行榜与体感运行时。
 */
#include "lab.h"
#include "lvgl.h"
#include "littlevgl2rtt.h"
#include "roll3_selftest.h"
#include "roll3_renderer.h"
#include "roll3_runtime.h"
#include "roll3_page.h"
#include "roll3_menu.h"
#include "roll3_records.h"

static void roll3_test_report(const char *name, bool passed)
{
    rt_kprintf("[roll3:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

/* 先建立测试和显示环境，再进入 LVGL 定时器处理循环。 */
int main(void)
{
    int roll3_failures = r3_selftest_run(roll3_test_report);
    roll3_failures += roll3_renderer_selftest(roll3_test_report);
    roll3_failures += roll3_session_selftest(roll3_test_report);
    rt_kprintf("[roll3:selftest] summary %s failures=%d\n",
               roll3_failures ? "FAIL" : "PASS", roll3_failures);
    if (roll3_failures)
        rt_kprintf("[roll3] physics self-test failed; 3D unavailable\n");

    /* lab_init 创建后台工作任务；littlevgl2rtt_init 将 LVGL 接到 SDK 的 lcd 驱动。 */
    if (lab_init() != RT_EOK || littlevgl2rtt_init("lcd") != RT_EOK) {
        rt_kprintf("Peripheral Lab: initialization failed\n");
        return -1;
    }
    int record_failures=r3_records_selftest(roll3_test_report);
    bool records_ready=record_failures==0&&roll3_records_init();
    rt_kprintf("[roll3:records] ready=%d selftest_failures=%d\n",(int)records_ready,record_failures);
    bool roll3_ready=roll3_runtime_init(roll3_failures==0);
    rt_kprintf("[roll3:boot] ready=%d\n",(int)roll3_ready);
    int ui_result=roll3_menu_create(roll3_ready);
    if(ui_result){rt_kprintf("[roll3] menu initialization failed: %d\n",ui_result);return -1;}
    rt_kprintf("[roll3:boot] game menu ready / levels=2 / local TOP 10\n");
    while (1) {
        /* 处理界面定时器、输入与重绘，返回建议的下次调用间隔。 */
        uint32_t delay = lv_timer_handler();
        /* 有持续动画时也让出 CPU；空闲时不超过 20 ms，保持触摸响应。 */
        if (delay < 2) delay = 2;
        if (delay > 20) delay = 20;
        rt_thread_mdelay(delay);

    }
}
