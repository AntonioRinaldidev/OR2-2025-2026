#include "vrp.h"

/**
 * Prints an error message in red to stdout and terminates the program.
 * @param err The error message string.
 */
void print_error(const char *err)
{
    printf("\n\n \033[1;31mERROR: %s \033[0m\n\n", err);
    fflush(NULL);
    exit(1);
}

/**
 * Parses the input file (TSPLIB format) to populate the instance structure.
 * Reads metadata (DIMENSION, CAPACITY) and data sections (COORDS, DEMAND, DEPOT).
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
    // =1 NODE_COORD_SECTION, =2 DEMAND_SECTION, =3 DEPOT_SECTION
    int do_print = (VERBOSE >= 4);

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (VERBOSE >= 5)
        {
            printf("%s", line);
            fflush(NULL); // makes sure the output is printed immediately
        }
        if (strlen(line) <= 1)
            continue; // skip empty lines

        par_name = strtok(line, " :");
        if (!par_name)
            continue;

        if (VERBOSE >= 6)
        {
            printf("parameter \"%s\" ", par_name);
            fflush(NULL);
        }

        if (strncmp(par_name, "NAME", 4) == 0)
        {
            active_section = 0;
            continue;
        }
        if (strncmp(par_name, "COMMENT", 7) == 0)
        {
            active_section = 0;
            token1 = strtok(NULL, "");
            if (VERBOSE >= 2)
                printf(" ... solving instance %s with model %d\n\n", token1, inst->model_type);
            continue;
        }
        if (strncmp(par_name, "TYPE", 4) == 0)
        {
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: TYPE field is missing value");
            if (strncmp(token1, "CVRP", 4) != 0)
                print_error(" format error:  only TYPE == CVRP implemented so far!!!!!!");
            active_section = 0;
            continue;
        }
        if (strncmp(par_name, "DIMENSION", 9) == 0)
        {
            if (inst->nnodes >= 0)
                print_error(" repeated DIMENSION section in input file");
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: DIMENSION field is missing value");
            inst->nnodes = atoi(token1);
            if (do_print)
                printf(" ... n. nodes %d\n", inst->nnodes);

            inst->demand = (double *)calloc(inst->nnodes, sizeof(double));
            inst->xcoord = (double *)calloc(inst->nnodes, sizeof(double));
            inst->ycoord = (double *)calloc(inst->nnodes, sizeof(double));
            if (inst->demand == NULL || inst->xcoord == NULL || inst->ycoord == NULL)
                print_error(" memory allocation error for node data");
            active_section = 0;
            continue;
        }
        if (strncmp(par_name, "CAPACITY", 8) == 0)
        {
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: CAPACITY field is missing value");
            inst->capacity = atof(token1);
            if (do_print)
                printf(" ... vehicle capacity %lf\n", inst->capacity);
            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "VEHICLES", 8) == 0)
        {
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: VEHICLES field is missing value");
            inst->nveh = atoi(token1);
            if (do_print)
                printf(" ... n. vehicles %d\n", inst->nveh);
            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "EDGE_WEIGHT_TYPE", 16) == 0)
        {
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: EDGE_WEIGHT_TYPE field is missing value");
            if (strncmp(token1, "EUC_2D", 6) != 0)
                print_error(" format error:  only EDGE_WEIGHT_TYPE == EUC_2D implemented so far!!!!!!");
            active_section = 0;
            continue;
        }

        if (strncmp(par_name, "NODE_COORD_SECTION", 18) == 0)
        {
            if (inst->nnodes <= 0)
                print_error(" ... DIMENSION section should appear before NODE_COORD_SECTION section");
            active_section = 1;
            continue;
        }

        if (strncmp(par_name, "DEMAND_SECTION", 14) == 0)
        {
            if (inst->nnodes <= 0)
                print_error(" ... DIMENSION section should appear before DEMAND_SECTION section");
            active_section = 2;
            continue;
        }

        if (strncmp(par_name, "DEPOT_SECTION", 13) == 0)
        {
            if (inst->depot >= 0)
                print_error(" ... DEPOT_SECTION repeated??");
            active_section = 3;
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
                print_error(" ... node index out of bounds in NODE_COORD_SECTION");
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: x coordinate missing in NODE_COORD_SECTION");
            inst->xcoord[i] = atof(token1);
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: y coordinate missing in NODE_COORD_SECTION");
            inst->ycoord[i] = atof(token1);
            if (do_print)
                printf(" ... node %4d at coordinates ( %15.7lf , %15.7lf )\n", i + 1, inst->xcoord[i], inst->ycoord[i]);
            continue;
        }
        else if (active_section == 2) // DEMAND_SECTION
        {
            int i = atoi(par_name) - 1;
            if (i < 0 || i >= inst->nnodes)
                print_error(" ... node index out of bounds in DEMAND_SECTION");
            token1 = strtok(NULL, " :");
            if (token1 == NULL)
                print_error(" format error: demand missing in DEMAND_SECTION");
            inst->demand[i] = atof(token1);
            if (do_print)
                printf(" ... node %4d has demand %10.5lf\n", i + 1, inst->demand[i]);
            continue;
        }
        else if (active_section == 3) // DEPOT_SECTION
        {
            int id = atoi(par_name);
            if (id == -1)
            {
                active_section = 0;
                continue;
            }
            int i = id - 1;
            if (i < 0 || i >= inst->nnodes)
                print_error(" ... node index out of bounds in DEPOT_SECTION");
            if (inst->depot >= 0)
                print_error(" ... multiple depots defined in DEPOT_SECTION");
            inst->depot = i;
            if (do_print)
                printf(" ... node %4d is a depot\n", i + 1);
            continue;
        }

        print_error(" ... wrong format for the current simplified parser!!!!!!!!!");
    }

    fclose(file);
}

/**
 * Parses command-line arguments to configure solver settings (file, time limit, threads, etc.).
 * Displays the help menu if arguments are missing or help is requested.
 * @param inst Pointer to the instance structure.
 */
