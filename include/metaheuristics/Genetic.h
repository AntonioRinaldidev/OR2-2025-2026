#ifndef GENETIC_H
#define GENETIC_H
#include "core/structures.h"
#include <pthread.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
/**
 * @brief Represents a single generation in the Genetic Algorithm.
 *
 * Since the population size is determined at runtime, the 'population'
 * member is a pointer. It acts as a dynamic array of 'solution' structures,
 * requiring memory allocation (e.g., via malloc or calloc) during initialization.
 */
typedef struct
{
    instance *inst;        // Pointer to the problem instance (contains nnodes, coordinates, etc.)
    int generation_number; // The index of the current generation
    int population_size;   // Number of individuals (solutions) in this generation
    solution *population;  // Pointer to dynamically allocate the array of solutions
    solution *champion;    // Pointer to the best solution of the generation
} generation;

typedef struct
{
    const generation *gen;
    solution *pool;
    int start_index;
    int end_index;
    int *freq;
    int *missing;
    int *visited_nodes;
} crossover_args;
void crossover(const instance *inst, int *parent1, int *parent2, int *child1, int *child2);
void ox1_crossover(const instance *inst, int *parent1, int *parent2, int *child, int *visited_nodes);
void audit_children_and_repair(const instance *inst, int *child, int *freq, int *missing);
void *crossover_worker(void *args);
int compare_solutions(const void *a, const void *b);
void natural_selection(generation *gen, generation *new_gen);
#endif // GENETIC_H