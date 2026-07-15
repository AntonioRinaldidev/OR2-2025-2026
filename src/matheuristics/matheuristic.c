#include "matheuristics/matheuristic.h"
#include "modules/solver.h"
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

void solve_local_branching(instance *inst)
{
    int n       = inst->nnodes;
    int n_edges = n * (n - 1) / 2;

    // --- Warm start: VNS for 1/10 of the total time limit ---
    double orig_timelimit   = inst->timelimit;
    inst->timelimit         = orig_timelimit / 10.0;
    inst->opt_applied       = true;
    solve_tsp(inst, inst->start_time);
    inst->timelimit         = orig_timelimit;
    inst->timelimit_reached = false;

    if (inst->best_solution.tour == NULL)
    {
        printf("[LB] ERROR: warm start produced no initial solution.\n");
        return;
    }
    if (VERBOSE >= 1)
        printf(COLOR_CYAN "[LB]" COLOR_RESET " Warm start cost: " COLOR_ORANGE "%.2f" COLOR_RESET "\n",
               inst->best_solution.cost);

    // --- Build CPLEX model with B&C callbacks ---
    init_cplex(inst);
    build_model(inst, inst->env, inst->lp);

    CPXLONG contextmask = CPX_CALLBACKCONTEXT_THREAD_UP   |
                          CPX_CALLBACKCONTEXT_THREAD_DOWN  |
                          CPX_CALLBACKCONTEXT_CANDIDATE    |
                          CPX_CALLBACKCONTEXT_RELAXATION;
    if (CPXcallbacksetfunc(inst->env, inst->lp, contextmask, callback_driver, inst) != 0)
        printf("[LB] Error setting callback function.\n");

    // Build successor array for the current reference solution x^0
    int *succ = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++)
        succ[inst->best_solution.tour[i]] = inst->best_solution.tour[(i + 1) % n];

    // Warm MIP start
    {
        int    beg[1]    = {0};
        int    effort[1] = {CPX_MIPSTART_CHECKFEAS};
        int   *varidx    = (int *)   malloc(n * sizeof(int));
        double *vals     = (double *)malloc(n * sizeof(double));
        for (int i = 0; i < n; i++)
        {
            varidx[i] = xpos(inst, i, succ[i]);
            vals[i]   = 1.0;
        }
        CPXaddmipstarts(inst->env, inst->lp, 1, n, beg, varidx, vals, effort, NULL);
        free(varidx);
        free(vals);
    }

    // Separation workspace
    separationThreadWorkspace ws;
    ws.adj_capacity = 4;
    ws.succ         = (int *)malloc(n * sizeof(int));
    ws.comp         = (int *)malloc(n * sizeof(int));
    ws.visited      = (int *)calloc(n, sizeof(int));
    ws.adj_count    = (int *)calloc(n, sizeof(int));
    ws.adj          = (int **)malloc(n * sizeof(int *));
    ws.queue        = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++)
        ws.adj[i] = (int *)malloc(ws.adj_capacity * sizeof(int));

    double *xstar = (double *)malloc(n_edges * sizeof(double));

    int k    = inst->lb_k_init;
    int iter = 0;

    // --- Main local branching loop ---
    while (!timelimit_check(inst, inst->start_time))
    {
        iter++;
        double remaining = orig_timelimit - (get_wall_time() - inst->start_time);
        if (remaining <= 0.0)
            break;

        // Refresh MIP start with current x^0 so CPLEX warm-starts from the
        // updated reference solution, not the original one.
        {
            int nmip = CPXgetnummipstarts(inst->env, inst->lp);
            if (nmip > 0)
                CPXdelmipstarts(inst->env, inst->lp, 0, nmip - 1);
            int    beg[1]    = {0};
            int    effort[1] = {CPX_MIPSTART_CHECKFEAS};
            int   *varidx    = (int *)   malloc(n * sizeof(int));
            double *vals     = (double *)malloc(n * sizeof(double));
            int curr = 0;
            for (int i = 0; i < n; i++)
            {
                varidx[i] = xpos(inst, curr, succ[curr]);
                vals[i]   = 1.0;
                curr      = succ[curr];
            }
            CPXaddmipstarts(inst->env, inst->lp, 1, n, beg, varidx, vals, effort, NULL);
            free(varidx);
            free(vals);
        }

        // Constraint: sum_{e in x^0} x_e >= n - k
        // Built from succ (the current x^0), not inst->best_solution which only
        // updates on improvement — using succ ensures x^0 always diversifies.
        int   *lb_idx  = (int *)   malloc(n * sizeof(int));
        double *lb_val = (double *)malloc(n * sizeof(double));
        {
            int curr = 0;
            for (int i = 0; i < n; i++)
            {
                lb_idx[i] = xpos(inst, curr, succ[curr]);
                lb_val[i] = 1.0;
                curr      = succ[curr];
            }
        }
        double rhs   = (double)(n - k);
        char   sense = 'G';
        int    izero = 0;

        int lb_row_idx = CPXgetnumrows(inst->env, inst->lp);
        if (CPXaddrows(inst->env, inst->lp, 0, 1, n,
                       &rhs, &sense, &izero, lb_idx, lb_val, NULL, NULL) != 0)
            printf("[LB] Error adding local branching constraint.\n");
        free(lb_idx);
        free(lb_val);

        // Sub-problem time limit: 1/10 of remaining, minimum 2 s
        double sub_time = remaining / 10.0;
        if (sub_time < 2.0) sub_time = 2.0;
        CPXsetdblparam(inst->env, CPX_PARAM_TILIM, sub_time);

        CPXmipopt(inst->env, inst->lp);
        int solstat = CPXgetstat(inst->env, inst->lp);

        bool improved = false;

        if (solstat == CPXMIP_OPTIMAL      ||
            solstat == CPXMIP_OPTIMAL_TOL   ||
            solstat == CPXMIP_TIME_LIM_FEAS)
        {
            CPXgetx(inst->env, inst->lp, xstar, 0, n_edges - 1);
            separate_integer_solution(inst, xstar, NULL, &ws);

            double new_cost = 0.0;
            for (int i = 0; i < n; i++)
                new_cost += dist(i, ws.succ[i], inst);

            // Always replace x^0 with the best solution CPLEX found
            memcpy(succ, ws.succ, n * sizeof(int));

            if (new_cost < inst->best_solution.cost - EPSILON)
            {
                improved = true;

                solution new_sol;
                new_sol.cost = new_cost;
                new_sol.tour = (int *)malloc(n * sizeof(int));
                int curr = 0;
                for (int i = 0; i < n; i++)
                {
                    new_sol.tour[i] = curr;
                    curr            = succ[curr];
                }
                update_best_solution(inst, &new_sol);
                free(new_sol.tour);

                if (VERBOSE >= 1)
                    printf(COLOR_GREEN "[LB] iter %d | New best: %.2f (k=%d)" COLOR_RESET "\n",
                           iter, inst->best_solution.cost, k);
            }
        }

        // Remove the local branching constraint. It sits at lb_row_idx;
        // any SECs added by separate_integer_solution landed at higher
        // indices and are kept to tighten future sub-problems.
        CPXdelrows(inst->env, inst->lp, lb_row_idx, lb_row_idx);

        // --- Adaptive k update (only when x^0 did not improve) ---
        if (!improved)
        {
            int old_k = k;
            if (solstat == CPXMIP_OPTIMAL || solstat == CPXMIP_OPTIMAL_TOL)
            {
                // Neighborhood proved locally optimal — widen search
                k = (k + inst->lb_k_step <= inst->lb_k_max)
                      ? k + inst->lb_k_step : inst->lb_k_max;
                if (VERBOSE >= 2 && k != old_k)
                    printf("[LB] iter %d | Neighborhood optimal, k -> %d\n", iter, k);
            }
            else
            {
                // Sub-problem hit time limit — neighborhood too large, shrink
                k = (k - inst->lb_k_step >= inst->lb_k_min)
                      ? k - inst->lb_k_step : inst->lb_k_min;
                if (VERBOSE >= 2 && k != old_k)
                    printf("[LB] iter %d | Time limit in sub-problem, k -> %d\n", iter, k);
            }
        }
    }

    // --- Cleanup ---
    free(xstar);
    free(succ);
    free(ws.succ);
    free(ws.comp);
    free(ws.visited);
    free(ws.adj_count);
    free(ws.queue);
    for (int i = 0; i < n; i++)
        free(ws.adj[i]);
    free(ws.adj);

    CPXfreeprob(inst->env, &inst->lp);
    CPXcloseCPLEX(&inst->env);
}