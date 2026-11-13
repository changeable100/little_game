#ifndef ROLL3_LEVEL2_H
#define ROLL3_LEVEL2_H
#include "roll3_contract.h"
const r3_vec_t *r3_level2_route(unsigned *count);
int r3_level2_selftest(void (*report)(const char *,bool));
#endif
