#ifndef ROLL3_SELFTEST_H
#define ROLL3_SELFTEST_H
#include <stdbool.h>
typedef void (*r3_test_report_t)(const char *name, bool passed);
/* Isolated 3D models, synthetic input. No board/IMU/display access. */
int r3_selftest_run(r3_test_report_t report);
int roll3_session_selftest(r3_test_report_t report);
#endif