void parse_command_line(int argc, char **argv, instance *inst)
{
    if (VERBOSE >= 4)
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
        printf("----------------------------------------------------------------------\n");
        printf(" TSP/VRP SOLVER - Help Menu\n");
        printf("----------------------------------------------------------------------\n");
        printf("Usage: %s -file <filename> [options]\n\n", argv[0]);

        printf("GENERAL LOGIC & FILES:\n");
        printf("  -file <path>        Path to the .tsp or .vrp input file (Required)\n");
        printf("  -model_type <n>     Select model: 0,1,2,... (To be Defined) \n");
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
        exit(1);
    }

    printf("----------------------------------------------------------------------\n");
    printf(" TSP/VRP SOLVER - Current Configuration\n");
    printf("----------------------------------------------------------------------\n");
    printf("Usage: %s -file <filename> [options]\n\n", argv[0]);

    printf("GENERAL LOGIC & FILES:\n");
    printf("  -file <path>        : %s\n", inst->input_file);
    printf("  -model_type <n>     : %d (To be Defined)\n", inst->model_type);
    printf("  -old_benders <0|1>  : %d (0=new, 1=old)\n", inst->old_benders);

    printf("\nPERFORMANCE & RESOURCES:\n");
    printf("  -threads <n>        : %d (0=auto)\n", inst->num_threads);
    printf("  -memory <MB>        : %d MB\n", inst->available_memory);
    printf("  -max_nodes <n>      : %d (-1=unlimited)\n", inst->max_nodes);

    printf("\nOPTIMIZATION CONSTRAINTS:\n");
    printf("  -seed <n>           : %d\n", inst->randomseed);
    printf("  -time_limit <s>     : %.2f sec\n", inst->timelimit);
    printf("  -cutoff <val>       : %.2f\n", inst->cutoff);
    printf("  -int                : %s\n", inst->integer_costs ? "Enabled" : "Disabled");
    printf("----------------------------------------------------------------------\n");
}
