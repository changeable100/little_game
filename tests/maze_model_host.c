/* Runs the exact board selftests on a host as supplemental logic validation. */
#include "maze_selftest.h"
#include <stdio.h>

static void report(const char *name, bool passed)
{
    printf("[maze:selftest] %s %s\n", name, passed ? "PASS" : "FAIL");
}

int main(void)
{
    int failures = maze_selftest_run(report);
    printf("[maze:selftest] summary %s failures=%d\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
