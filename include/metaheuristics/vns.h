#ifndef VNS_H_
#define VNS_H_
#include "core/structures.h"

void apply_3_opt_kick(instance *inst, solution *sol, unsigned int *seed);
void apply_3_opt_kick_reversing(instance *inst, solution *sol, unsigned int *seed);
void apply_random_3_opt_kick(instance *inst, solution *sol, unsigned int *seed);

#endif // VNS_H_