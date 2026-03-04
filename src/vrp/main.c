#include "vrp.h"

// ANSI Color Codes
#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"

#define COLOR_ORANGE "\033[38;5;208m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_RESET "\033[0m"

void debug(const char *err)
{
    printf("\nDEBUG: %s \n", err);
    fflush(NULL);
}

void free_instance(instance *inst)
{
    if (inst->demand) free(inst->demand);
    if (inst->xcoord) free(inst->xcoord);
    if (inst->ycoord) free(inst->ycoord);
    if (inst->load_min) free(inst->load_min);
    if (inst->load_max) free(inst->load_max);
}

int main(int argc, char **argv)
{

    if (VERBOSE >= 2)
    {
        for (int a = 0; a < argc; a++)
            printf("%s ", argv[a]);
        printf("\n");
    }

    // Initialize everything to 0/NULL safely
    instance inst = {0};

    parse_command_line(argc, argv, &inst);
    parse_instance(&inst);

    // --- TEST ---
    if (inst.nnodes > 0) {
        int *optimal_tour = (int *)malloc(inst.nnodes * sizeof(int));
        
        // The parser handles the filename generation AND checks if it exists
        if (parse_optimal_solution(&inst, optimal_tour)) {
            
            print_tour(optimal_tour, inst.nnodes);

            // Check it and calculate the total weight
            if (check_tour(optimal_tour, &inst)) {
                // ONLY plot if the tour is mathematically valid
                printf(COLOR_BLUE "\n> Plotting the optimal tour... Check 'tour_plot.png'!\n" COLOR_RESET);
                plot_tour(&inst, optimal_tour);
            } else {
                printf(COLOR_RED "> Aborting plot: The sequence is INVALID!\n" COLOR_RESET);
            }
        }
        
        free(optimal_tour);
    }
    // --------------------------------

    free_instance(&inst);
    return 0;
}