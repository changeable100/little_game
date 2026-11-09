#include "roll3_selftest.h"
#include <stdio.h>
static void report(const char *name,bool passed) {printf("%s %s\n",name,passed?"PASS":"FAIL");}
int main(void) {
    int failures=r3_selftest_run(report);
    failures += roll3_session_selftest(report);
    printf("roll3 summary %s failures=%d\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
