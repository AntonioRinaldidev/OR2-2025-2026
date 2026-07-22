#include "metaheuristics/tabu.h"
#include "core/utilities.h"
#include "heuristics/2_opt.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

double find_best_two_opt_tabu(instance *inst, solution *sol, int *pa, int *pb,
                              long *tabu_until, long iteration, double best_known_cost)
{
    int n = inst->nnodes;
    double cost_diff, best_cost_diff = INF;
    *pa = -1;
    *pb = -1;

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 2; j < n; j++)
        {
            int next_i = (i + 1) % n;
            int next_j = (j + 1) % n;

            cost_diff = dist(sol->tour[i], sol->tour[j], inst) + dist(sol->tour[next_i], sol->tour[next_j], inst) - dist(sol->tour[i], sol->tour[next_i], inst) - dist(sol->tour[j], sol->tour[next_j], inst);

            // The two NEW edges this move would create:
            int new_edge1_a = sol->tour[i], new_edge1_b = sol->tour[j];
            int new_edge2_a = sol->tour[next_i], new_edge2_b = sol->tour[next_j];

            bool is_tabu = (tabu_until[new_edge1_a * n + new_edge1_b] > iteration) ||
                           (tabu_until[new_edge2_a * n + new_edge2_b] > iteration);

            if (is_tabu)
            {
                // Aspiration: allow it anyway if it would beat the best solution ever found.
                double candidate_cost = sol->cost + cost_diff;
                if (candidate_cost >= best_known_cost - EPSILON)
                    continue; // still forbidden, skip this candidate
            }

            // NOTE: unlike find_best_two_opt, we do NOT require cost_diff < 0 here.
            // Tabu Search must move to the best ALLOWED neighbor every iteration,
            // even if every allowed neighbor is worse than the current tour --
            // that's what lets it climb out of a local optimum.
            if (cost_diff < best_cost_diff)
            {
                best_cost_diff = cost_diff;
                *pa = i;
                *pb = j;
            }
        }
    }
    return best_cost_diff;
}

void run_tabu_search(instance *inst, solution *sol, double start_time, unsigned int *seed)
{
    (void)seed; // not used yet -- kept for signature parity / future extensions

    if (!inst->use_tabu)
        return;

    int n = inst->nnodes;

    // Default tenure: n/10, floored at 5, if the user didn't set one explicitly.
    int tenure = (inst->tabu_tenure > 0) ? inst->tabu_tenure : ((n / 10 > 5) ? n / 10 : 5);
    int max_iters_no_improve = (inst->tabu_max_iters_no_improve > 0) ? inst->tabu_max_iters_no_improve : 1000;

    // O(n^2) tabu matrix -- same memory profile as inst->dists. For very large
    // instances (same philosophy as compute_distances' 4000-node cutoff) this
    // should eventually move to a sparser recency list instead of a full matrix.
    long *tabu_until = (long *)calloc((size_t)n * n, sizeof(long));
    if (tabu_until == NULL)
        print_error("Memory allocation failed for tabu_until matrix.");

    solution best_sol;
    best_sol.tour = (int *)malloc(n * sizeof(int));
    memcpy(best_sol.tour, sol->tour, n * sizeof(int));
    best_sol.cost = sol->cost;

    long iteration = 0;
    int iters_no_improve = 0;

    if (VERBOSE >= 2)
        printf(COLOR_CYAN "[TABU]" COLOR_RESET " Starting from cost " COLOR_ORANGE "%.2f" COLOR_RESET " (tenure=%d, max_stall=%d)\n",
               sol->cost, tenure, max_iters_no_improve);

    while (iters_no_improve < max_iters_no_improve)
    {
        iteration++;
        if (iteration % 50 == 0 && timelimit_check(inst, start_time))
            break;

        int pa, pb;
        double diff = find_best_two_opt_tabu(inst, sol, &pa, &pb, tabu_until, iteration, best_sol.cost);

        if (pa == -1)
        {
            // Every move forbidden (can happen on tiny instances / large tenure) -- bail out.
            if (VERBOSE >= 2)
                printf(COLOR_YELLOW "[TABU]" COLOR_RESET " No allowed move found, stopping early.\n");
            break;
        }

        // Capture the edges about to be REMOVED before mutating the tour,
        // so we can mark them tabu (forbid re-adding them) afterward.
        int next_pa = (pa + 1) % n;
        int next_pb = (pb + 1) % n;
        int removed_edge1_a = sol->tour[pa], removed_edge1_b = sol->tour[next_pa];
        int removed_edge2_a = sol->tour[pb], removed_edge2_b = sol->tour[next_pb];

        apply_two_opt(sol->tour, pa, pb);
        sol->cost += diff;

        long forbidden_until = iteration + tenure;
        tabu_until[removed_edge1_a * n + removed_edge1_b] = forbidden_until;
        tabu_until[removed_edge1_b * n + removed_edge1_a] = forbidden_until;
        tabu_until[removed_edge2_a * n + removed_edge2_b] = forbidden_until;
        tabu_until[removed_edge2_b * n + removed_edge2_a] = forbidden_until;

        if (sol->cost < best_sol.cost - EPSILON)
        {
            memcpy(best_sol.tour, sol->tour, n * sizeof(int));
            best_sol.cost = sol->cost;
            iters_no_improve = 0;

            if (VERBOSE >= 3)
                printf(COLOR_GREEN "[TABU]" COLOR_RESET " iter %ld | New best: %.2f\n", iteration, best_sol.cost);
        }
        else
        {
            iters_no_improve++;
        }
    }

    // Tabu Search wanders uphill by design -- always return the best solution
    // actually found, not whatever the "current" tour happens to be on exit.
    memcpy(sol->tour, best_sol.tour, n * sizeof(int));
    sol->cost = best_sol.cost;

    if (VERBOSE >= 1)
        printf(COLOR_YELLOW "[TABU-END]" COLOR_RESET " Iterations: %ld | Final cost: %.2f\n", iteration, best_sol.cost);

    free(best_sol.tour);
    free(tabu_until);
}