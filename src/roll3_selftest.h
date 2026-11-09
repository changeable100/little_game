#ifndef ROLL3_SELFTEST_H
#define ROLL3_SELFTEST_H
#include "maze_selftest.h"
/* Isolated 3D models, synthetic input. No board/IMU/display access. */
int r3_selftest_run(maze_test_report_t report);
int roll3_session_selftest(maze_test_report_t report);
#endif
