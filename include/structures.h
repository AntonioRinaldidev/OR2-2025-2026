#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdbool.h>

typedef struct
{
    double cost;
    int *tour;

} solution;

typedef struct
{
    int nnodes;
    vertex *vertices;
    double *dists; // Flattened 2D array for distance matrix

    int randomseed;
    int percentage_elites;
    int num_threads;
    double timelimit;
    char input_file[1000];
    solution best_solution;
    bool opt_applied;
    char opt_name[50];

} instance;

typedef struct
{
    double xcoord;
    double ycoord;
} vertex;

#endif // STRUCTURES_H