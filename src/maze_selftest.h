#ifndef MAZE_SELFTEST_H
#define MAZE_SELFTEST_H

#include <stdbool.h>

typedef void (*maze_test_report_t)(const char *name, bool passed);

/* Returns number of failed scenarios. Uses isolated models, never real IMU. */
int maze_selftest_run(maze_test_report_t report);

#endif
