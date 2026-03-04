#include "vrp.h"

void print_error(const char *err);
void parse_instance(instance *inst);
void parse_command_line(int argc, char **argv, instance *inst);

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

    // --- TEST FOR UTILITIES ---
    if (inst.nnodes > 0) {
        printf("\n--- Testing TSP Utilities ---\n");
        
        // Create a dummy tour (0, 1, 2, ..., nnodes-1)
        int *dummy_tour = (int *)malloc(inst.nnodes * sizeof(int));
        for (int i = 0; i < inst.nnodes; i++) {
            dummy_tour[i] = i; 
        }

        // 1. Print the tour
        print_tour(dummy_tour, inst.nnodes);

        // 2. Check if it's a valid cycle
        if (check_tour(dummy_tour, inst.nnodes)) {
            printf("\n> The generated sequence is a VALID tour!\n");
        } else {
            printf("\n> The generated sequence is INVALID!\n");
        }

        // 3. Plot the tour using GNUplot
        printf("> Plotting the tour... Check your project folder for 'tour_plot.png'!\n");
        plot_tour(&inst, dummy_tour);

        free(dummy_tour);
    }
    // --------------------------------

    free_instance(&inst);
    return 0;
}