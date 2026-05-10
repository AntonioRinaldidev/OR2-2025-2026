#include "matheuristics/matheuristic.h"
#include <ilcplex/cplex.h>

void change_bound(instance *inst, int edge_idx, double lb, double ub)
{
    int indices[1] = {edge_idx};
    char lu_lower[1] = {'L'};
    char lu_upper[1] = {'U'};
    double lb_val[1] = {lb};
    double ub_val[1] = {ub};

    if (CPXchgbds(inst->env, inst->lp, 1, indices, lu_lower, lb_val) != 0)
        printf("Error changing lower bound for edge %d\n", edge_idx);

    if (CPXchgbds(inst->env, inst->lp, 1, indices, lu_upper, ub_val) != 0)
        printf("Error changing upper bound for edge %d\n", edge_idx);
}

void solve_matheuristic(instance *inst, double p)
{

    if (inst->best_solution.tour == NULL || inst->pool_size == 0)
    {
        printf("ERROR: No initial solution available for matheuristic.\n");
        return;
    }
    int n_edges = inst->nnodes * (inst->nnodes - 1) / 2;
    int n_fixed = 0;
    int *fixed_indices = (int *)malloc(n_edges * sizeof(int));

    int *succ = (int *)malloc(inst->nnodes * sizeof(int));

    for (int i = 0; i < inst->nnodes; i++)
    {
        succ[inst->best_solution.tour[i]] = inst->best_solution.tour[(i + 1) % inst->nnodes];
    }

    init_cplex(inst);
    build_model(inst, inst->env, inst->lp);
    CPXLONG contextmask = CPX_CALLBACKCONTEXT_THREAD_UP |
                          CPX_CALLBACKCONTEXT_THREAD_DOWN |
                          CPX_CALLBACKCONTEXT_CANDIDATE |
                          CPX_CALLBACKCONTEXT_RELAXATION;

    if (CPXcallbacksetfunc(inst->env, inst->lp, contextmask, callback_driver, inst) != 0)
    {
        printf("Error setting callback function\n");
    }

    int total_nz = inst->pool_size * inst->nnodes;
    int *beg = (int *)malloc(inst->pool_size * sizeof(int));
    int *varindices = (int *)malloc(total_nz * sizeof(int));
    double *values = (double *)malloc(total_nz * sizeof(double));
    int *effortlevel = (int *)malloc(inst->pool_size * sizeof(int));

    for (int k = 0; k < inst->pool_size; k++)
    {

        for (int i = 0; i < inst->nnodes; i++)
        {
            succ[inst->solution_pool[k].tour[i]] = inst->solution_pool[k].tour[(i + 1) % inst->nnodes];
        }

        beg[k] = k * inst->nnodes;
        effortlevel[k] = CPX_MIPSTART_CHECKFEAS;
        for (int i = 0; i < inst->nnodes; i++)
        {
            varindices[beg[k] + i] = xpos(inst, i, succ[i]);
            values[beg[k] + i] = 1.0;
        }
    }

    CPXaddmipstarts(inst->env, inst->lp,
                    inst->pool_size,
                    total_nz,
                    beg,
                    varindices,
                    values,
                    effortlevel,
                    NULL);

    free(beg);
    free(varindices);
    free(values);
    free(effortlevel);

    for (int i = 0; i < inst->nnodes; i++)
        succ[inst->best_solution.tour[i]] = inst->best_solution.tour[(i + 1) % inst->nnodes];

    double *xstar = (double *)malloc(n_edges * sizeof(double));
    separationThreadWorkspace ws;
    ws.adj_capacity = 4;
    ws.succ = (int *)malloc(inst->nnodes * sizeof(int));
    ws.comp = (int *)malloc(inst->nnodes * sizeof(int));
    ws.visited = (int *)calloc(inst->nnodes, sizeof(int));
    ws.adj_count = (int *)calloc(inst->nnodes, sizeof(int));
    ws.adj = (int **)malloc(inst->nnodes * sizeof(int *));
    ws.queue = (int *)malloc(inst->nnodes * sizeof(int));
    for (int i = 0; i < inst->nnodes; i++)
        ws.adj[i] = (int *)malloc(ws.adj_capacity * sizeof(int));

    while (!timelimit_check(inst, inst->start_time))
    {
        n_fixed = 0;
        for (int i = 0; i < inst->nnodes; i++)
        {

            double r = generate_random_number();
            if (r < p)
            {
                change_bound(inst, xpos(inst, i, succ[i]), 1, 1);
                fixed_indices[n_fixed++] = xpos(inst, i, succ[i]);
            }
        }
        double remaining = inst->timelimit - (get_wall_time() - inst->start_time);
        CPXsetdblparam(inst->env, CPX_PARAM_TILIM, remaining);
        CPXmipopt(inst->env, inst->lp);
        CPXgetx(inst->env, inst->lp, xstar, 0, n_edges - 1);

        separate_integer_solution(inst, xstar, NULL, &ws);
        if (CPXgetstat(inst->env, inst->lp) == CPXMIP_OPTIMAL || CPXgetstat(inst->env, inst->lp) == CPXMIP_OPTIMAL_TOL || CPXgetstat(inst->env, inst->lp) == CPXMIP_TIME_LIM_FEAS)
        {
            double new_cost = 0.0;
            for (int i = 0; i < inst->nnodes; i++)
                new_cost += dist(i, ws.succ[i], inst);

            if (new_cost < inst->best_solution.cost - EPSILON)
            {
                memcpy(succ, ws.succ, inst->nnodes * sizeof(int));

                solution new_sol;
                new_sol.cost = new_cost;
                new_sol.tour = (int *)malloc(inst->nnodes * sizeof(int));
                int curr = 0;
                for (int i = 0; i < inst->nnodes; i++)
                {
                    new_sol.tour[i] = curr;
                    curr = succ[curr];
                }
                update_best_solution(inst, &new_sol);
                p *= 1.1;
                if (p > 0.9)
                    p = 0.9;
                if (p < 0.1)
                    p = 0.1;
                free(new_sol.tour);
            }
            else
            {
                p *= 0.9;
                if (p > 0.9)
                    p = 0.9;
                if (p < 0.1)
                    p = 0.1;
            }

            // unfix all variables
            for (int i = 0; i < n_fixed; i++)
                change_bound(inst, fixed_indices[i], 0.0, 1.0);
        }
        else
        {
            for (int i = 0; i < n_fixed; i++)
                change_bound(inst, fixed_indices[i], 0.0, 1.0);
            p *= 0.9;
            if (p > 0.9)
                p = 0.9;
            if (p < 0.1)
                p = 0.1;
        }
    }

    free(xstar);
    free(succ);
    free(fixed_indices);
    free(ws.succ);
    free(ws.comp);
    free(ws.visited);
    free(ws.adj_count);
    free(ws.queue);
    for (int i = 0; i < inst->nnodes; i++)
        free(ws.adj[i]);
    free(ws.adj);

    CPXfreeprob(inst->env, &inst->lp);
    CPXcloseCPLEX(&inst->env);
}