#ifndef TABU_H_
#define TABU_H_
#include "core/structures.h"

/**
 * @brief Runs Tabu Search 2-opt local search starting from the given solution.
 *
 * Repeatedly applies the best ALLOWED 2-opt move (best non-tabu move, or any
 * tabu move that satisfies the aspiration criterion of beating the best
 * solution found so far) until `tabu_max_iters_no_improve` consecutive
 * non-improving iterations occur, or the time limit is reached.
 *
 * Unlike VNS/2-opt, Tabu Search accepts non-improving (even worsening) moves
 * every iteration, using the tabu list to avoid immediately undoing recent
 * moves and cycling back to solutions already visited.
 *
 * @param inst Pointer to the problem instance.
 * @param sol Pointer to the solution to be refined in place. On return holds
 *            the best solution actually found, not necessarily the final
 *            "current" tour (which may have wandered uphill).
 * @param start_time Wall-clock start time of the enclosing solve, for
 *                    timelimit_check.
 * @param seed Unused by the core algorithm today (move selection is
 *             deterministic) but kept for signature parity with
 *             refine_solution, and to support future randomized
 *             tie-breaking / restarts.
 */
void run_tabu_search(instance *inst, solution *sol, double start_time, unsigned int *seed);

/**
 * @brief 2-opt neighbor scan restricted by tabu-search rules.
 *
 * Scans every non-adjacent edge pair like find_best_two_opt, but instead of
 * only considering improving moves, it tracks the best ALLOWED move overall
 * (improving or not) so the search can climb out of local optima. A move is
 * "allowed" if neither newly-created edge is currently tabu, OR if applying
 * it would beat best_known_cost (aspiration criterion).
 *
 * @param inst Pointer to the problem instance.
 * @param sol Pointer to the current solution (not modified).
 * @param pa Output: first cut-point index of the best allowed move (-1 if none found).
 * @param pb Output: second cut-point index of the best allowed move (-1 if none found).
 * @param tabu_until Flattened N x N array; tabu_until[a*n+b] is the iteration
 *                    number up to which edge (a,b) may not be re-added.
 * @param iteration Current tabu-search iteration counter.
 * @param best_known_cost Cost of the best solution found so far (for aspiration).
 * @return The cost delta of the best allowed move (may be >= 0).
 */
double find_best_two_opt_tabu(instance *inst, solution *sol, int *pa, int *pb,
                              long *tabu_until, long iteration, double best_known_cost);

#endif // TABU_H_