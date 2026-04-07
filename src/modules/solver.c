#include "solver.h"

void apply_2opt_local_search(instance *inst, solution *sol, clock_t start_time)
{
    int pa, pb;
    double best_cost_diff;
    while (1)
    {
        if (timelimit_check(inst, start_time))
            break;
        best_cost_diff = find_best_two_opt(inst, sol, &pa, &pb);
        if (best_cost_diff >= -EPSILON)
            break;
        apply_two_opt(sol->tour, pa, pb);
        sol->cost += best_cost_diff;
    }
}
/**
 * @brief Refines a given solution using both 2-opt local search and variable neighbourhood search (VNS).
 *
 * First, the 2-opt local search is applied to the given solution to find a local optimum.
 * Then, the VNS is applied in a loop. Each iteration of the VNS loop consists of:
 * - A random 3-opt kick is applied to the current best solution.
 * - The 2-opt local search is applied to the kicked solution.
 * - If the new solution is better than the current best, it is accepted and the frustration counter is reset.
 * - If the new solution is not better, the frustration counter is incremented.
 * - The loop is stopped when a maximum number of kicks without improvement is reached, or when the time limit is reached.
 *
 * @param inst Pointer to the problem instance.
 * @param sol Pointer to the solution to be refined.
 * @param start_time The starting time of the refinement process (in seconds).
 */
void refine_solution(instance *inst, solution *sol, clock_t start_time)
{
    if (!inst->opt_applied)
        return;

    if (VERBOSE >= 4)
    {
        printf(COLOR_MAGENTA "[REFINE]" COLOR_RESET " Starting refinement for cost " COLOR_ORANGE "%.2f" COLOR_RESET "\n", sol->cost);
    }
    double pre_opt_cost = sol->cost;

    // --- 2OPT IMPROVEMENT ---
    apply_2opt_local_search(inst, sol, start_time);

    if (VERBOSE >= 3 && (pre_opt_cost - sol->cost > EPSILON))
    {
        printf(COLOR_GREEN "[SUCCESS]" COLOR_RESET " 2-opt improved cost: " COLOR_ORANGE "%.2f" COLOR_RESET " -> " COLOR_ORANGE "%.2f" COLOR_RESET "\n",
               pre_opt_cost, sol->cost);
    }

    // --- VNS LOOP  ---
    solution working_sol;
    working_sol.tour = (int *)calloc(inst->nnodes, sizeof(int));
    int kicks_without_improvement = 0;
    int MAX_KICKS = 5;
    int iter = 0;

    while (kicks_without_improvement < MAX_KICKS)
    {
        iter++;
        if (iter % 100 == 0)
        {
            if (timelimit_check(inst, start_time))
                break;
        }

        // Time limit check

        // Start the VNS iteration from our current local best
        memcpy(working_sol.tour, sol->tour, inst->nnodes * sizeof(int));
        working_sol.cost = sol->cost;

        // Kick and descend
        apply_random_3_opt_kick(inst, &working_sol);
        apply_2opt_local_search(inst, &working_sol, start_time);
        working_sol.cost = calculate_cost(inst, working_sol.tour);

        // Acceptance
        if (working_sol.cost < sol->cost - EPSILON)
        {
            if (VERBOSE >= 2)
                printf(COLOR_GREEN "[SUCCESS]" COLOR_RESET " VNS improvement! " COLOR_ORANGE "%.2f" COLOR_RESET " -> " COLOR_ORANGE "%.2f" COLOR_RESET "\n",
                       sol->cost, working_sol.cost);

            memcpy(sol->tour, working_sol.tour, inst->nnodes * sizeof(int));

            sol->cost = working_sol.cost;
            kicks_without_improvement = 0; // Reset frustration counter
        }
        else
        {
            kicks_without_improvement++;
        }
    }

    free(working_sol.tour);
}

/**
 * @brief Main entry point for the TSP solver.
 *
 * Solves the TSP using a multi-start heuristic approach. It iterates through every possible starting node, generates a Greedy NN tour, and refines it using 2-opt local search and VNS.
 *
 * @param inst A pointer to the instance structure.
 * @param start_time The starting time of the solver.
 */
void solve_tsp(instance *inst, clock_t start_time)
{
    solution current_sol;
    current_sol.tour = (int *)calloc(inst->nnodes, sizeof(int));
    current_sol.cost = INF;

    for (int start_node = 0; start_node < inst->nnodes; start_node++)
    {
        if (timelimit_check(inst, start_time))
            break;

        current_sol.cost = 0.0; // Reset Cost

        greedyNN(inst, &current_sol, start_node);

        if (VERBOSE >= 3)
        {
            printf(COLOR_CYAN "\n[SOLVER]" COLOR_RESET " Start node %-3d | Greedy Cost: " COLOR_ORANGE "%.2f" COLOR_RESET "\n",
                   start_node, current_sol.cost);
        }

        refine_solution(inst, &current_sol, start_time);

        if (current_sol.cost < inst->best_solution.cost - EPSILON)
        {
            inst->best_solution.cost = current_sol.cost;
            if (inst->best_solution.tour == NULL)
            {
                inst->best_solution.tour = (int *)malloc(inst->nnodes * sizeof(int));
            }
            memcpy(inst->best_solution.tour, current_sol.tour, inst->nnodes * sizeof(int));

            if (VERBOSE >= 1)
            {
                printf(COLOR_GREEN "[NEW BEST]" COLOR_RESET " Node %d | Global Cost: " COLOR_GREEN "%.2f" COLOR_RESET "\n",
                       start_node, inst->best_solution.cost);
            }
        }
    }

    free(current_sol.tour);
}
