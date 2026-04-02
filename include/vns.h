#ifndef VNS_H_
#define VNS_H_
#include "structures.h"
#include "2_opt.h"

void apply_3_opt_kick(instance *inst, solution *sol);
void apply_3_opt_kick_reversing(instance *inst, solution *sol);
void apply_random_3_opt_kick(instance *inst, solution *sol);

#endif // VNS_H_
