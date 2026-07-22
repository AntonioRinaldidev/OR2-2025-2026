#ifndef UTILITIES_H

#define UTILITIES_H

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <ilcplex/cplex.h>
#include <pthread.h>

#include "structures.h"

#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"

#define COLOR_ORANGE "\033[38;5;208m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_RESET "\033[0m"
extern double EPSILON; // Tolerance for floating-point comparisons in cost validation
extern int VERBOSE;    // Verbosity level (0=silent, 1=info, 2=default, 3=detail, 4=debug, 5=trace)
/*
 * 0: Silent mode (only fatal errors)
 * 1: Info mode (final results, major status changes)
 * 2: Default mode (configuration summaries, default execution info)
 * 3: Detail mode (per-iteration results, e.g., multi-start loops)
 * 4: Debug mode (low-level details, e.g., parsing info)
 * 5: Trace mode (raw line-by-line data for deep debugging)
 */
#define INF 1e30

#define NUM_DECIMALS 2
#define COST_MULTIPLIER 100 // 10^NUM_DECIMALS

void free_instance(instance *inst);
void print_error(const char *err);
void parse_instance(instance *inst);
void parse_command_line(int argc, char **argv, instance *inst);
void swap(int *a, int *b);
void update_best_solution(instance *inst, solution *new_sol);
bool timelimit_check(instance *inst, double start_time);
double get_wall_time();
void open_gnuplot(instance *inst);
void refresh_gnuplot(instance *inst);
void close_gnuplot(instance *inst);
void log_result(instance *inst);

// --- TSP UTILITY FUNCTIONS ---
void print_tour(int *tour, int num_nodes);
int is_tour_feasible(solution *sol, instance *inst);
void compute_distances(instance *inst);
double calculate_cost(instance *inst, int *tour);
void plot_tour(instance *inst, int *tour, char *title);
int parse_tour(instance *inst, int *tour);
void generate_random_tour(instance *inst, int *tour);
double generate_random_number();

static inline double dist(int i, int j, instance *inst)
{
    if (inst->dists)
        return sqrt(inst->dists[i * inst->nnodes + j]);
    double dx = inst->vertices[i].xcoord - inst->vertices[j].xcoord;
    double dy = inst->vertices[i].ycoord - inst->vertices[j].ycoord;
    return sqrt(dx * dx + dy * dy);
}

static inline double dist_sq(int i, int j, instance *inst)
{
    if (inst->dists)
        return inst->dists[i * inst->nnodes + j];
    double dx = inst->vertices[i].xcoord - inst->vertices[j].xcoord;
    double dy = inst->vertices[i].ycoord - inst->vertices[j].ycoord;
    return dx * dx + dy * dy;
}

// --- END TSP UTILITY FUNCTIONS ---

// --- INSTANCES RANDOM GENERATOR ---
void generate_random_instance(instance *inst, double x_max, double y_max);
int save_instance_to_tsp(instance *inst, const char *filepath);
#endif