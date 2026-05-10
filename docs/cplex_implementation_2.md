# Technical Specification: solve_with_cplex.c

## 1. Mathematical Formulation
The solver implements the **Dantzig-Fulkerson-Johnson (DFJ)** formulation of the TSP. 
- **Objective**: Minimize $\sum c_{ij} x_{ij}$
- **Degree Constraints**: $\sum_{j} x_{ij} = 2$ for all nodes $i$.[cite: 1]
- **Subtour Elimination**: $\sum_{i,j \in S} x_{ij} \le |S| - 1$ added dynamically.[cite: 1]

## 2. The Variable Mapping Engine (`xpos`)
To reduce the variable count by half, the implementation only considers the upper triangle of the adjacency matrix. 
- **Variable Count**: $\frac{n(n-1)}{2}$[cite: 1]
- **Logic**: All references to $(i, j)$ are normalized so that $i < j$ before calculating the 1D index.[cite: 1]

## 3. Dynamic Separation Ecosystem
The solver utilizes CPLEX **Generic Callbacks** to enforce feasibility across 14+ parallel threads.[cite: 1]

### A. Candidate Separation (Integer)
When a thread finds an integer solution ($x_{ij} \in \{0, 1\}$):
- It builds a successor array from the adjacency list.[cite: 1]
- It runs `find_components` to identify subtours.[cite: 1]
- If $n\_comp > 1$, it rejects the solution and injects SECs as **Lazy Constraints**.[cite: 1]
- It applies a **Patching Heuristic** to turn the subtours into a valid tour and "posts" it back to CPLEX as a heuristic incumbent.[cite: 1]

### B. Relaxation Separation (Fractional)
When a thread is at a node in the search tree with a fractional solution:
- **Connectivity Check**: A Breadth-First Search (BFS) is performed to find disconnected components ($0$-weight cuts).[cite: 1]
- **Min-Cut Analysis**: If connected, the **Stoer-Wagner algorithm** ($O(V^3)$) is triggered to find cuts with weight $< 2.0$.[cite: 1]
- **Throttling**: To maximize node-per-second throughput, fractional separation is limited to the **Root Node** and a periodic **50-node interval**.[cite: 1]

## 4. Resource & Memory Management
- **Thread-Safe Workspaces**: Each thread is allocated a `separationThreadWorkspace` during the `THREAD_UP` event.[cite: 1] This prevents data races on shared arrays like `succ`, `comp`, and `visited`.[cite: 1]
- **Matrix Skeletons**: 2D matrices used in Stoer-Wagner are allocated as "skeletons" (array of pointers) to ensure symmetric edge assignments (`weights[i][j] = weights[j][i]`) never access unallocated memory.[cite: 1]

## 5. Result Extraction & Status Handling
The solver retrieves the final solution if CPLEX returns:
- `CPXMIP_OPTIMAL` (Status 101)[cite: 1]
- `CPXMIP_OPTIMAL_TOL` (Status 102)[cite: 1]
- `CPXMIP_TIME_LIM_FEAS` (Status 107 - Feasible solution found before timeout)[cite: 1]

It reconstructs the final tour by running one final separation pass on the optimal $x^*$ values to build the successor vector for the `.tour` output.[cite: 1]