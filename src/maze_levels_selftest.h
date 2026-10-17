#ifndef MAZE_LEVELS_SELFTEST_H
#define MAZE_LEVELS_SELFTEST_H

#include <stdbool.h>

typedef void (*maze_level_test_report_t)(const char *name, bool passed);

int maze_levels_selftest_run(maze_level_test_report_t report);

#endif
