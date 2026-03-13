
#include "utilities.h"

int VERBOSE = 2;
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
 * Prints an error message in red to stdout and terminates the program.
 * @param err The error message string.
 */
void print_error(const char *err)
{
    printf("\n\n" COLOR_RED " ERROR: %s \033[0m" COLOR_RESET "\n\n",
           err);
    fflush(NULL);
    exit(1);
}

/**
 * Parses the input file (TSPLIB format) to populate the instance structure.
 * @param inst Pointer to the instance structure.
 */
void parse_instance(instance *inst)
{
    if (strcmp(inst->input_file, "NULL") == 0)
    {
        print_error("No input file specified. Use -file <filename> or -random <n>.");
    }
    FILE *file = fopen(inst->input_file, "r");
    if (file == NULL)
        print_error(" input file not found!");

    inst->xcoord = NULL;
    inst->ycoord = NULL;

    inst->nnodes = -1;

    char line[200];
    char *par_name;
    char *token1;

    int active_section = 0;
    // =1 NODE_COORD_SECTION
    int do_print = (VERBOSE >= 4); // Level 4 for detailed parsing info

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (VERBOSE >= 5)
        {
            if (active_section == 0)
            {
                printf(COLOR_MAGENTA "[RAW] %s" COLOR_RESET, line);
                fflush(NULL); // makes sure the output is printed immediately
            }
        }
        if (strlen(line) <= 1)
            continue; // skip empty lines

        par_name = strtok(line, " :\t\r\n");
        if (!par_name)
            continue;

        if (strncmp(par_name, "NAME", 4) == 0)
        {
            active_section = 0;
            continue;
        }
        if (strncmp(par_name, "COMMENT", 7) == 0)
        {
            active_section = 0;
            token1 = strtok(NULL, "");
            if (VERBOSE >= 2) // Level 2 for basic info
                printf("Solving instance " COLOR_GREEN "%s" COLOR_RESET "\n", token1 ? token1 : " (no description)");
            continue;
        }
        if (strncmp(par_name, "TYPE", 4) == 0)
        {
            token1 = strtok(NULL, " :\t\r\n");
            if (token1 == NULL)
                print_error("Format error: TYPE field is missing value");

            active_section = 0;
            continue;
        }
        if (strncmp(par_name, "DIMENSION", 9) == 0)
        {
            if (inst->nnodes >= 0)
                print_error("Repeated DIMENSION section in input file");
            token1 = strtok(NULL, " :\t\r\n");
            if (token1 == NULL)
                print_error("Format error: DIMENSION field is missing value");
            inst->nnodes = atoi(token1);
            if (do_print)
                printf("Number of nodes: " COLOR_BLUE "%d" COLOR_RESET "\n", inst->nnodes);

            inst->ycoord = (double *)calloc(inst->nnodes, sizeof(double));
            inst->xcoord = (double *)calloc(inst->nnodes, sizeof(double));
            continue;
        }

        if (strncmp(par_name, "EDGE_WEIGHT_TYPE", 16) == 0)
        {
            token1 = strtok(NULL, " :\t\r\n");
            if (token1 == NULL)
                print_error("Format error: EDGE_WEIGHT_TYPE field is missing value");

            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "NODE_COORD_SECTION", 18) == 0)
        {
            if (inst->nnodes <= 0)
                print_error("DIMENSION section should appear before NODE_COORD_SECTION section");
            active_section = 1;
            if (do_print)
                printf(COLOR_MAGENTA "Reading Node Coordinates...\n" COLOR_RESET);
            continue;
        }

        if (strncmp(par_name, "EOF", 3) == 0)
        {
            active_section = 0;
            break;
        }

        if (active_section == 1) // NODE_COORD_SECTION
        {
            int i = atoi(par_name) - 1;
            if (i < 0 || i >= inst->nnodes)
                print_error("Node index out of bounds in NODE_COORD_SECTION");
            token1 = strtok(NULL, " :\t\r\n");
            if (token1 == NULL)
                print_error("Format error: x coordinate missing in NODE_COORD_SECTION");
            inst->xcoord[i] = atof(token1);
            token1 = strtok(NULL, " :\t\r\n");
            if (token1 == NULL)
                print_error("Format error: y coordinate missing in NODE_COORD_SECTION");
            inst->ycoord[i] = atof(token1);
            if (do_print)
                printf("Node " COLOR_YELLOW "[%4d] " COLOR_RESET "at coordinates ( " COLOR_CYAN "%6.0lf " COLOR_RESET ", " COLOR_CYAN "%6.0lf " COLOR_RESET ")\n", i + 1, inst->xcoord[i], inst->ycoord[i]);
            continue;
        }
    }

    if (VERBOSE >= 1) // Level 1 for success messages
        printf("Instance " COLOR_GREEN "%s" COLOR_RESET " parsed successfully.\n", inst->input_file);

    fclose(file);
}

