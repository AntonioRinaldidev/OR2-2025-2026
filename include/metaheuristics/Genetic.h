#ifndef GENETIC_H
#define GENETIC_H
#include "core/structures.h"
#include "core/utilities.h"
#include "construction/NearestN.h"
#include "heuristics/2_opt.h"
#include "modules/solver.h" // For apply_2opt_local_search
#include <time.h>
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

typedef struct
{
    int population_size;
    int generations;
    double mutation_rate;
    double crossover_rate;
    int elitism_count; // How many top solutions to keep automatically
    // bool use_2opt;     // Toggle for the memetic refinement
} GA_Params;
void crossover(const instance *inst, int *parent1, int *parent2, int *child1, int *child2);
void ox1_crossover(const instance *inst, int *parent1, int *parent2, int *child, int *visited_nodes);
void audit_children_and_repair(const instance *inst, int *child, int *freq, int *missing);
void *crossover_worker(void *args);
int compare_solutions(const void *a, const void *b);
void natural_selection(generation *gen, generation *new_gen);

void initilize_generation(generation *gen, double start_time);
void run_genetic_algorithm(instance *inst);
generation *create_generation(instance *inst, int pop_size);
void free_generation(generation *gen);
#endif // GENETIC_H