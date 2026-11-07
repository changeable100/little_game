#ifndef MAZE_UI_RUNTIME_H
#define MAZE_UI_RUNTIME_H
#include "maze_ui.h"
/* All calls are main/LVGL-thread only. Never initialize/cancel runtime here. */
int maze_ui_runtime_open(void);
/* Temporary release: detach own renderer before destroying UI; no BACK. */
maze_result_t maze_ui_runtime_release(void);
maze_result_t maze_ui_runtime_last_result(void);
const char *maze_ui_runtime_startup_text(void);
#endif
