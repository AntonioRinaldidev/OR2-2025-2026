#include "core/utilities.h"
#include <ilcplex/cplex.h>

/**
 * @brief Initializes the CPLEX environment and creates an empty problem object.
 *
 * This function acts as the "handshake" between the C code and the IBM ILOG CPLEX optimization engine.
 * @param inst Pointer to the problem instance, which stores the CPLEX environment and problem pointers.
 */
void init_cplex(instance *inst)
{
    int status = 0;
    printf("DEBUG: Attempting to open CPLEX environment...\n");
    fflush(stdout);

    // 1. Open the CPLEX environment (allocates memory and checks license)
    inst->env = CPXopenCPLEX(&status);

    if (inst->env == NULL)
    {
        char errmsg[1024];
        CPXgeterrorstring(NULL, status, errmsg);
        printf("LICENSE/ENV ERROR (Code %d): %s\n", status, errmsg);
        exit(1);
    }

    printf("DEBUG: Environment created successfully.\n");

    CPXsetintparam(inst->env, CPX_PARAM_SCRIND, CPX_ON);
    CPXsetintparam(inst->env, CPX_PARAM_RANDOMSEED, 1234);

    // Maps the instance time limit to the CPLEX internal parameter
    CPXsetdblparam(inst->env, CPX_PARAM_TILIM, inst->timelimit);
    CPXsetdblparam(inst->env, CPX_PARAM_EPINT, 0.0);

    // 2. Create the problem container (the "shell" for our TSP model)
    inst->lp = CPXcreateprob(inst->env, &status, "TSP_Model");
    if (status != 0)
    {
        printf("Error creating CPLEX problem (Code %d).\n", status);
        exit(1);
    }

    printf("DEBUG: Problem container 'TSP_Model' created.\n");

    // 3. Enable screen indicator so CPLEX prints its progress to the terminal
    CPXsetintparam(inst->env, CPX_PARAM_SCRIND, CPX_ON);

    printf("DEBUG: CPLEX initialization complete.\n");
    fflush(stdout);
}

/**
 * @brief Maps a pair of nodes (i, j) to a unique 1D index for the CPLEX variable array.
 *
 * To optimize memory and reduce the number of variables, the formulation only creates
 * variables for the upper triangle of the adjacency matrix (since the graph is undirected).
 *
 * @param inst Pointer to the problem instance.
 * @param i Index of the first node.
 * @param j Index of the second node.
 * @return The 1D integer index corresponding to the edge between node i and node j.
 */
int xpos(instance *inst, int i, int j)
{
    // A node cannot have an edge to itself
    if (i == j)
        print_error("i == j in xpos");

    // Ensure symmetry: we only store variables where i < j
    if (i > j)
        return xpos(inst, j, i);

    /**
     * The formula: i*n + j - (i+1)*(i+2)/2
     * This "flattens" the 2D matrix while skipping the diagonal and lower triangle.
     */
    int pos = i * inst->nnodes + j - ((i + 1) * (i + 2)) / 2;
    return pos;
}

/**
 * @brief Populates the CPLEX model with variables (Columns) and constraints (Rows).
 *
 * This function defines the base variables (binary edge selections) and the primary
 * degree constraints (each node must have exactly 2 incident edges).
 *
 * @param inst Pointer to the problem instance.
 * @param env CPLEX environment pointer.
 * @param lp CPLEX problem pointer.
 */
