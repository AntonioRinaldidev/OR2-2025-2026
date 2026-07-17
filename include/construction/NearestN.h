#ifndef NEARESTN_H
#define NEARESTN_H
#include "core/utilities.h"

void greedyNN(instance *inst, solution *sol, int start_node);
void cardinality_grasp(instance *inst, solution *sol, int cardinality, int start_node, unsigned int *seed);
void value_based_grasp(instance *inst, double alpha, solution *sol, int start_node, unsigned int *seed);

#endif // NEARESTN_H