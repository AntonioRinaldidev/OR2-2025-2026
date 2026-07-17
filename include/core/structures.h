#ifndef STRUCTURES_H
#define STRUCTURES_H
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <ilcplex/cplex.h>

typedef struct
{
    double cost;
    int *tour;

} solution;

typedef struct
{
    double xcoord;
    double ycoord;
} vertex;

typedef enum
{
    CROSSOVER_NAIVE = 0,
    CROSSOVER_OX1 = 1
} CrossoverType;

typedef struct
{
    int *succ;        // Stores directed successors for cycle walking
    int *comp;        // Stores component IDs (subtours)
    int *visited;     // Boolean array for component identification
    int **adj;        // Adjacency list representation of current candidate xstar
    int *adj_count;   // Number of neighbors for each node
    int adj_capacity; // Capacity per node
    int *queue;       // BFS queue

} separationThreadWorkspace;

typedef struct
{
    int nnodes;
    vertex *vertices;
    double *dists; // Flattened 2D array for distance matrix

    int randomseed;

    double timelimit;
    bool timelimit_reached;
    double start_time;
    char input_file[1000];
    FILE *gnuplot_pipe;

    int num_threads;

    solution best_solution;
    solution *solution_pool;
    int pool_size;
    int max_pool_size;

    bool opt_applied;
    char opt_name[50];

    bool ga_applied;
    int population_size;
    CrossoverType crossover_type;
    int percentage_elites;
    int percentage_discard;
    int tournament_strength;

    CPXENVptr env;
    CPXLPptr lp;

    bool use_cplex;
    separationThreadWorkspace *thread_workspaces[128];

    bool use_matheuristic;
    bool use_local_branching;

    // Local branching hyperparameters
    int lb_k_init;
    int lb_k_min;
    int lb_k_max;
    int lb_k_step;

} instance;

typedef struct
{
    instance *inst;
    unsigned int rand_seed;
    int start; // primo start_node di questo thread
    int end;   // ultimo start_node di questo thread
    double start_time;
    solution best; // miglior soluzione trovata da questo thread
} solver_thread_args;

#endif // STRUCTURES_H