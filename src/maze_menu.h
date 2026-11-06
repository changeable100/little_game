#ifndef MAZE_MENU_H
#define MAZE_MENU_H

#include "maze_ui.h"

/* Standalone game navigation. Same UI thread as maze_ui and LVGL.
 * Returns maze_ui_result codes. No Lab worker, radio, game clock or model
 * instance is started here. The current entry opens an explicit preview. */
int maze_menu_create(void);
/* Idempotent; first releases the menu-owned maze preview and help page. */
void maze_menu_destroy(void);

#endif
