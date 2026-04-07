#ifndef TWO_OPT_H
#define TWO_OPT_H

#include "core/utilities.h"

double find_best_two_opt(instance *inst, solution *sol, int *pa, int *pb);
void apply_two_opt(int *tour, int pa, int pb);

#endif // TWO_OPT_H