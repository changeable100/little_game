#ifndef MAZE_LEVELS_H
#define MAZE_LEVELS_H

#include "maze_contract.h"

const maze_level_t *maze_levels_default(void);
bool maze_levels_reachable(const maze_level_t *level);

#endif
