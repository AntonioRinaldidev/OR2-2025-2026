#include "core/utilities.h"
#include "modules/solver.h"
#include "core/solve_with_cplex.h"
#include <time.h>
#include "matheuristics/matheuristic.h"

// TODO:

//  - [ ] Add Extra mileage (optional implementation)
//  - [ ] Add Tabu Search (optional implementation)
// For the thesis  we will say the algorithm, describe it, maybe pseudocode, show the results, use performance profile to choose hyperparameters
// For the final thesis, remember to track the branch and cut  with relaxation and without relaxation in the performance profile

/**
 * Main entry point for the TSP solver.
 * Initializes the problem instance, parses command-line arguments, reads the input map,
 * and attempts to load, validate, and plot an optimal solution if one is available.
 * @param argc The number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 upon successful program termination.
 */
int main(int argc, char **argv)

{

    double start_time = get_wall_time();
    // Initialize everything to 0/NULL safely
    instance inst = {0};

    parse_command_line(argc, argv, &inst);

    if (strcmp(inst.input_file, "NULL") != 0)
    {
        parse_instance(&inst);
    }
    else if (inst.nnodes > 0)
    {

        generate_random_instance(&inst, 1000.0, 1000.0);
    }

    compute_distances(&inst);

    if (inst.nnodes > 0)
    {

        open_gnuplot(&inst);

        // --- SOLVER ---
        inst.best_solution.tour = NULL;
        inst.best_solution.cost = INF;

        inst.start_time = start_time;
        if (inst.use_cplex)
        {
            solve_with_cplex(&inst);
        }

        else if (inst.ga_applied)
        {

            // The GA handles its own initialization (seeding with VNS) and evolution
            run_genetic_algorithm(&inst);
        }
        else if (inst.use_local_branching)
        {
            solve_local_branching(&inst);
        }
        else if (inst.use_matheuristic)
        {
            fill_solution_pool(&inst, start_time);
            solve_matheuristic(&inst, 0.5);

            for (int i = 0; i < inst.pool_size; i++)
                free(inst.solution_pool[i].tour);
            free(inst.solution_pool);
        }
        else
        {
            // Standard multi-start Greedy + VNS approach
            solve_tsp(&inst, start_time);
        }

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
    printf("Press Enter to exit...");
    getchar();
    close_gnuplot(&inst);

    log_result(&inst);
    free_instance(&inst);

    return 0;
}