void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * Pre-computes the distance matrix for the instance.
 * Stores the result in inst->dists.
 * @param inst Pointer to the instance structure.
 */
void compute_distances(instance *inst)
{
    // 1. Fix Memory Leak & Stale Cache
    // We must free the old matrix AND set the pointer to NULL.
    // Setting it to NULL ensures dist_sq() calculates values from scratch
    // instead of trying to read from a freed or stale array.
    if (inst->dists)
    {
        free(inst->dists);
        inst->dists = NULL;
    }

    int n = inst->nnodes;

    // 2. Memory Optimization (Smart Caching)
    // If the instance is too large, the matrix consumes too much RAM (risk of swapping).
    // Threshold: ~4000 nodes (4000^2 * 8 bytes ≈ 128 MB).
    // If exceeded, we skip the cache; dist() functions will auto-calculate on the fly.
    if (n > 4000)
    {
        if (VERBOSE >= 2)
            printf(COLOR_YELLOW "Optimization: Instance too large (%d nodes). Skipping distance matrix allocation.\n" COLOR_RESET, n);
        return;
    }

    double *d = (double *)malloc((size_t)n * n * sizeof(double));
    if (d == NULL)
        print_error("Memory allocation failed for distance matrix.");

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            d[i * n + j] = dist_sq(i, j, inst);
        }
    }
    inst->dists = d;
}

/**
 * Parses command-line arguments to configure solver settings (file, time limit, threads, etc.).
 * Displays the help menu if arguments are missing or help is requested.
 * @param inst Pointer to the instance structure.
 */
