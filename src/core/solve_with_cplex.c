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
    if (VERBOSE >= 3)
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
    if (VERBOSE >= 3)
        printf("DEBUG: Environment created successfully.\n");

    CPXsetintparam(inst->env, CPX_PARAM_RANDOMSEED, 1234);
    CPXsetintparam(inst->env, CPX_PARAM_MIPDISPLAY, 2);

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
    if (VERBOSE >= 3)
        printf("DEBUG: Problem container 'TSP_Model' created.\n");

    // 3. Enable screen indicator so CPLEX prints its progress to the terminal
    CPXsetintparam(inst->env, CPX_PARAM_SCRIND, CPX_ON);
    if (VERBOSE >= 3)
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
void add_SECs(instance *inst, int n_comp, int *comp, CPXCALLBACKCONTEXTptr context, bool is_fractional)
{
    int *comp_size = (int *)calloc(n_comp + 1, sizeof(int));
    int *node_list = (int *)malloc(inst->nnodes * sizeof(int));
    int *comp_start = (int *)malloc((n_comp + 2) * sizeof(int));

    // This organizes nodes so the solver can quickly identify which nodes belong to which subtour.
    // Counts how many nodes are in each component
    for (int i = 0; i < inst->nnodes; i++)
        comp_size[comp[i]]++;

    // Calculates where each component's list of nodes will start in a "flattened" array
    comp_start[1] = 0;
    for (int k = 1; k <= n_comp; k++)
        comp_start[k + 1] = comp_start[k] + comp_size[k];

    // Arranges nodes in the flattened array so that all nodes from Component 1 come first, followed by all nodes from Component 2, and so on
    int *temp_count = (int *)calloc(n_comp + 1, sizeof(int));
    for (int i = 0; i < inst->nnodes; i++)
    {
        int c = comp[i];
        node_list[comp_start[c] + temp_count[c]] = i;
        temp_count[c]++;
    }
    //

    // Maximum possible memory needed for the constraints.
    // Why (n/2)?: If you break a subtour of size k, you automatically break the remaining subtour of size n-k.
    int max_n_s = (inst->nnodes / 2) + 1;
    int max_edges = (max_n_s * (max_n_s - 1)) / 2;
    int *index = (int *)malloc(max_edges * sizeof(int));
    double *value = (double *)malloc(max_edges * sizeof(double));

    for (int k = 1; k <= n_comp; k++)
    {
        int nodes_in_subtour = comp_size[k];
        if (nodes_in_subtour <= 1 || nodes_in_subtour * 2 >= inst->nnodes)
            continue;
        int nnz = 0;
        int start_idx = comp_start[k];
        int end_idx = comp_start[k + 1];

        // For every node i and node j in the same component k, it finds the index of the edge variable x_{ij} using xpos.
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

        double rhs = (double)nodes_in_subtour - 1.0; // It sets the Right-Hand Side (rhs) to $NodesInSubtour - 1.
        char sense = 'L';                            // 'L' stands for Less-than-or-equal (<=)
        int izero = 0;

        if (context == NULL)
        {
            // If context is NULL, it adds the constraint directly to the global problem.
            CPXaddrows(inst->env, inst->lp, 0, 1, nnz, &rhs, &sense, &izero, index, value, NULL, NULL);
        }
        else if (is_fractional)
        {
            // CPLEX needs pointers to these values, not the values themselves
            int purgeable = CPX_USECUT_FILTER;
            int local = 0; // 0 means the cut is globally valid

            CPXcallbackaddusercuts(context, 1, nnz, &rhs, &sense, &izero, index, value, &purgeable, &local);
        }
        else
        {
            // If context is provided(during a callback), it uses CPXcallbackrejectcandidate
            // This tells CPLEX: "The integer solution you just found is illegal. Here is a constraint to make sure you never pick this specific subtour again".
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

    // Determine the number of columns (variables) in the model
    int cols = CPXgetnumcols(inst->env, inst->lp);

    // Allocate memory for variable indices and their values
    int *indices = (int *)malloc(cols * sizeof(int));
    double *x_heur = (double *)calloc(cols, sizeof(double));

    // Safety check for memory allocation
    if (indices == NULL || x_heur == NULL)
    {
        print_error("Memory allocation failed in post_heuristic_solution");
    }

    /**
     * CPLEX generic callback requires the index array to be non-NULL
     * if the count (cols) is greater than zero. We fill it with 0...cols-1.
     */
    for (int i = 0; i < cols; i++)
    {
        indices[i] = i;
    }

    // Set variable values to 1.0 for edges in the heuristic tour
    for (int i = 0; i < inst->nnodes; i++)
    {
        x_heur[xpos(inst, i, succ[i])] = 1.0;
    }

    /**
     * Call CPLEX to post the integer solution.
     */
    if (CPXcallbackpostheursoln(context, cols, indices, x_heur, obj, CPXCALLBACKSOLUTION_CHECKFEAS) != 0)
        if (VERBOSE >= 3)
            printf("WARNING: CPXcallbackpostheursoln failed to post heuristic solution\n");

    // Clean up temporary memory
    free(indices);
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

    // Building the Adjacency List translating from xstar
    for (int i = 0; i < inst->nnodes; i++)
    {
        ws->adj_count[i] = 0; // was: ws->adj[i][0] = -1; ws->adj[i][1] = -1;
        ws->visited[i] = 0;
    }

    // When building the adjacency list
    for (int i = 0; i < inst->nnodes; i++)
    {
        for (int j = i + 1; j < inst->nnodes; j++)
        {
            if (xstar[xpos(inst, i, j)] > 0.5)
            {
                if (ws->adj_count[i] >= ws->adj_capacity || ws->adj_count[j] >= ws->adj_capacity)
                    print_error("Adjacency capacity exceeded — node degree > adj_capacity");

                ws->adj[i][ws->adj_count[i]++] = j;
                ws->adj[j][ws->adj_count[j]++] = i;
            }
        }
    }

    // Converts the undirected adjacency list into a directed successor array
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

    // Block that handles Subtour Elimination.
    int n_comp = find_components(inst, ws->succ, ws->comp);
    if (n_comp > 1)
    {
        add_SECs(inst, n_comp, ws->comp, context, false);
        patching_heuristic(inst, ws->succ, ws->comp, n_comp);
        double obj_heur = 0.0;
        for (int i = 0; i < inst->nnodes; i++)
            obj_heur += dist(i, ws->succ[i], inst);

        post_heuristic_solution(inst, context, ws->succ, obj_heur);
    }
    return n_comp;
}

int separate_fractional_solution(instance *inst, double *xstar, CPXCALLBACKCONTEXTptr context, separationThreadWorkspace *ws)
{
    int n_comp = 0;
    for (int i = 0; i < inst->nnodes; i++)
        ws->comp[i] = -1;

    // 1. BFS Connectivity Check (Fast exit)
    for (int i = 0; i < inst->nnodes; i++)
    {
        if (ws->comp[i] != -1)
            continue;
        n_comp++;
        int head = 0, tail = 0;
        ws->comp[i] = n_comp;
        ws->queue[tail++] = i;
        while (head < tail)
        {
            int curr = ws->queue[head++];
            for (int next = 0; next < inst->nnodes; next++)
            {
                if (curr == next)
                    continue;
                if (xstar[xpos(inst, curr, next)] > 1e-6 && ws->comp[next] == -1)
                {
                    ws->comp[next] = n_comp;
                    ws->queue[tail++] = next;
                }
            }
        }
    }

    if (n_comp > 1)
    {
        if (VERBOSE >= 3)
        {
            printf("\n [BFS] Disconnected fractional components found: %d. Adding SECs...\n", n_comp);
        }
        add_SECs(inst, n_comp, ws->comp, context, true);
        return n_comp;
    }

    if (VERBOSE >= 3)
    {
        printf(" [BFS] Graph is connected. Triggering Stoer-Wagner Min-Cut...\n");
    }

    // 2. Stoer-Wagner (Min-Cut for Connected Graphs)
    // --- ALLOCATION STEP ---
    // NOTE: Each partition is allocated with inst->nnodes slots (O(n²) total memory).
    // In practice partitions shrink as nodes are contracted, so most slots are unused.
    // For large instances (n > ~1000) this becomes a bottleneck and a flat buffer
    // with dynamic resizing would be more appropriate.
    int **partitions = (int **)malloc(inst->nnodes * sizeof(int *));
    int *partition_size = (int *)malloc(inst->nnodes * sizeof(int));
    double **weights = (double **)malloc(inst->nnodes * sizeof(double *));
    int *active_nodes = (int *)malloc(inst->nnodes * sizeof(int));

    // Build the skeleton first to prevent EXC_BAD_ACCESS
    for (int i = 0; i < inst->nnodes; i++)
    {
        partitions[i] = (int *)malloc(inst->nnodes * sizeof(int));
        weights[i] = (double *)calloc(inst->nnodes, sizeof(double));
    }

    // --- STEP 1: POPULATION & INITIALIZATION ---
    // Initialize the weighted adjacency matrix and partition trackers for the algorithm.
    for (int i = 0; i < inst->nnodes; i++)
    {
        // Each node starts as its own partition (a set containing only itself).
        partitions[i][0] = i;
        partition_size[i] = 1;
        active_nodes[i] = i;

        // Populate weights using the fractional values (xstar) from the current relaxation.
        for (int j = i + 1; j < inst->nnodes; j++)
        {
            double val = xstar[xpos(inst, i, j)];
            if (val > 1e-6) // Only track edges with significant flow.
            {
                weights[i][j] = val;
                weights[j][i] = val; // Maintain symmetry in the weighted graph.
            }
        }
    }

    // --- STEP 2: STOER-WAGNER MIN-CUT PHASES ---
    // The algorithm runs N-1 phases. Each phase finds a "cut-of-the-phase" and then contracts two nodes.
    int current_nnodes = inst->nnodes;
    while (current_nnodes > 1)
    {
        // Tracks the sum of weights from the set 'A' to nodes not yet in 'A'.
        double *w_to_A = (double *)calloc(current_nnodes, sizeof(double));
        int *in_A = (int *)calloc(current_nnodes, sizeof(int));
        int prev = 0, curr = 0;

        // Min-Cut Phase: Add nodes one by one to set 'A' based on maximum connectivity.
        for (int i = 0; i < current_nnodes; i++)
        {
            int next = -1;
            for (int j = 0; j < current_nnodes; j++)
            {
                // Select the node not in 'A' that is most tightly connected to 'A'.
                if (!in_A[j] && (next == -1 || w_to_A[j] > w_to_A[next]))
                    next = j;
            }
            in_A[next] = 1;

            // Check if we have reached the end of the phase (the last node added).
            if (i == current_nnodes - 1)
            {
                // The weight of the last node added is the weight of the "cut-of-the-phase".
                double cut_weight = w_to_A[next];

                // --- VIOLATION DETECTION ---
                // If the min-cut is < 2.0, we have found a violated Subtour Elimination Constraint.
                if (cut_weight < 2.0 - 1e-6)
                {
                    // Label nodes: Component 1 is the set we just cut off, Component 2 is everything else.
                    for (int k = 0; k < inst->nnodes; k++)
                        ws->comp[k] = 2;
                    for (int k = 0; k < partition_size[next]; k++)
                        ws->comp[partitions[next][k]] = 1;

                    // Add the cut to CPLEX as a User Cut or Lazy Constraint.
                    add_SECs(inst, 2, ws->comp, context, true);

                    free(w_to_A);
                    free(in_A);
                    goto cleanup; // Exit separation early once a violation is found.
                }

                // --- NODE CONTRACTION ---
                // Merge the last node (t) into the second-to-last node (s).
                int s = prev, t = next;
                for (int j = 0; j < current_nnodes; j++)
                {
                    if (j != s && j != t)
                    {
                        // Add weights of 't' to 's' for all other active nodes.
                        weights[active_nodes[s]][active_nodes[j]] += weights[active_nodes[t]][active_nodes[j]];
                        weights[active_nodes[j]][active_nodes[s]] = weights[active_nodes[s]][active_nodes[j]];
                    }
                }

                // Merge the node list of 't' into the node list of 's'.
                for (int k = 0; k < partition_size[t]; k++)
                    partitions[s][partition_size[s]++] = partitions[t][k];

                // Deactivate node 't' by swapping it with the last node in the current active set.
                active_nodes[t] = active_nodes[current_nnodes - 1];
                partition_size[t] = partition_size[current_nnodes - 1];
                int *temp_ptr = partitions[t];
                partitions[t] = partitions[current_nnodes - 1];
                partitions[current_nnodes - 1] = temp_ptr;
            }
            else
            {
                // Update 'w_to_A' values for the next iteration of the phase.
                prev = curr;
                curr = next;
                for (int j = 0; j < current_nnodes; j++)
                {
                    if (!in_A[j])
                        w_to_A[j] += weights[active_nodes[next]][active_nodes[j]];
                }
            }
        }
        current_nnodes--; // The graph is now one node smaller.
        free(w_to_A);
        free(in_A);
    }

// --- STEP 3: CLEANUP ---
// Ensures all local memory for the Stoer-Wagner matrix and trackers is freed.
cleanup:
    for (int i = 0; i < inst->nnodes; i++)
    {
        free(weights[i]);
        free(partitions[i]);
    }
    free(weights);
    free(partitions);
    free(partition_size);
    free(active_nodes);
    return 1;
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
int CPXPUBLIC callback_driver(CPXCALLBACKCONTEXTptr context, CPXLONG contextid, void *userhandle)
{
    instance *inst = (instance *)userhandle;
    int threadid;

    if (contextid == CPX_CALLBACKCONTEXT_THREAD_UP ||
        contextid == CPX_CALLBACKCONTEXT_THREAD_DOWN ||
        contextid == CPX_CALLBACKCONTEXT_CANDIDATE ||
        contextid == CPX_CALLBACKCONTEXT_RELAXATION)
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
        ws->adj_count = (int *)calloc(inst->nnodes, sizeof(int));
        ws->queue = (int *)malloc(inst->nnodes * sizeof(int));
        ws->adj_capacity = 4;

        ws->adj = (int **)malloc(inst->nnodes * sizeof(int *));
        for (int i = 0; i < inst->nnodes; i++)
            ws->adj[i] = (int *)malloc(ws->adj_capacity * sizeof(int));

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
            free(ws->adj_count);
            free(ws->queue);
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

        separate_integer_solution(inst, xstar, context, ws);

        free(xstar);
        return 0;
    }
    if (contextid == CPX_CALLBACKCONTEXT_RELAXATION)
    {
        CPXLONG node_count;
        CPXcallbackgetinfolong(context, CPXCALLBACKINFO_NODECOUNT, &node_count);

        // Skip all work if we aren't at the root or our 50-node interval
        if (node_count > 0 && node_count % 50 != 0)
            return 0;

        // Now that we know we are going to run separation, do the expensive stuff
        separationThreadWorkspace *ws = inst->thread_workspaces[threadid];
        int cols = CPXgetnumcols(inst->env, inst->lp);
        double *xstar = (double *)malloc(cols * sizeof(double));

        if (CPXcallbackgetrelaxationpoint(context, xstar, 0, cols - 1, NULL) == 0)
        {
            separate_fractional_solution(inst, xstar, context, ws);
        }

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

    // Dynamic Branch & Cut
    CPXLONG contextmask = CPX_CALLBACKCONTEXT_THREAD_UP |
                          CPX_CALLBACKCONTEXT_THREAD_DOWN |
                          CPX_CALLBACKCONTEXT_CANDIDATE |
                          CPX_CALLBACKCONTEXT_RELAXATION;
    CPXcallbacksetfunc(inst->env, inst->lp, contextmask, callback_driver, inst);

    CPXmipopt(inst->env, inst->lp);

    int sol_stat = CPXgetstat(inst->env, inst->lp);
    if (sol_stat == CPXMIP_OPTIMAL || sol_stat == CPXMIP_OPTIMAL_TOL || sol_stat == CPXMIP_TIME_LIM_FEAS)
    {
        if (VERBOSE >= 3)
            printf("DEBUG: Entering solution retrieval with status %d\n", sol_stat);
        double final_cost;
        if (CPXgetobjval(inst->env, inst->lp, &final_cost))
            print_error("CPXgetobjval failed");

        int cols = CPXgetnumcols(inst->env, inst->lp);
        double *xstar = (double *)malloc(cols * sizeof(double));
        if (CPXgetx(inst->env, inst->lp, xstar, 0, cols - 1))
            print_error("CPXgetx failed");

        separationThreadWorkspace final_ws;
        final_ws.adj_capacity = 4;
        final_ws.succ = (int *)malloc(inst->nnodes * sizeof(int));
        final_ws.comp = (int *)malloc(inst->nnodes * sizeof(int));
        final_ws.visited = (int *)calloc(inst->nnodes, sizeof(int));
        final_ws.adj_count = (int *)calloc(inst->nnodes, sizeof(int));
        final_ws.adj = (int **)malloc(inst->nnodes * sizeof(int *));
        for (int i = 0; i < inst->nnodes; i++)
            final_ws.adj[i] = (int *)malloc(final_ws.adj_capacity * sizeof(int));

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

        if (sol.cost < inst->best_solution.cost - EPSILON)
            update_best_solution(inst, &sol);

        free(xstar);
        free(sol.tour);
        free(final_ws.succ);
        free(final_ws.comp);
        free(final_ws.visited);
        for (int i = 0; i < inst->nnodes; i++)
            free(final_ws.adj[i]);
        free(final_ws.adj);
        free(final_ws.adj_count);
    }
    else
    {
        if (VERBOSE >= 3)
            printf("DEBUG: CPLEX finished with status %d - Solution not retrieved.\n", sol_stat);
    }

    CPXfreeprob(inst->env, &inst->lp);
    CPXcloseCPLEX(&inst->env);
}