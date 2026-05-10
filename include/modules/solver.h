#ifndef SOLVER_H_
#define SOLVER_H_
#include <time.h>
#include "core/structures.h"
#include "core/utilities.h"
#include "construction/NearestN.h"
#include "heuristics/2_opt.h"
#include "metaheuristics/vns.h"
#include "metaheuristics/genetic.h"

/**
 * Iterates through every possible starting node, generates a Greedy NN tour,
 * and refines it using 2-opt and VNS.
 */
void solve_tsp(instance *inst, double start_time);

void apply_2opt_local_search(instance *inst, solution *sol, double start_time);

/**
 * Applies 2-opt local search and VNS kicks to a given solution
 * until a local optimum is reached or time runs out.
 */
void refine_solution(instance *inst, solution *sol, double start_time);
void fill_solution_pool(instance *inst, double start_time)

#endif // SOLVER_H_