void parse_command_line(int argc, char **argv, instance *inst)
{
    if (VERBOSE >= 4) // Level 4 for debugging arguments
    {
        printf(" running %s with %d parameters \n", argv[0], argc - 1);
    }

    strcpy(inst->input_file, "NULL");

    inst->num_threads = 0; // Controls parallel processing (0 means automatic detection of available cores)
    inst->nnodes = 0;      // Number of nodes for random generation

    // Optimization Constraints
    inst->randomseed = -1; // Seed for random number generation, useful for reproducibility
    inst->timelimit = INF; // How long the solver is allowed to run before it is terminated

    int help = 0;
    if (argc < 1)
    {
        printf("Usage: %s -help for help\n", argv[0]);
        exit(1);
    }
    for (int i = 1; i < argc; i++)
    {
        // model type

        // input file
        if (strcmp(argv[i], "-file") == 0 || strcmp(argv[i], "-input") == 0 || strcmp(argv[i], "-f") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing filename after -file option");
            strcpy(inst->input_file, argv[++i]);
            continue;
        }

        // node number for random generation
        if (strcmp(argv[i], "-node_number") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -node_number");
            inst->nnodes = atoi(argv[++i]);
            continue;
        }

        // Number of threads
        if (strcmp(argv[i], "-threads") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -threads");
            inst->num_threads = atoi(argv[++i]);
            continue;
        }

        // Random seed
        if (strcmp(argv[i], "-seed") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -seed");
            if (atoi(argv[i + 1]) == -1)
                print_error(" random seed must be a non-negative integer");
            inst->randomseed = abs(atoi(argv[++i]));
            continue;
        }

        // 2-Opt Heuristic Optimization
        if (strcmp(argv[i], "-2opt") == 0)
        {
            strcpy(inst->opt_name, "2-opt");
            inst->opt_applied = 1;
            continue;
        }

        // total time limit
        if (strcmp(argv[i], "-time_limit") == 0 || strcmp(argv[i], "-time") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -time_limit");
            inst->timelimit = atof(argv[++i]);
            continue;
        }

        // Verbosity level
        if (strcmp(argv[i], "-verbose") == 0 || strcmp(argv[i], "-v") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -verbose");
            VERBOSE = atoi(argv[++i]);
            continue;
        }

        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            help = 1;
            continue;
        }
    }

    // --- VALIDATE ARGUMENTS ---
    int file_mode = (strcmp(inst->input_file, "NULL") != 0);
    int seed_set = (inst->randomseed != -1);
    int nodes_set = (inst->nnodes > 0);

    if (file_mode && (seed_set || nodes_set))
    {
        print_error("Cannot use file mode (-file) with random generation flags (-seed, -node_number).");
    }
    if (!file_mode && (seed_set != nodes_set))
    {
        print_error("-seed and -node_number must be used together.");
    }
    if (!file_mode && !seed_set && !help)
    {
        print_error("You must specify an input mode: either -file <path> or (-seed <n> and -node_number <n>).");
    }

    if (help)
    {
        printf(COLOR_ORANGE);
        printf("----------------------------------------------------------------------\n");
        printf(" TSP/VRP SOLVER - Help Menu\n");
        printf("----------------------------------------------------------------------\n");
        printf("Usage: %s -file <filename> [options]\n\n", argv[0]);

        printf("GENERAL LOGIC & FILES:\n");
        printf("  -file <path>        Path to the .tsp or .vrp input file.\n");
        printf("  -seed <n>           Enable random generation with a specific seed.\n");
        printf("  -node_number <n>    Number of nodes for the random instance (requires -seed).\n");

        printf("\nPERFORMANCE & RESOURCES:\n");
        printf("  -threads <n>        Number of CPU threads (0=auto).\n");
        printf("  -verbose <n>        Set verbosity level (0=silent, 1=info, 2=default, 3=detail, 4=debug, 5=trace).\n");

        printf("\nOPTIMIZATION CONSTRAINTS:\n");
        printf("  -seed <n>           Set the random seed for reproducible results\n");
        printf("  -time_limit <s>     Maximum execution time in seconds before termination\n");

        printf("\nOPTIMIZATION HEURISTICS:\n");
        printf("  -2opt               Apply 2-opt local search heuristic\n");
        printf("\n");
        printf("----------------------------------------------------------------------\n");
        printf(COLOR_RESET);
        exit(1);
    }

    if (VERBOSE >= 2) // Level 2 for configuration summary
    {
        printf("\n\n" COLOR_MAGENTA "Configuration Summary:" COLOR_RESET "\n");
        printf("----------------------------------------------------------------------\n");
        printf(" TSP SOLVER - Current Configuration\n");
        printf("----------------------------------------------------------------------\n");

        printf(COLOR_MAGENTA "GENERAL LOGIC & FILES:\n" COLOR_RESET);
        if (file_mode)
            printf("  -file <path>        : %s\n", inst->input_file);
        else
            printf("  -node_number <n>    : %d\n", inst->nnodes);

        printf(COLOR_MAGENTA "\nPERFORMANCE & RESOURCES:\n" COLOR_RESET);
        printf("  -threads <n>        : %d (0=auto)\n", inst->num_threads);
        printf("  -verbose <n>        : %d\n", VERBOSE);

        printf(COLOR_MAGENTA "\nOPTIMIZATION CONSTRAINTS:\n" COLOR_RESET);
        printf("  -seed <n>           : %d\n", inst->randomseed);
        if (inst->timelimit >= CPX_INFBOUND)
            printf("  -time_limit <s>     : Infinity\n");
        else
            printf("  -time_limit <s>     : %.3f sec\n", inst->timelimit);
        printf(COLOR_MAGENTA "\nOPTIMIZATION HEURISTICS:\n" COLOR_RESET);
        if (inst->opt_applied)
            printf("  -2opt               : Applied\n");
        else
            printf("  -2opt               : Not applied\n");

        printf("\n");

        printf("----------------------------------------------------------------------\n");
        printf(COLOR_RESET);
    }
}

