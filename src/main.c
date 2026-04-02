#include "utilities.h"
#include "greedyNN.h"
#include "2_opt.h"
#include "vns.h"
#include <time.h>

// TODO:
//  - [ ] Clean the code and make it more readable
//  - [ ] Use update_best_solution to update the best solution
//  - [ ] Add the Epsilon parameter to command line
//  - [ ] Implement Time Limit
//  - [ ] Implement gnu plot window
//  - [ ] Finish log refactoring
//  - [ ] Add Extra mileage (optional implementation)
/**
 * Main entry point for the TSP/VRP solver.
 * Initializes the problem instance, parses command-line arguments, reads the input map,
 * and attempts to load, validate, and plot an optimal solution if one is available.
 * @param argc The number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 upon successful program termination.
 */
int main(int argc, char **argv)
{

    if (VERBOSE >= 4)
    {
        for (int a = 0; a < argc; a++)
            printf("%s ", argv[a]);
        printf("\n");
    }

    // Initialize everything to 0/NULL safely
    instance inst = {0};

    parse_command_line(argc, argv, &inst);

    if (strcmp(inst.input_file, "NULL") != 0)
    {
        parse_instance(&inst);
    }
    else if (inst.nnodes > 0)
    {
        // Capture user preferences before they are overwritten
        double saved_timelimit = inst.timelimit;
        int saved_threads = inst.num_threads;
        int saved_seed = inst.randomseed;
        int saved_opt_applied = inst.opt_applied;
        char saved_opt_name[50];
        strcpy(saved_opt_name, inst.opt_name);

        inst = generate_random_instance(inst.nnodes, 1000.0, 1000.0, saved_seed);

        // Restore user preferences
        inst.timelimit = saved_timelimit;
        inst.num_threads = saved_threads;
        inst.opt_applied = saved_opt_applied;
        strcpy(inst.opt_name, saved_opt_name);
    }

    compute_distances(&inst);

    // --- OPTIMAL TOUR CHECK ---
    if (inst.nnodes > 0)
    {
        double optimal_cost = INF;
        int *optimal_tour = (int *)calloc(inst.nnodes, sizeof(int));

        // The parser handles the filename generation AND checks if it exists
        if (parse_tour(&inst, optimal_tour))
        {
            // Calculate cost
            solution sol;
            sol.tour = optimal_tour;
            sol.cost = calculate_cost(&inst, optimal_tour);

            // Validate silently
            if (is_tour_feasible(&sol, &inst))
            {
                optimal_cost = sol.cost;
            }
            else
            {
                free(optimal_tour);
                free_instance(&inst);
                print_error("The optimal tour sequence is INVALID!");
            }
        }
        free(optimal_tour);

        // --- RUN GREEDY NN ---
        if (VERBOSE >= 1)
            printf(COLOR_MAGENTA "\n> Running Greedy Nearest Neighbor from all starting nodes...\n" COLOR_RESET);
        solution best_nn_sol = {.cost = INF, .tour = NULL};
        best_nn_sol.tour = (int *)calloc(inst.nnodes, sizeof(int));

        solution current_sol;
        current_sol.tour = (int *)calloc(inst.nnodes, sizeof(int));

        solution working_sol;
        working_sol.tour = (int *)calloc(inst.nnodes, sizeof(int));

        clock_t start_time = clock();
        double best_cost_diff = 0.0;
        int pa, pb;
        int count_improvements = 0;

        for (int start_node = 0; start_node < inst.nnodes; start_node++)
        {
            current_sol.cost = 0.0; // Reset cost for each run

            clock_t current_time = clock();
            double time_elapsed = (double)(current_time - start_time) / CLOCKS_PER_SEC;
            if (time_elapsed > inst.timelimit)
            {
                printf(COLOR_YELLOW "\nTime limit reached (%.3f s). Stopping early.\n" COLOR_RESET, time_elapsed);

                break;
            }

            greedyNN(&inst, &current_sol, start_node);
            printf(COLOR_MAGENTA "[START]" COLOR_RESET " - Start node %-3d | Cost: " COLOR_ORANGE "%.3f\n" COLOR_RESET, start_node, current_sol.cost);
            // Apply 2-opt local search to every solution generated
            if (inst.opt_applied)
            {
                int local_swaps = 0;
                double pre_opt_cost = current_sol.cost;
                while (1)
                {
                    best_cost_diff = find_best_two_opt(&inst, &current_sol, &pa, &pb);
                    if (best_cost_diff >= -EPSILON) // No improvement found (considering floating-point tolerance)
                        break;

                    apply_two_opt(current_sol.tour, pa, pb);
                    if (VERBOSE >= 5)
                    {
                        printf("  - [REG-2OPT] Applied swap (%d, %d) | Improvement: " COLOR_GREEN "%.3f\n" COLOR_RESET, pa, pb, best_cost_diff);
                    }
                    local_swaps++;
                    current_sol.cost += best_cost_diff;
                }

                if (VERBOSE >= 3 && local_swaps > 0)
                {
                    printf(COLOR_CYAN "[2-OPT RESULT] 2-opt finished: %d swaps | Reduced by %.3f | New cost: %.3f\n" COLOR_RESET,
                           local_swaps, pre_opt_cost - current_sol.cost, current_sol.cost);
                }

                // --- VNS LOOP (Applied to this specific start node) ---
                int kicks_without_improvement = 0;
                int MAX_KICKS = 5; // Keep it small since we do this for EVERY start node!

                while (kicks_without_improvement < MAX_KICKS)
                {

                    // Time limit check inside the VNS loop
                    clock_t current_time = clock();
                    if ((double)(current_time - start_time) / CLOCKS_PER_SEC > inst.timelimit)
                        break;

                    // Start the VNS iteration from our current local best
                    memcpy(working_sol.tour, current_sol.tour, inst.nnodes * sizeof(int));
                    working_sol.cost = current_sol.cost;

                    // 1. Apply the 3-opt Kick
                    apply_random_3_opt_kick(&inst, &working_sol);
                    if (VERBOSE >= 5)
                        printf("  - [VNS-3OPT] Applied 3-opt kick | New cost: " COLOR_ORANGE "%.3f\n" COLOR_RESET, working_sol.cost);

                    // 2. Fall down into the new valley with 2-opt
                    while (1)
                    {
                        best_cost_diff = find_best_two_opt(&inst, &working_sol, &pa, &pb);
                        if (best_cost_diff >= -EPSILON)
                            break;

                        apply_two_opt(working_sol.tour, pa, pb);
                        working_sol.cost += best_cost_diff;
                        if (VERBOSE >= 5)
                            printf("  - [VNS-2OPT] Applied swap (%d, %d) | Improvement: " COLOR_GREEN "%.3f\n" COLOR_RESET, pa, pb, best_cost_diff);
                    }
                    working_sol.cost = calculate_cost(&inst, working_sol.tour);

                    // 3. Acceptance Criterion
                    if (working_sol.cost < current_sol.cost - EPSILON)
                    {
                        memcpy(current_sol.tour, working_sol.tour, inst.nnodes * sizeof(int));
                        current_sol.cost = working_sol.cost;
                        kicks_without_improvement = 0; // Reset frustration counter
                        if (VERBOSE >= 2)
                            printf(COLOR_MAGENTA "  - [VNS] Kick broke local optimum! New cost: %.3f\n" COLOR_RESET, current_sol.cost);
                    }
                    else
                    {
                        kicks_without_improvement++;
                    }
                }
            }

            // Recalculate cost cleanly from scratch to prevent floating-point drift
            current_sol.cost = calculate_cost(&inst, current_sol.tour);

            int is_new_best = 0;                            // Flag to check for improvement
            if (current_sol.cost < best_nn_sol.cost - 1e-5) // Use a small tolerance
            {

                is_new_best = 1;
                best_nn_sol.cost = current_sol.cost;
                memcpy(best_nn_sol.tour, current_sol.tour, inst.nnodes * sizeof(int));

                char plot_title[100];
                snprintf(plot_title, sizeof(plot_title), "greedy_nn_optimized_%d", count_improvements);
                count_improvements++;

                if (VERBOSE >= 5)
                    print_tour(best_nn_sol.tour, inst.nnodes);

                if (VERBOSE >= 3)
                    plot_tour(&inst, current_sol.tour, plot_title);
            }

            if (VERBOSE >= 2)
            {
                if (is_new_best)
                {
                    printf(COLOR_GREEN "[RESULT]" COLOR_RESET " - Cost: " COLOR_ORANGE "%.3f (New Best)\n\n" COLOR_RESET, current_sol.cost);
                }
                else
                {
                    printf(COLOR_GREEN "[RESULT]" COLOR_RESET " - Cost: %.3f\n\n", current_sol.cost);
                }
            }
        }
        free(current_sol.tour); // Free at the end
        free(working_sol.tour);

        if (VERBOSE >= 1)
        {
            printf("Best Greedy NN Cost: " COLOR_CYAN "%.3f\n" COLOR_RESET, best_nn_sol.cost);

            if (optimal_cost != INF)
            {
                printf("Optimal Tour Cost: " COLOR_GREEN "%.3f" COLOR_RESET "\n", optimal_cost);
                double gap = (best_nn_sol.cost - optimal_cost) / optimal_cost * 100.0;
                printf("Gap: %.3f%%\n", gap);
            }
        }
        if (VERBOSE >= 4)
            print_tour(best_nn_sol.tour, inst.nnodes);

        // Check and plot the result
        if (is_tour_feasible(&best_nn_sol, &inst))
        {
            if (VERBOSE >= 1)
            {
                plot_tour(&inst, best_nn_sol.tour, "final");
            }

            // Safely store the validated best solution in the global instance
            inst.best_solution.cost = best_nn_sol.cost;
            if (!inst.best_solution.tour)
                inst.best_solution.tour = (int *)malloc(inst.nnodes * sizeof(int));
            memcpy(inst.best_solution.tour, best_nn_sol.tour, inst.nnodes * sizeof(int));
        }
        else
        {
            free(best_nn_sol.tour);
            free_instance(&inst);
            print_error("The final Greedy NN solution is invalid.");
        }

        free(best_nn_sol.tour);
    }
    // --------------------------------

    free_instance(&inst);
    return 0;
}