/*
 * main.c — B独立菜单与A输入运行时共用UI线程；串口诊断验证真实输入。
 */
#include "lab.h"
#include "lvgl.h"
#include "littlevgl2rtt.h"
#include "maze_levels_selftest.h"
#include "maze_selftest.h"
#include "maze_controller_selftest.h"
#include "maze_runtime.h"
#include "maze_menu.h"

static void maze_adapter_test_report(const char *name, bool passed)
{
    rt_kprintf("[maze:adapter:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

static void maze_test_report(const char *name, bool passed)
{
    rt_kprintf("[maze:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

static void maze_level_test_report(const char *name, bool passed)
{
    rt_kprintf("[maze:level:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

/* 先建立测试和显示环境，再进入 LVGL 定时器处理循环。 */
int main(void)
{
    int maze_failures = maze_selftest_run(maze_test_report);
    bool maze_tests_ok = maze_failures == 0;
    rt_kprintf("[maze:selftest] summary %s failures=%d\n",
               maze_failures ? "FAIL" : "PASS", maze_failures);
    if (maze_failures) {
        rt_kprintf("[maze] model self-test failed; maze feature is not validated\n");
    }
    maze_failures = maze_levels_selftest_run(maze_level_test_report);
    maze_tests_ok = maze_tests_ok && maze_failures == 0;
    rt_kprintf("[maze:level:selftest] summary %s failures=%d\n",
               maze_failures ? "FAIL" : "PASS", maze_failures);
    if (maze_failures) {
        rt_kprintf("[maze] level self-test failed; level is not validated\n");
    }

    maze_failures = maze_controller_selftest_run(maze_adapter_test_report);
    maze_tests_ok = maze_tests_ok && maze_failures == 0;
    rt_kprintf("[maze:adapter:selftest] summary %s failures=%d\n",
               maze_failures ? "FAIL" : "PASS", maze_failures);

    /* lab_init 创建后台工作任务；littlevgl2rtt_init 将 LVGL 接到 SDK 的 lcd 驱动。 */
    if (lab_init() != RT_EOK || littlevgl2rtt_init("lcd") != RT_EOK) {
        rt_kprintf("Peripheral Lab: initialization failed\n");
        return -1;
    }
    /* Finish runtime initialization before any product entry can be opened.
     * B's production adapter must use init_status to gate its controls. */
    maze_result_t runtime_result = maze_runtime_init(maze_tests_ok);
    rt_kprintf("[maze:boot] runtime=%d result=%d\n",
               (int)maze_runtime_init_status(), (int)runtime_result);
    if (runtime_result != MAZE_OK)
        rt_kprintf("[maze] runtime unavailable; production entry must stay disabled\n");
    /* Internal lab_io is needed by A's sampler; the product entry is B's menu. */
    int ui_result = maze_menu_create();
    if (ui_result != MAZE_UI_SUCCESS) {
        rt_kprintf("[maze] menu initialization failed: %d\n", ui_result);
        if (runtime_result == MAZE_OK)
            rt_kprintf("[maze:boot] startup cleanup=%d\n",
                       (int)maze_runtime_cancel_startup());
        return -1;
    }
    rt_kprintf("[maze:boot] menu ready\n");
    rt_kprintf("[maze:app] AB merge 2026-11-06; standalone preview + motion diagnostics\n");
    while (1) {
        /* 处理界面定时器、输入与重绘，返回建议的下次调用间隔。 */
        uint32_t delay = lv_timer_handler();
        /* 有持续动画时也让出 CPU；空闲时不超过 20 ms，保持触摸响应。 */
        if (delay < 2) delay = 2;
        if (delay > 20) delay = 20;
        rt_thread_mdelay(delay);
    }
}
