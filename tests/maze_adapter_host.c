#include "maze_controller_selftest.h"
#include "maze_levels_selftest.h"
#include "maze_selftest.h"
#include <stdio.h>

static void report(const char *name, bool passed)
{
    printf("%s %s\n", name, passed ? "PASS" : "FAIL");
}

int main(void)
{
    int failures = maze_selftest_run(report);
    failures += maze_levels_selftest_run(report);
    failures += maze_controller_selftest_run(report);
    printf("summary %s failures=%d\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
