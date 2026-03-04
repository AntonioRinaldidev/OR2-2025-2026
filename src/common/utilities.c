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
        print_error("No input file specified. Use -file <filename> to specify the input file.");
    }
    FILE *file = fopen(inst->input_file, "r");
    if (file == NULL)
        print_error(" input file not found!");

    inst->nnodes = -1;
    inst->depot = -1;
    inst->nveh = -1;

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

            inst->demand = (double *)calloc(inst->nnodes, sizeof(double));
            inst->xcoord = (double *)calloc(inst->nnodes, sizeof(double));
            inst->ycoord = (double *)calloc(inst->nnodes, sizeof(double));
            if (inst->demand == NULL || inst->xcoord == NULL || inst->ycoord == NULL)
                print_error("Memory allocation error for node data");
            active_section = 0;
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

    // General Logic & Files
    inst->model_type = 0;
    inst->old_benders = 0; // Old or new Benders decomposition (0 = new, 1 = old)
    strcpy(inst->input_file, "NULL");

    // Performance & Resources
    inst->available_memory = 12000; // Limits available memory for CPLEX execution in MB (e.g., 12000)
    inst->num_threads = 0;          // Controls parallel processing (0 means automatic detection of available cores)
    inst->max_nodes = -1;           //-1 means unlimited, otherwise sets a cap on the number of branching nodes in the final run

    // Optimization Constraints
    inst->randomseed = 0;           // Seed for random number generation, useful for reproducibility
    inst->timelimit = CPX_INFBOUND; // How long the solver is allowed to run before it is terminated
    inst->cutoff = CPX_INFBOUND;    // Sets an upper bound on the objective function value, guiding the solver to focus on solutions better than this threshold
    inst->integer_costs = 0;        // Indicates whether the costs in the model should be treated as integers (1) or not (0)

    int help = 0;
    if (argc < 1)
    {
        printf("Usage: %s -help for help\n", argv[0]);
        exit(1);
    }
    for (int i = 1; i < argc; i++)
    {
        // model type
        if (strcmp(argv[i], "-model_type") == 0 || strcmp(argv[i], "-model") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -model_type");
            inst->model_type = atoi(argv[++i]);
            continue;
        }
        // old benders
        if (strcmp(argv[i], "-old_benders") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -old_benders");
            inst->old_benders = atoi(argv[++i]);
            continue;
        }

        // input file
        if (strcmp(argv[i], "-file") == 0 || strcmp(argv[i], "-input") == 0 || strcmp(argv[i], "-f") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing filename after -file option");
            strcpy(inst->input_file, argv[++i]);
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

        // Available memory
        if (strcmp(argv[i], "-memory") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -memory");
            inst->available_memory = atoi(argv[++i]);
            continue;
        }

        // Max number of nodes
        if (strcmp(argv[i], "-max_nodes") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -max_nodes");
            inst->max_nodes = atoi(argv[++i]);
            continue;
        }

        // Random seed
        if (strcmp(argv[i], "-seed") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -seed");
            inst->randomseed = abs(atoi(argv[++i]));
            continue;
        }

        // total time limit
        if (strcmp(argv[i], "-time_limit") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -time_limit");
            inst->timelimit = atof(argv[++i]);
            continue;
        }

        // cutoff
        if (strcmp(argv[i], "-cutoff") == 0)
        {
            if (i + 1 >= argc)
                print_error(" missing value for -cutoff");
            inst->cutoff = atof(argv[++i]);
            continue;
        }

        // integer costs
        if (strcmp(argv[i], "-int") == 0)
        {
            inst->integer_costs = 1;
            continue;
        }
        if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            help = 1;
            continue;
        }
    }
    if (help)
    {
        printf(COLOR_ORANGE);
        printf("----------------------------------------------------------------------\n");
        printf(" TSP/VRP SOLVER - Help Menu\n");
        printf("----------------------------------------------------------------------\n");
        printf("Usage: %s -file <filename> [options]\n\n", argv[0]);

        printf("GENERAL LOGIC & FILES:\n");
        printf("  -file <path>        Path to the .tsp or .vrp input file (Required)\n");
        printf("  -model_type <n>     Select model: 0,1,2, (To be Defined) \n");
        printf("  -old_benders <0|1>  Switch between new (0) or old (1) Benders decomposition\n");

        printf("\nPERFORMANCE & RESOURCES:\n");
        printf("  -threads <n>        Number of CPU threads (0 for automatic detection)\n");
        printf("  -memory <MB>        Maximum RAM allowed for CPLEX (e.g., 12000 for 12GB)\n");
        printf("  -max_nodes <n>      Limit branching nodes in the search tree (-1 for unlimited)\n");

        printf("\nOPTIMIZATION CONSTRAINTS:\n");
        printf("  -seed <n>           Set the random seed for reproducible results\n");
        printf("  -time_limit <s>     Maximum execution time in seconds before termination\n");
        printf("  -cutoff <val>       Ignore solutions with a cost higher than this value\n");
        printf("  -int                Force the model to treat all edge costs as integers\n");
        printf("----------------------------------------------------------------------\n");
        printf(COLOR_RESET);
        exit(1);
    }

    if (VERBOSE >= 2) // Level 2 for configuration summary
    {
        printf("\n\n" COLOR_MAGENTA "Configuration Summary:" COLOR_RESET "\n");
        printf("----------------------------------------------------------------------\n");
        printf(" TSP/VRP SOLVER - Current Configuration\n");
        printf("----------------------------------------------------------------------\n");

        printf(COLOR_MAGENTA "GENERAL LOGIC & FILES:\n" COLOR_RESET);
        printf("  -file <path>        : %s\n", inst->input_file);
        printf("  -model_type <n>     : %d \n", inst->model_type);
        printf("  -old_benders <0|1>  : %d \n", inst->old_benders);

        printf(COLOR_MAGENTA "\nPERFORMANCE & RESOURCES:\n" COLOR_RESET);
        printf("  -threads <n>        : %d (0=auto)\n", inst->num_threads);
        printf("  -memory <MB>        : %d MB\n", inst->available_memory);
        printf("  -max_nodes <n>      : %d (-1=unlimited)\n", inst->max_nodes);

        printf(COLOR_MAGENTA "\nOPTIMIZATION CONSTRAINTS:\n" COLOR_RESET);
        printf("  -seed <n>           : %d\n", inst->randomseed);
        printf("  -time_limit <s>     : %.2f sec\n", inst->timelimit);
        printf("  -cutoff <val>       : %.2f\n", inst->cutoff);
        printf("  -int                : %s\n", inst->integer_costs ? "Enabled" : "Disabled");
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
void print_tour(int *tour, int num_nodes) {
    printf(COLOR_BLUE "Tour sequence: " COLOR_RESET);
    for (int i = 0; i < num_nodes; i++) {
        // Shift back to 1-based indexing for output
        printf("%d ", tour[i] + 1); 
    }
    printf("\n");
}

/**
 * Validates the structural integrity of a given tour array and calculates its total cost.
 * Checks that the array contains a mathematically valid cycle with no duplicates or out-of-bound nodes.
 * @param tour Array representing the sequence of visited nodes.
 * @param inst Pointer to the instance structure containing node coordinates.
 * @return 1 if the sequence forms a valid continuous cycle, 0 if invalid.
 */
int check_tour(int *tour, instance *inst) {
    int num_nodes = inst->nnodes;
    int *visited = (int *)calloc(num_nodes, sizeof(int));
    
    for (int i = 0; i < num_nodes; i++) {
        int node = tour[i];
        if (node < 0 || node >= num_nodes) {
            printf(COLOR_RED "Error: Node %d is out of bounds!\n" COLOR_RESET, node);
            free(visited);
            return 0; // Invalid
        }
        visited[node]++;
    }
    
    for (int i = 0; i < num_nodes; i++) {
        if (visited[i] != 1) {
            printf(COLOR_RED "Error: Node %d was visited %d times (must be exactly 1)!\n" COLOR_RESET, i, visited[i]);
            free(visited);
            return 0; // Invalid
        }
    }
    
    // --- Calculate Total Cost ---
    double total_cost = 0.0;
    for (int i = 0; i < num_nodes - 1; i++) {
        total_cost += dist(tour[i], tour[i+1], inst); // Distance from i to i+1
    }
    // Add the distance to close the loop (from the last node back to the first)
    total_cost += dist(tour[num_nodes - 1], tour[0], inst);

    if (VERBOSE >= 1) {
        printf(COLOR_GREEN "\n... Tour check passed: Valid cycle.\n" COLOR_RESET);
        printf(COLOR_CYAN "... TOTAL TOUR COST: %.2f\n" COLOR_RESET, total_cost);
    }
    
    free(visited);
    return 1; // Valid cycle
}

/**
 * Generates a visual plot of the TSP tour using GNUplot.
 * It writes the node coordinates to a temporary GNUplot pipe and saves the graph as a PNG image.
 * @param inst Pointer to the instance structure containing the coordinate map.
 * @param tour Array representing the sequence of nodes to plot.
 */
void plot_tour(instance *inst, int *tour) {
    
    FILE *gnuplotPipe = popen("gnuplot", "w");
    if (!gnuplotPipe) {
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
    for (int i = 0; i < inst->nnodes; i++) {
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
int parse_tour(instance *inst, int *tour) {
    // Dynamically generate the optimal filename from the instance input file
    char opt_filename[1000];
    strcpy(opt_filename, inst->input_file);
    char *ext = strrchr(opt_filename, '.');
    if (ext != NULL) {
        strcpy(ext, ".opt.tour");
    } else {
        strcat(opt_filename, ".opt.tour"); // Fallback just in case
    }

    // Safely check if the file exists
    FILE *file = fopen(opt_filename, "r");
    if (!file) {
        if (VERBOSE >= 1)
            printf(COLOR_YELLOW "\n--- Notice: No optimal tour file found at %s ---\n" COLOR_RESET, opt_filename);
        return 0; // Return 0 (False) to tell main.c to skip the checks
    }
    
    if (VERBOSE >= 1)
        printf("\n" COLOR_MAGENTA "Loading optimal tour... " COLOR_RESET "\n");

    // Parse the file
    char line[256];
    int active_section = 0;
    int idx = 0;
    
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "TOUR_SECTION", 12) == 0) {
            active_section = 1;
            continue;
        }
        if (active_section) {
            int node = atoi(line);
            if (node == -1) break; 
            
            if (idx >= inst->nnodes) {
                print_error("Optimal tour file contains more nodes than the instance dimension!");
            }
            
            tour[idx++] = node - 1; 
        }
    }
    fclose(file);

    return 1; // Return 1 (True) to indicate success
}