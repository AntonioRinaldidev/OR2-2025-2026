#include "core/utilities.h"
#include "modules/solver.h"
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

    clock_t start_time = clock();
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

    if (inst.nnodes > 0)
    {

        // --- SOLVER ---
        inst.best_solution.tour = NULL;
        inst.best_solution.cost = INF;

        solve_tsp(&inst, start_time);

        // --- OPTIMAL TOUR CHECK ---
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

        if (VERBOSE >= 1)
        {
            printf("\n" COLOR_GREEN "[FINAL RESULT]" COLOR_RESET "\n");
            printf("Best Cost Found: " COLOR_ORANGE "%.3f" COLOR_RESET "\n", inst.best_solution.cost);

            if (optimal_cost != INF)
            {
                double gap = (inst.best_solution.cost - optimal_cost) / optimal_cost * 100.0;
                printf("Gap to Optimal: %.3f%%\n", gap);
            }
        }

        // 4. Validate and Plot the real winner
        if (is_tour_feasible(&(inst.best_solution), &inst))
        {
            plot_tour(&inst, inst.best_solution.tour, "final");
        }
        else
        {
            printf(COLOR_RED "ERROR: The final solution is invalid!" COLOR_RESET "\n");
        }
    }
    // --------------------------------

    free_instance(&inst);
    return 0;
}