/**
 * Prints the sequence of a given tour to stdout.
 * Outputs the node indices adjusted to a 1-based format for readability.
 * @param tour Array representing the order of visited nodes (0-based internally).
 * @param num_nodes The total number of nodes in the tour.
 */
void print_tour(int *tour, int num_nodes)
{
    printf(COLOR_BLUE "Tour sequence: " COLOR_RESET);
    for (int i = 0; i < num_nodes; i++)
    {
        // Shift back to 1-based indexing for output
        printf("%d ", tour[i] + 1);
    }
    printf("\n");
}

/**
 * Calculates the total cost of a tour using the standard Euclidean distance.
 * @param inst Pointer to the instance structure.
 * @param tour Array representing the sequence of visited nodes.
 * @return The total cost of the tour.
 */
double calculate_cost(instance *inst, int *tour)
{
    double cost = 0.0;
    for (int i = 0; i < inst->nnodes - 1; i++)
    {
        cost += dist(tour[i], tour[i + 1], inst);
    }
    cost += dist(tour[inst->nnodes - 1], tour[0], inst);
    return cost;
}

/**
 * Validates the structural integrity and cost consistency of a given tour.
 * Checks for a valid cycle, no duplicates, correct bounds, and matches the reported cost.
 * @param tour Array representing the sequence of visited nodes.
 * @param inst Pointer to the instance structure containing node coordinates.
 * @return 1 if the tour is valid, 0 otherwise.
 */
int validate_tour(solution *sol, instance *inst)
{
    int num_nodes = inst->nnodes;
    int *visited = (int *)calloc(num_nodes, sizeof(int));
    int *tour = sol->tour;

    // Check 1: Node bounds and duplicates
    for (int i = 0; i < num_nodes; i++)
    {
        int node = tour[i];
        if (node < 0 || node >= num_nodes)
        {
            printf(COLOR_RED "Validation Error: Node %d is out of bounds [0, %d]!\n" COLOR_RESET, node, num_nodes - 1);
            free(visited);
            return 0;
        }
        visited[node]++;
    }

    // Check 2: Each node visited exactly once
    for (int i = 0; i < num_nodes; i++)
    {
        if (visited[i] != 1)
        {
            printf(COLOR_RED "Validation Error: Node %d was visited %d times (must be exactly 1)!\n" COLOR_RESET, i, visited[i]);
            free(visited);
            return 0;
        }
    }
    free(visited); // Visited array is no longer needed

    // Check 3: Recalculate cost and compare with the solution's reported cost
    double total_cost = calculate_cost(inst, tour);

    if (fabs(total_cost - sol->cost) > 0.01)
    {
        printf(COLOR_YELLOW "\nValidation Warning: Calculated cost %.3f does not match reported cost %.3f\n" COLOR_RESET, total_cost, sol->cost);
        return 0;
    }

    if (VERBOSE >= 3)
    {
        printf(COLOR_GREEN "\n... Validation Passed: Tour is valid with cost %.3f.\n" COLOR_RESET, total_cost);
    }

    return 1;
}

/**
 * Generates a visual plot of the TSP tour using GNUplot.
 * It writes the node coordinates to a temporary GNUplot pipe and saves the graph as a PNG image.
 * @param inst Pointer to the instance structure containing the coordinate map.
 * @param tour Array representing the sequence of nodes to plot.
 */