void build_model(instance *inst, CPXENVptr env, CPXLPptr lp)
{
    // Define the 'personality' of our constraints and variables
    char binary = 'B'; // Variable type: Binary (0 or 1)
    double rhs = 2.0;  // Every node must have degree 2
    char sense = 'E';  // Constraint sense: Equality (=)
    int izero = 0;     // Buffer offset for adding rows one-by-one

    // Allocate a buffer for variable and constraint names
    char **cname = (char **)calloc(1, sizeof(char *));
    cname[0] = (char *)calloc(100, sizeof(char));

    /**
     * STEP 1: ADD COLUMNS (Variables)
     * For every unique pair (i, j), create a binary variable x_ij.
     */
    for (int i = 0; i < inst->nnodes; i++)
    {
        for (int j = i + 1; j < inst->nnodes; j++)
        {
            // Name the variable x(node1, node2) using 1-based indexing
            sprintf(cname[0], "x(%d,%d)", i + 1, j + 1);

            double obj = dist(i, j, inst); // Objective coefficient = distance
            double lb = 0.0;               // Lower bound
            double ub = 1.0;               // Upper bound

            // Commits the variable to the CPLEX engine
            if (CPXnewcols(env, lp, 1, &obj, &lb, &ub, &binary, cname))
            {
                print_error("wrong CPXnewcols on x var.s");
            }

            // Sanity Check: Ensure our xpos logic matches CPLEX's internal indexing
            if (CPXgetnumcols(env, lp) - 1 != xpos(inst, i, j))
            {
                print_error("Variable index mismatch in xpos vs CPLEX");
            }
        }
    }

    /**
     * STEP 2: ADD ROWS (Degree Constraints)
     * For every node h, the sum of incident edges must equal 2.
     */
    int *index = (int *)malloc(inst->nnodes * sizeof(int));
    double *value = (double *)malloc(inst->nnodes * sizeof(double));

    for (int h = 0; h < inst->nnodes; h++)
    {
        int nnz = 0; // Number of Non-Zero elements in this specific constraint
        for (int i = 0; i < inst->nnodes; i++)
        {
            if (i == h)
                continue; // Skip self-loops

            // Find the ID of the variable connecting h and i
            index[nnz] = xpos(inst, i, h);
            value[nnz] = 1.0; // The variable's weight in the sum is 1.0
            nnz++;
        }

        sprintf(cname[0], "degree(%d)", h + 1);

        /**
         * Adds the rule: Sum of x_ih (for all i) = 2.0
         * CPLEX translates this into one row of the constraint matrix.
         */
        if (CPXaddrows(env, lp, 0, 1, nnz, &rhs, &sense, &izero, index, value, NULL, cname))
        {
            print_error("wrong CPXaddrows [degree]");
        }
    }

    // Clean up temporary memory
    free(index);
    free(value);
    free(cname[0]);
    free(cname);
}

/**
 * @brief Identifies disconnected cycles (subtours) in the current solution.
 *
 * It traverses the successors array to assign a unique component ID to each
 * group of connected nodes. A valid TSP solution will only have 1 component.
 *
 * @param inst Pointer to the problem instance.
 * @param succ Array containing the successor of each node.
 * @param comp Array to be populated with the component ID for each node.
 * @return The total number of disconnected components found.
 */
int find_components(instance *inst, int *succ, int *comp)
{
    int n_comp = 0;
    for (int i = 0; i < inst->nnodes; i++)
        comp[i] = -1;

    for (int i = 0; i < inst->nnodes; i++)
    {
        if (comp[i] != -1)
            continue;
        n_comp++;
        int curr = i;
        while (comp[curr] == -1)
        {
            comp[curr] = n_comp;
            curr = succ[curr];
        }
    }
    return n_comp;
}

/**
 * @brief Adds Subtour Elimination Constraints (SECs) to the model to break invalid cycles.
 *
 * Formulates constraints ensuring that any identified subtour of size S cannot have
 * more than S - 1 internal edges. It adds these SECs dynamically via the callback context.
 *
 * @param inst Pointer to the problem instance.
 * @param n_comp Number of disconnected components identified.
 * @param comp Array containing the component ID of each node.
 * @param context CPLEX callback context (if NULL, SECs are added directly to the static model).
 */
