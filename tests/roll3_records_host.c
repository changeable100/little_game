#include "roll3_records.h"
#include <stdio.h>
static void report(const char *s,bool pass){printf("%s %s\n",s,pass?"PASS":"FAIL");}
int main(void){return r3_records_selftest(report);}
