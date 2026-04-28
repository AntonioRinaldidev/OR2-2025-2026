# CPLEX Solver Integration Documentation (Dynamic Branch & Cut)

## 1. Initialization and Callback Registration
The solver is initialized using standard CPLEX environment (`env`) and problem (`lp`) pointers. Unlike static models, this implementation uses a **Generic Callback** to handle subtours dynamically during the search.

### Implementation Details
- **Context Mask**: Listens for `THREAD_UP`, `THREAD_DOWN`, and `CANDIDATE` events.
- **Registration**: Connected via `CPXcallbacksetfunc`.
- **Optimization**: Performed in a single call to `CPXmipopt`, allowing CPLEX to maintain its internal search tree.



---

## 2. Multi-Threaded Resource Management
Since the solver runs on 14+ threads, the system utilizes **Thread-Local Workspaces** to prevent memory corruption.

### The `separationThreadWorkspace`
Each thread is assigned a private structure containing:
- `adj`: Adjacency list for the current candidate.
- `succ` / `comp`: Successor and component vectors for cycle detection.
- `visited`: Helper array for graph traversal.

### Lifecycle Management
- **`THREAD_UP`**: Allocates the workspace and stores the pointer in `inst->thread_workspaces[threadid]`.
- **`THREAD_DOWN`**: Frees the thread-specific memory once the search branch terminates.



---

## 3. Dynamic Subtour Elimination
Instead of waiting for the solver to finish, subtours are identified and rejected "on the fly."

### Integer Separation (`CANDIDATE` Context)
When CPLEX finds a potential solution, the `callback_driver` triggers:
1. **Adjacency Mapping**: Translates the 1D variable array into a thread-private graph.
2. **Component Identification**: Uses `find_components` to detect subtours.
3. **Rejection**: If `n_comp > 1`, the candidate is rejected using `CPXcallbackrejectcandidate`.
4. **SEC Generation**: Violated Subtour Elimination Constraints are added to the search tree.

---

## 4. Heuristic Integration (Incumbent Posting)
To accelerate the search, the solver doesn't just reject bad solutions; it actively improves them.

### Mechanism
- **Patching**: When subtours are found, the `patching_heuristic` merges them into a valid Hamiltonian cycle.
- **Posting**: The resulting tour is "posted" back to CPLEX using `CPXcallbackpostheursoln` with the `CPXCALLBACKSOLUTION_CHECKFEAS` flag.
- **Impact**: This provides CPLEX with high-quality incumbents early, allowing for aggressive pruning of the Branch & Bound tree.



---

## 5. Robust Solution Retrieval
The implementation accounts for various solver termination states to ensure the best solution is always saved.

- **Status Handling**: The retrieval logic checks for `CPXMIP_OPTIMAL`, `CPXMIP_TIME_LIM_FEAS`, and **`CPXMIP_OPTIMAL_TOL` (Status 102)**.
- **Tour Reconstruction**: The final $x_{ij}$ values are passed through a final separation pass to build the successor vector for plotting.
- **Data Integrity**: `update_best_solution` is called to overwrite the initial `INF` cost with the final validated cost.

---

## Current Project Status
- [x] **Generic Callback Driver**: Refactored from static loop to modern `CPX_CALLBACKCONTEXT` logic.
- [x] **Parallel Safety**: Thread-local workspaces implemented for 14+ threads.
- [x] **Incumbent Posting**: Patching heuristic results are now shared with the CPLEX engine.
- [x] **Status 102 Fix**: Corrected retrieval logic to prevent $10^{30}$ (INF) cost errors.

## Next Milestones (Roadmap)
- [ ] **Phase 1: Fractional Separation**: Implement `RELAXATION` context to add User Cuts.
- [ ] **Phase 2: Min-Cut Algorithm**: Utilize Max-Flow/Min-Cut (e.g., Edmonds-Karp) for fractional SECs.
- [ ] **Phase 3: Distance Matrix Optimization**: Pre-calculate distances to reduce CPU overhead across parallel threads.