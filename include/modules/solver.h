#ifndef SOLVER_H_
#define SOLVER_H_
#include <time.h>
#include "core/structures.h"
#include "core/utilities.h"
#include "construction/greedyNN.h"
#include "heuristics/2_opt.h"
#include "metaheuristics/vns.h"

/**
 * Iterates through every possible starting node, generates a Greedy NN tour,
 * and refines it using 2-opt and VNS.
 */
void solve_tsp(instance *inst, clock_t start_time);

void apply_2opt_local_search(instance *inst, solution *sol, clock_t start_time);

/**
 * Applies 2-opt local search and VNS kicks to a given solution
 * until a local optimum is reached or time runs out.
 */
void refine_solution(instance *inst, solution *sol, clock_t start_time);

#endif // SOLVER_H_