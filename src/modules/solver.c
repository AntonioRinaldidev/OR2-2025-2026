#include "modules/solver.h"
#include <unistd.h>

void apply_2opt_local_search(instance *inst, solution *sol, double start_time)
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
void refine_solution(instance *inst, solution *sol, double start_time, unsigned int *seed)
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
        apply_random_3_opt_kick(inst, &working_sol, seed);
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

void *solver_worker(void *args)
{
    solver_thread_args *arg = (solver_thread_args *)args;
    solution current_sol;
    current_sol.tour = (int *)calloc(arg->inst->nnodes, sizeof(int));
    current_sol.cost = INF;
    arg->best.tour = (int *)calloc(arg->inst->nnodes, sizeof(int));
    arg->best.cost = INF;

    for (int start_node = arg->start; start_node < arg->end; start_node++)
    {
        if (timelimit_check(arg->inst, arg->start_time))
            break;

        current_sol.cost = INF;

        switch (arg->inst->construction_type)
        {
        case CONSTRUCT_CARDINALITY_GRASP:
            cardinality_grasp(arg->inst, &current_sol, arg->inst->grasp_cardinality, start_node, &arg->rand_seed);
            break;
        case CONSTRUCT_VALUE_GRASP:
            value_based_grasp(arg->inst, arg->inst->grasp_alpha, &current_sol, start_node, &arg->rand_seed);
            break;
        default:
            greedyNN(arg->inst, &current_sol, start_node);
            break;
        }

        if (!is_tour_feasible(&current_sol, arg->inst))
            continue;

        refine_solution(arg->inst, &current_sol, arg->start_time, &arg->rand_seed);

        if (current_sol.cost < arg->best.cost - EPSILON)
        {
            memcpy(arg->best.tour, current_sol.tour, arg->inst->nnodes * sizeof(int));
            arg->best.cost = current_sol.cost;
        }
    }

    free(current_sol.tour);
    return NULL;
}

void solve_tsp(instance *inst, double start_time)
{
    int num_threads;
    if (inst->num_threads <= 0)
    {
        long detected = sysconf(_SC_NPROCESSORS_ONLN);
        num_threads = (detected > 0) ? (int)detected : 4;
    }
    else
    {
        num_threads = inst->num_threads;
    }

    // Cap threads to number of nodes
    if (num_threads > inst->nnodes)
        num_threads = inst->nnodes;
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    solver_thread_args *args = malloc(num_threads * sizeof(solver_thread_args));

    int nodes_per_thread = inst->nnodes / num_threads;
    int remainder = inst->nnodes % num_threads;
    int current_start = 0;

    // Spawn threads
    for (int t = 0; t < num_threads; t++)
    {
        args[t].inst = inst;
        args[t].start_time = start_time;
        args[t].start = current_start;
        args[t].end = current_start + nodes_per_thread + (t < remainder ? 1 : 0);
        args[t].rand_seed = (unsigned int)(inst->randomseed + t);
        args[t].best.tour = NULL;
        args[t].best.cost = INF;
        current_start = args[t].end;

        pthread_create(&threads[t], NULL, solver_worker, &args[t]);
    }

    // Join threads and collect best solution
    for (int t = 0; t < num_threads; t++)
    {
        pthread_join(threads[t], NULL);

        if (args[t].best.cost < inst->best_solution.cost - EPSILON)
        {
            update_best_solution(inst, &args[t].best);
            if (VERBOSE >= 1)
                printf(COLOR_GREEN "[NEW BEST]" COLOR_RESET " Thread %d | Global Cost: " COLOR_GREEN "%.2f" COLOR_RESET "\n",
                       t, inst->best_solution.cost);
        }

        if (args[t].best.tour)
            free(args[t].best.tour);
    }

    free(threads);
    free(args);
}
void fill_solution_pool(instance *inst, double start_time)
{
    unsigned int local_seed = (unsigned int)inst->randomseed;

    solution current_sol;
    current_sol.tour = (int *)calloc(inst->nnodes, sizeof(int));
    current_sol.cost = INF;
    inst->pool_size = 0;
    inst->max_pool_size = inst->nnodes * 0.05;
    if (inst->max_pool_size < 1)
        inst->max_pool_size = 1;
    if (inst->max_pool_size > 30)
        inst->max_pool_size = 30;

    inst->solution_pool = (solution *)calloc(inst->max_pool_size, sizeof(solution));

    for (int start_node = 0; start_node < inst->nnodes; start_node++)
    {
        if (timelimit_check(inst, start_time))
            break;

        current_sol.cost = INF; // Reset Cost

        switch (inst->construction_type)
        {
        case CONSTRUCT_CARDINALITY_GRASP:
            cardinality_grasp(inst, &current_sol, inst->grasp_cardinality, start_node, &local_seed);
            break;
        case CONSTRUCT_VALUE_GRASP:
            value_based_grasp(inst, inst->grasp_alpha, &current_sol, start_node, &local_seed);
            break;
        default:
            greedyNN(inst, &current_sol, start_node);
            break;
        }

        if (VERBOSE >= 3)
        {
            printf(COLOR_CYAN "\n[SOLVER]" COLOR_RESET " Start node %-3d | Greedy Cost: " COLOR_ORANGE "%.2f" COLOR_RESET "\n",
                   start_node, current_sol.cost);
        }

        if (!is_tour_feasible(&current_sol, inst))
        {
            if (VERBOSE >= 1)
            {
                printf(COLOR_RED "[FAILURE]" COLOR_RESET " Start node %-3d | Greedy Cost: " COLOR_ORANGE "%.2f" COLOR_RESET "\n",
                       start_node, current_sol.cost);
            }
            continue;
        }
        refine_solution(inst, &current_sol, start_time, &local_seed);
        if (current_sol.cost < inst->best_solution.cost - EPSILON)
        {
            update_best_solution(inst, &current_sol);

            if (VERBOSE >= 1)
            {
                printf(COLOR_GREEN "[NEW BEST]" COLOR_RESET " Node %d | Global Cost: " COLOR_GREEN "%.2f" COLOR_RESET "\n",
                       start_node, inst->best_solution.cost);
            }
        }
        if (current_sol.cost < inst->best_solution.cost * 1.10)
        {

            if (inst->pool_size < inst->max_pool_size)
            {
                inst->solution_pool[inst->pool_size].tour = (int *)calloc(inst->nnodes, sizeof(int));
                memcpy(inst->solution_pool[inst->pool_size].tour, current_sol.tour, inst->nnodes * sizeof(int));
                inst->solution_pool[inst->pool_size].cost = current_sol.cost;
                inst->pool_size++;
            }
            else
            {
                int worst_index = 0;
                double worst_cost = inst->solution_pool[0].cost;
                for (int i = 1; i < inst->pool_size; i++)
                {
                    if (inst->solution_pool[i].cost > worst_cost)
                    {
                        worst_cost = inst->solution_pool[i].cost;
                        worst_index = i;
                    }
                }
                if (current_sol.cost < worst_cost)
                {
                    free(inst->solution_pool[worst_index].tour);
                    inst->solution_pool[worst_index].tour = (int *)malloc(inst->nnodes * sizeof(int));
                    memcpy(inst->solution_pool[worst_index].tour, current_sol.tour, inst->nnodes * sizeof(int));
                    inst->solution_pool[worst_index].cost = current_sol.cost;
                }
            }
        }
    }

    free(current_sol.tour);
}
