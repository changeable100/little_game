/*
 * main.c — 独立迷宫入口：运行启动自检、初始化显示，在主线程驱动游戏UI。
 */
#include <rtthread.h>
#include "lvgl.h"
#include "littlevgl2rtt.h"
#include "maze_levels_selftest.h"
#include "maze_selftest.h"
#include "maze_menu.h"

static void maze_test_report(const char *name, bool passed)
{
    rt_kprintf("[maze:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

static void maze_level_test_report(const char *name, bool passed)
{
    rt_kprintf("[maze:level:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

/* 保留模型/地图自检，开机直接显示游戏菜单，不启动Lab测试任务。 */
int main(void)
{
    int maze_failures = maze_selftest_run(maze_test_report);
    rt_kprintf("[maze:selftest] summary %s failures=%d\n",
               maze_failures ? "FAIL" : "PASS", maze_failures);
    if (maze_failures) {
        rt_kprintf("[maze] model self-test failed; maze feature is not validated\n");
    }
    maze_failures = maze_levels_selftest_run(maze_level_test_report);
    rt_kprintf("[maze:level:selftest] summary %s failures=%d\n",
               maze_failures ? "FAIL" : "PASS", maze_failures);
    if (maze_failures) {
        rt_kprintf("[maze] level self-test failed; level is not validated\n");
    }

    if (littlevgl2rtt_init("lcd") != RT_EOK) {
        rt_kprintf("[maze] display initialization failed\n");
        return -1;
    }
    int ui_result = maze_menu_create();
    if (ui_result != MAZE_UI_SUCCESS) {
        rt_kprintf("[maze] menu initialization failed: %d\n", ui_result);
        return -1;
    }
    rt_kprintf("[maze:app] standalone game menu ready; preview controls only\n");
    while (1) {
        /* 处理界面定时器、输入与重绘，返回建议的下次调用间隔。 */
        uint32_t delay = lv_timer_handler();
        /* 有持续动画时也让出 CPU；空闲时不超过 20 ms，保持触摸响应。 */
        if (delay < 2) delay = 2;
        if (delay > 20) delay = 20;
        rt_thread_mdelay(delay);
    }
}
