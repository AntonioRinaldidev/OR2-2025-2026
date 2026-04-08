#ifndef STRUCTURES_H
#define STRUCTURES_H
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

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
    int nnodes;
    vertex *vertices;
    double *dists; // Flattened 2D array for distance matrix

    int randomseed;

    double timelimit;
    bool timelimit_reached;
    double start_time;
    char input_file[1000];
    FILE *gnuplot_pipe;
    pthread_mutex_t plot_mutex;

    int num_threads;

    solution best_solution;

    bool opt_applied;
    char opt_name[50];

    bool ga_applied;
    int population_size;
    CrossoverType crossover_type;
    int percentage_elites;

} instance;

#endif // STRUCTURES_H