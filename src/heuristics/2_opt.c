#include "heuristics/2_opt.h"
#include "core/structures.h"

double find_best_two_opt(instance *inst, solution *sol, int *pa, int *pb)
{
    double cost_diff, best_cost_diff = 0.0;
    *pa = -1;
    *pb = -1;

    for (int i = 0; i < inst->nnodes - 1; i++)
    {
        // j starts at i+2 to avoid checking adjacent edges (swapping adjacent edges does nothing)
        for (int j = i + 2; j < inst->nnodes; j++)
        {
            int next_i = (i + 1) % inst->nnodes;
            int next_j = (j + 1) % inst->nnodes;

            cost_diff = dist(sol->tour[i], sol->tour[j], inst) + dist(sol->tour[next_i], sol->tour[next_j], inst) - dist(sol->tour[i], sol->tour[next_i], inst) - dist(sol->tour[j], sol->tour[next_j], inst);

            if (cost_diff < best_cost_diff)
            {
                best_cost_diff = cost_diff;
                *pa = i;
                *pb = j;
            }
        }
    }
    return best_cost_diff;
}

void apply_two_opt(int *tour, const int pa, const int pb)
{
    int i = pa + 1;
    int j = pb;

    while (i < j)
    {
        swap(&tour[i], &tour[j]);
        i++;
        j--;
    }
}