void plot_tour(instance *inst, int *tour)
{

    FILE *gnuplotPipe = popen("gnuplot", "w");
    if (!gnuplotPipe)
    {
        printf(COLOR_RED "Error: Could not open GNUplot.\n" COLOR_RESET);
        return;
    }

    // Output a PNG file
    fprintf(gnuplotPipe, "set terminal pngcairo size 800,600\n");
    fprintf(gnuplotPipe, "set output 'tour_plot.png'\n");

    fprintf(gnuplotPipe, "set title 'TSP Tour'\n");
    fprintf(gnuplotPipe, "set key off\n");
    fprintf(gnuplotPipe, "plot '-' with linespoints pt 7 lc rgb 'blue'\n");

    // Loop through the tour array to feed the coordinates in order
    for (int i = 0; i < inst->nnodes; i++)
    {
        int node = tour[i];
        fprintf(gnuplotPipe, "%lf %lf\n", inst->xcoord[node], inst->ycoord[node]);
    }

    // Print the starting node again to close the plotted cycle
    int start_node = tour[0];
    fprintf(gnuplotPipe, "%lf %lf\n", inst->xcoord[start_node], inst->ycoord[start_node]);

    // Signal end of data to GNUplot
    fprintf(gnuplotPipe, "e\n");

    fflush(gnuplotPipe);
    pclose(gnuplotPipe);
}

/**
 * Parses a given `.opt.tour` solution file dynamically generated from the input file path.
 * Loads the optimal sequence into the provided array for further evaluation and checking.
 * @param inst Pointer to the instance structure containing the problem dimension and initial filename.
 * @param tour Pre-allocated array to be filled with the valid 0-based node sequence.
 * @return 1 on successful read and parse, 0 if the corresponding optimal tour file does not exist.
 */
int parse_tour(instance *inst, int *tour)
{
    // Dynamically generate the optimal filename from the instance input file
    char opt_filename[1000];
    strcpy(opt_filename, inst->input_file);
    char *ext = strrchr(opt_filename, '.');
    if (ext != NULL)
    {
        strcpy(ext, ".opt.tour");
    }
    else
    {
        strcat(opt_filename, ".opt.tour"); // Fallback just in case
    }

    // Safely check if the file exists
    FILE *file = fopen(opt_filename, "r");
    if (!file)
    {
        if (VERBOSE >= 3)
            printf(COLOR_YELLOW "\n--- Notice: No optimal tour file found at %s ---\n" COLOR_RESET, opt_filename);
        return 0; // Return 0 (False) to tell main.c to skip the checks
    }

    if (VERBOSE >= 3)
        printf("\n" COLOR_MAGENTA "Loading optimal tour... " COLOR_RESET "\n");

    // Parse the file
    char line[256];
    int active_section = 0;
    int idx = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strncmp(line, "TOUR_SECTION", 12) == 0)
        {
            active_section = 1;
            continue;
        }
        if (active_section)
        {
            int node = atoi(line);
            if (node == -1)
                break;

            if (idx >= inst->nnodes)
            {
                print_error("Optimal tour file contains more nodes than the instance dimension!");
            }

            tour[idx++] = node - 1;
        }
    }
    fclose(file);

    return 1; // Return 1 (True) to indicate success
}

/**
 * Generates a random TSP instance with specified dimensions and seed.
 * @param nnodes Number of nodes.
 * @param x_max Maximum x-coordinate value.
 * @param y_max Maximum y-coordinate value.
 * @param seed Random seed for reproducibility.
 * @return A fully initialized instance structure with random coordinates. No filename is associated with it.
 */
instance generate_random_instance(int nnodes, double x_max, double y_max, int seed)
{
    instance inst;
    memset(&inst, 0, sizeof(instance));

    inst.nnodes = nnodes;
    inst.randomseed = seed;
    inst.timelimit = CPX_INFBOUND; // Default to infinite time
    inst.num_threads = 0;          // Default to auto

    inst.xcoord = (double *)calloc(nnodes, sizeof(double));
    inst.ycoord = (double *)calloc(nnodes, sizeof(double));

    srand(seed);

    for (int i = 0; i < nnodes; i++)
    {
        inst.xcoord[i] = ((double)rand() / RAND_MAX) * x_max;
        inst.ycoord[i] = ((double)rand() / RAND_MAX) * y_max;
    }

    return inst;
}