void add_SECs(instance *inst, int n_comp, int *comp, CPXCALLBACKCONTEXTptr context)
{
    int *comp_size = (int *)calloc(n_comp + 1, sizeof(int));
    int *node_list = (int *)malloc(inst->nnodes * sizeof(int));
    int *comp_start = (int *)malloc((n_comp + 2) * sizeof(int));

    for (int i = 0; i < inst->nnodes; i++)
        comp_size[comp[i]]++;

    comp_start[1] = 0;
    for (int k = 1; k <= n_comp; k++)
        comp_start[k + 1] = comp_start[k] + comp_size[k];

    int *temp_count = (int *)calloc(n_comp + 1, sizeof(int));
    for (int i = 0; i < inst->nnodes; i++)
    {
        int c = comp[i];
        node_list[comp_start[c] + temp_count[c]] = i;
        temp_count[c]++;
    }

    int max_n_s = (inst->nnodes / 2) + 1;
    int max_edges = (max_n_s * (max_n_s - 1)) / 2;
    int *index = (int *)malloc(max_edges * sizeof(int));
    double *value = (double *)malloc(max_edges * sizeof(double));

    for (int k = 1; k <= n_comp; k++)
    {
        int nodes_in_subtour = comp_size[k];
        if (nodes_in_subtour > inst->nnodes / 2)
            continue;

        int nnz = 0;
        int start_idx = comp_start[k];
        int end_idx = comp_start[k + 1];

        for (int i_ptr = start_idx; i_ptr < end_idx; i_ptr++)
        {
            int i = node_list[i_ptr];
            for (int j_ptr = i_ptr + 1; j_ptr < end_idx; j_ptr++)
            {
                int j = node_list[j_ptr];
                index[nnz] = xpos(inst, i, j);
                value[nnz] = 1.0;
                nnz++;
            }
        }

        double rhs = (double)nodes_in_subtour - 1.0;
        char sense = 'L';
        int izero = 0;

        if (context == NULL)
        {
            CPXaddrows(inst->env, inst->lp, 0, 1, nnz, &rhs, &sense, &izero, index, value, NULL, NULL);
        }
        else
        {
            CPXcallbackrejectcandidate(context, 1, nnz, &rhs, &sense, &izero, index, value);
        }
    }

    free(comp_size);
    free(node_list);
    free(comp_start);
    free(temp_count);
    free(index);
    free(value);
}

/**
 * @brief Merges multiple subtours into a single Hamiltonian cycle (Feasible Tour).
 *
 * Evaluates potential edge swaps between different components to patch them together
 * greedily. It anchors on component '1' and incrementally absorbs other components.
 *
 * @param inst Pointer to the problem instance.
 * @param succ Array containing the successor of each node (modified in-place).
 * @param comp Array containing the component ID of each node.
 * @param n_comp Number of disconnected components.
 */
void patching_heuristic(instance *inst, int *succ, int *comp, int n_comp)
{
    if (n_comp <= 1)
        return;

    // We merge every component k > 1 into component 1
    for (int k = 2; k <= n_comp; k++)
    {
        double best_delta = INFINITY;
        int best_i = -1;
        int best_j = -1;

        // Find the best pair (i, j) to swap
        for (int i = 0; i < inst->nnodes; i++)
        {
            if (comp[i] != k)
                continue; // Node i must be in the current component k

            for (int j = 0; j < inst->nnodes; j++)
            {
                if (comp[j] != 1)
                    continue; // Node j must be in the 'main' component 1

                int i_next = succ[i];
                int j_next = succ[j];

                // Calculate the cost of swapping edges (i, i_next) and (j, j_next)
                // for (i, j_next) and (j, i_next)
                double delta = (dist(i, j_next, inst) + dist(j, i_next, inst)) -
                               (dist(i, i_next, inst) + dist(j, j_next, inst));

                if (delta < best_delta)
                {
                    best_delta = delta;
                    best_i = i;
                    best_j = j;
                }
            }
        }

        // Apply the best swap found for this component
        if (best_i != -1 && best_j != -1)
        {
            int i_next = succ[best_i];
            int j_next = succ[best_j];

            succ[best_i] = j_next;
            succ[best_j] = i_next;

            // Mark all nodes of the merged component k as now belonging to component 1
            for (int node = 0; node < inst->nnodes; node++)
            {
                if (comp[node] == k)
                    comp[node] = 1;
            }
        }
    }
}

/**
 * @brief Posts a valid heuristic solution to the CPLEX engine to update the incumbent.
 *
 * Converts a successor array (1D path representation) back into CPLEX binary variables,
 * then submits it. This gives the solver a tighter upper bound, pruning the search tree.
 *
 * @param inst Pointer to the problem instance.
 * @param context CPLEX callback context.
 * @param succ Array containing the successor sequence of the valid tour.
 * @param obj The objective cost of the heuristic solution.
 */
void post_heuristic_solution(instance *inst, CPXCALLBACKCONTEXTptr context, int *succ, double obj)
{
    if (context == NULL)
        return;
    int cols = CPXgetnumcols(inst->env, inst->lp);
    double *x_heur = (double *)calloc(cols, sizeof(double));

    for (int i = 0; i < inst->nnodes; i++)
    {
        x_heur[xpos(inst, i, succ[i])] = 1.0;
    }

    CPXcallbackpostheursoln(context, cols, NULL, x_heur, obj, CPXCALLBACKSOLUTION_CHECKFEAS);
    free(x_heur);
}

