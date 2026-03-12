#include "utilities.h"
#include "greedyNN.h"
#include <time.h>

/**
 * Frees all dynamically allocated memory within the instance structure.
 * Prevents memory leaks by safely deallocating arrays for coordinates, demands, and loads.
 * @param inst Pointer to the instance structure to be cleaned up.
 */
void free_instance(instance *inst)
{
    if (inst->xcoord)
        free(inst->xcoord);
    if (inst->ycoord)
        free(inst->ycoord);
    if (inst->best_solution.tour)
        free(inst->best_solution.tour);
    if (inst->dists)
        free(inst->dists);
}

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

        inst = generate_random_instance(inst.nnodes, 1000.0, 1000.0, saved_seed);

        // Restore user preferences
        inst.timelimit = saved_timelimit;
        inst.num_threads = saved_threads;
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
            if (validate_tour(&sol, &inst))
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
        clock_t start_time = clock();
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

            int is_new_best = 0; // Flag to check for improvement
            if (current_sol.cost < best_nn_sol.cost)
            {
                is_new_best = 1;
                best_nn_sol.cost = current_sol.cost;
                memcpy(best_nn_sol.tour, current_sol.tour, inst.nnodes * sizeof(int));
            }

            if (VERBOSE >= 3)
            {
                if (is_new_best)
                {
                    printf("  - Start node %-3d | Cost: " COLOR_GREEN "%.3f (New Best)\n" COLOR_RESET, start_node + 1, current_sol.cost);
                }
                else
                {
                    printf("  - Start node %-3d | Cost: %.3f\n", start_node + 1, current_sol.cost);
                }
            }
        }
        free(current_sol.tour); // Free at the end

        if (VERBOSE >= 1)
        {
            printf("Best Greedy NN Cost: " COLOR_CYAN "%.3f\n" COLOR_RESET, best_nn_sol.cost);
            print_tour(best_nn_sol.tour, inst.nnodes);

            if (optimal_cost != INF)
            {
                printf("Optimal Tour Cost: " COLOR_GREEN "%.3f" COLOR_RESET "\n", optimal_cost);
                double gap = (best_nn_sol.cost - optimal_cost) / optimal_cost * 100.0;
                printf("Gap: %.3f%%\n", gap);
            }
        }

        // Check and plot the result
        if (validate_tour(&best_nn_sol, &inst))
        {
            if (VERBOSE >= 1)
            {
                plot_tour(&inst, best_nn_sol.tour);
            }
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