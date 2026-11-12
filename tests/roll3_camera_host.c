#include "roll3_renderer.h"
#include <stdio.h>
static void report(const char *name,bool passed){printf("%s %s\n",name,passed?"PASS":"FAIL");}
int main(void){return roll3_renderer_selftest(report)?1:0;}