/**
 * @brief Unified separation logic used by the Callback to handle integer candidates.
 *
 * Converts a candidate CPLEX solution into an adjacency graph, identifies subtours,
 * and triggers either the generation of SECs (if subtours exist) or validates the solution.
 *
 * @param inst Pointer to the problem instance.
 * @param xstar The candidate solution values for the binary variables provided by CPLEX.
 * @param context CPLEX callback context.
 * @param ws Thread-local workspace containing pre-allocated arrays for graph traversal.
 * @return The number of components found in the current candidate solution.
 */
int separate_integer_solution(instance *inst, double *xstar, CPXCALLBACKCONTEXTptr context, separationThreadWorkspace *ws)
{
    for (int i = 0; i < inst->nnodes; i++)
    {
        ws->adj[i][0] = -1;
        ws->adj[i][1] = -1;
        ws->visited[i] = 0;
    }

    for (int i = 0; i < inst->nnodes; i++)
    {
        for (int j = i + 1; j < inst->nnodes; j++)
        {
            if (xstar[xpos(inst, i, j)] > 0.5)
            {
                if (ws->adj[i][0] == -1)
                    ws->adj[i][0] = j;
                else
                    ws->adj[i][1] = j;
                if (ws->adj[j][0] == -1)
                    ws->adj[j][0] = i;
                else
                    ws->adj[j][1] = i;
            }
        }
    }

    for (int i = 0; i < inst->nnodes; i++)
    {
        if (ws->visited[i])
            continue;
        int curr = i;
        int next = ws->adj[curr][0];
        while (!ws->visited[curr])
        {
            ws->visited[curr] = 1;
            ws->succ[curr] = next;
            int prev = curr;
            curr = next;
            next = (ws->adj[curr][0] == prev) ? ws->adj[curr][1] : ws->adj[curr][0];
        }
    }

    int n_comp = find_components(inst, ws->succ, ws->comp);
    if (n_comp > 1)
    {
        add_SECs(inst, n_comp, ws->comp, context);
        patching_heuristic(inst, ws->succ, ws->comp, n_comp);
        double obj_heur = 0.0;
        for (int i = 0; i < inst->nnodes; i++)
            obj_heur += dist(i, ws->succ[i], inst);

        post_heuristic_solution(inst, context, ws->succ, obj_heur);
    }
    return n_comp;
}

/**
 * @brief The CPLEX generic callback driver function.
 *
 * Intercepts events during the CPLEX Branch & Cut process to allocate thread-local
 * memory, examine candidate solutions, and enforce subtour elimination.
 *
 * @param context CPLEX callback context.
 * @param contextid The specific event triggering the callback (e.g., THREAD_UP, CANDIDATE).
 * @param userhandle User data pointer containing the problem instance.
 * @return 0 on success.
 */
static int CPXPUBLIC callback_driver(CPXCALLBACKCONTEXTptr context, CPXLONG contextid, void *userhandle)
{
    instance *inst = (instance *)userhandle;
    int threadid;

    if (contextid == CPX_CALLBACKCONTEXT_THREAD_UP ||
        contextid == CPX_CALLBACKCONTEXT_THREAD_DOWN ||
        contextid == CPX_CALLBACKCONTEXT_CANDIDATE)
    {

        if (CPXcallbackgetinfoint(context, CPXCALLBACKINFO_THREADID, &threadid) != 0)
            return 0;
    }
    else
    {
        return 0;
    }

    if (contextid == CPX_CALLBACKCONTEXT_THREAD_UP)
    {
        separationThreadWorkspace *ws = (separationThreadWorkspace *)malloc(sizeof(separationThreadWorkspace));
        ws->succ = (int *)malloc(inst->nnodes * sizeof(int));
        ws->comp = (int *)malloc(inst->nnodes * sizeof(int));
        ws->visited = (int *)malloc(inst->nnodes * sizeof(int));
        ws->adj = (int **)malloc(inst->nnodes * sizeof(int *));
        for (int i = 0; i < inst->nnodes; i++)
            ws->adj[i] = (int *)malloc(2 * sizeof(int));
        inst->thread_workspaces[threadid] = ws;
        return 0;
    }

    if (contextid == CPX_CALLBACKCONTEXT_THREAD_DOWN)
    {
        separationThreadWorkspace *ws = inst->thread_workspaces[threadid];
        if (ws != NULL)
        {
            for (int i = 0; i < inst->nnodes; i++)
                free(ws->adj[i]);
            free(ws->adj);
            free(ws->visited);
            free(ws->comp);
            free(ws->succ);
            free(ws);
            inst->thread_workspaces[threadid] = NULL;
        }
        return 0;
    }

    if (contextid == CPX_CALLBACKCONTEXT_CANDIDATE)
    {
        separationThreadWorkspace *ws = inst->thread_workspaces[threadid];
        int cols = CPXgetnumcols(inst->env, inst->lp);
        double *xstar = (double *)malloc(cols * sizeof(double));
        CPXcallbackgetcandidatepoint(context, xstar, 0, cols - 1, NULL);

        // CORRECTED: Pass exactly 4 arguments
        separate_integer_solution(inst, xstar, context, ws);

        free(xstar);
        return 0;
    }
    return 0;
}

