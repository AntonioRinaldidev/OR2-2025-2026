#ifndef MATHEURISTIC_H_
#define MATHEURISTIC_H_
#include "core/utilities.h"
#include "core/structures.h"
#include "core/solve_with_cplex.h"

void solve_matheuristic(instance *inst, double p);
void change_bound(instance *inst, int edge_idx, double lb, double ub);
void solve_local_branching(instance *inst);

#endif // MATHEURISTIC_H_