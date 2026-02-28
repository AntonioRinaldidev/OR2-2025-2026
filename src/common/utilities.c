#include "vrp.h"

void parse_instance(instance *inst)
{
}

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
            inst->model_type = atoi(argv[++i]);
            continue;
        }
        // old benders
        if (strcmp(argv[i], "-old_benders") == 0)
        {
            inst->old_benders = atoi(argv[++i]);
            continue;
        }

        // input file
        if (strcmp(argv[i], "-file") == 0 || strcmp(argv[i], "-input") == 0 || strcmp(argv[i], "-f") == 0)
        {
            strcpy(inst->input_file, argv[++i]);
            continue;
        }

        // Number of threads
        if (strcmp(argv[i], "-threads") == 0)
        {
            inst->num_threads = atoi(argv[++i]);
            continue;
        }

        // Available memory
        if (strcmp(argv[i], "-memory") == 0)
        {
            inst->available_memory = atoi(argv[++i]);
            continue;
        }

        // Max number of nodes
        if (strcmp(argv[i], "-max_nodes") == 0)
        {
            inst->max_nodes = atoi(argv[++i]);
            continue;
        }

        // Random seed
        if (strcmp(argv[i], "-seed") == 0)
        {
            inst->randomseed = abs(atoi(argv[++i]));
            continue;
        }

        // total time limit
        if (strcmp(argv[i], "-time_limit") == 0)
        {
            inst->timelimit = atof(argv[++i]);
            continue;
        }

        // cutoff
        if (strcmp(argv[i], "-cutoff") == 0)
        {
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