/**
 * @brief Main driver to formulate and solve the TSP to optimality using CPLEX.
 *
 * This function controls the entire optimization pipeline:
 * 1. Initializes the model and constraints.
 * 2. Attaches the Branch & Cut callback for dynamically adding SECs.
 * 3. Starts the optimization engine.
 * 4. Extracts and stores the globally optimal sequence upon completion.
 *
 * @param inst Pointer to the problem instance.
 */
void solve_with_cplex(instance *inst)
{
    init_cplex(inst);
    build_model(inst, inst->env, inst->lp);

    // Dynamic Branch & Cut (Modern Flow)
    CPXLONG contextmask = CPX_CALLBACKCONTEXT_THREAD_UP |
                          CPX_CALLBACKCONTEXT_THREAD_DOWN |
                          CPX_CALLBACKCONTEXT_CANDIDATE;
    CPXcallbacksetfunc(inst->env, inst->lp, contextmask, callback_driver, inst);

    CPXmipopt(inst->env, inst->lp);

    int sol_stat = CPXgetstat(inst->env, inst->lp);
    if (sol_stat == CPXMIP_OPTIMAL || sol_stat == CPXMIP_OPTIMAL_TOL || sol_stat == CPXMIP_TIME_LIM_FEAS)
    {
        printf("DEBUG: Entering solution retrieval with status %d\n", sol_stat);
        double final_cost;
        CPXgetobjval(inst->env, inst->lp, &final_cost);

        int cols = CPXgetnumcols(inst->env, inst->lp);
        double *xstar = (double *)malloc(cols * sizeof(double));
        CPXgetx(inst->env, inst->lp, xstar, 0, cols - 1);

        separationThreadWorkspace final_ws;
        final_ws.succ = (int *)malloc(inst->nnodes * sizeof(int));
        final_ws.comp = (int *)malloc(inst->nnodes * sizeof(int));
        final_ws.visited = (int *)calloc(inst->nnodes, sizeof(int));
        final_ws.adj = (int **)malloc(inst->nnodes * sizeof(int *));
        for (int i = 0; i < inst->nnodes; i++)
            final_ws.adj[i] = (int *)malloc(2 * sizeof(int));

        separate_integer_solution(inst, xstar, NULL, &final_ws);

        solution sol;
        sol.cost = final_cost;
        sol.tour = (int *)malloc(inst->nnodes * sizeof(int));
        int curr = 0;
        for (int i = 0; i < inst->nnodes; i++)
        {
            sol.tour[i] = curr;
            curr = final_ws.succ[curr];
        }

        update_best_solution(inst, &sol);

        free(xstar);
        free(sol.tour);
        free(final_ws.succ);
        free(final_ws.comp);
        free(final_ws.visited);
        for (int i = 0; i < inst->nnodes; i++)
            free(final_ws.adj[i]);
        free(final_ws.adj);
    }
    else
    {
        printf("DEBUG: CPLEX finished with status %d - Solution not retrieved.\n", sol_stat);
    }

    CPXfreeprob(inst->env, &inst->lp);
    CPXcloseCPLEX(&inst->env);
}