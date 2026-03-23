# 🔄 2-Opt Local Search: Implementation & Journey

This document details the implementation of the 2-opt local search heuristic in our TSP solver. It covers the final architecture as well as the step-by-step evolution from the initial concept to the robust final version.

---

## 1. The Final Implementation

The 2-opt algorithm improves an existing TSP tour by iteratively removing two non-adjacent edges and reconnecting the two resulting paths to form a shorter valid tour.

### Core Components

1.  **`find_best_two_opt`** (`src/common/2_opt.c`)
    *   **Purpose:** Evaluates all possible 2-opt swaps in the current tour and identifies the single move that yields the greatest cost reduction.
    *   **Logic:** Iterates through node indices `i` and `j` (where `j \ge i+2` to avoid adjacent edges). It safely calculates the "next" nodes using modulo arithmetic (`% inst->nnodes`) to handle the circular wrap-around of the tour.
    *   **Return:** Returns the `best_cost_diff` (a negative `double` if an improvement is found) and updates the best indices `pa` and `pb` via pointers.

2.  **`apply_two_opt`** (`src/common/2_opt.c`)
    *   **Purpose:** Physically applies the chosen 2-opt move to the tour array.
    *   **Logic:** Reverses the segment of the array strictly starting at `pa + 1` and ending at `pb`. This in-place reversal effectively breaks the old edges and reconnects the path correctly.

3.  **Integration (`main.c`)**
    *   **Execution:** Applied conditionally via the `-2opt` command-line flag.
    *   **Convergence:** Wrapped in a `while(1)` loop that continuously calls `find_best_two_opt` and `apply_two_opt` until the returned improvement is negligible (`\ge -1e-5`).
    *   **Safety:** Recalculates the total tour cost from scratch after the 2-opt phase finishes to eliminate floating-point drift.

---

## 2. The Development Journey

The implementation evolved significantly through debugging and mathematical refinement. Here are the key milestones of that journey:

### Phase 1: The Distance Function Dilemma (`dist` vs `dist_sq`)
*   **The Concept:** Initially, we wanted to use `dist_sq` (squared distance) to calculate the cost difference, just as we did in the Greedy NN inner loop, to save CPU cycles.
*   **The Realization:** We discovered a mathematical limitation: While $A < B \iff A^2 < B^2$, the same is **not true for sums**. $(A^2 + B^2) < (C^2 + D^2)$ does *not* guarantee $(A + B) < (C + D)$. 
*   **The Fix:** We had to use the actual `dist` function (with `sqrt`) inside the 2-opt evaluation to ensure the geometric length truly decreased.

### Phase 2: Circular Array Boundaries
*   **The Concept:** The tour is stored as a linear array, but mathematically represents a continuous cycle.
*   **The Realization:** When `j` reaches the end of the array (`nnodes - 1`), `j + 1` goes out of bounds. 
*   **The Fix:** We implemented modulo arithmetic `(j + 1) % inst->nnodes` to safely wrap the logic around to the start of the array, treating the tour as a true cycle.

### Phase 3: Pointers over Returns
*   **The Concept:** We needed to extract three pieces of data from the search function: the cost difference, and the two indices (`pa`, `pb`).
*   **The Fix:** We structured `find_best_two_opt` to return the `double` cost difference natively, while updating `pa` and `pb` via memory pointers passed by reference from `main`.

### Phase 4: The Infinite Loop Bug (Array Reversal)
*   **The Concept:** To apply the swap, we needed to swap nodes.
*   **The Bug:** Initially, the array reversal started at `pa` and tried to use modulo arithmetic moving backward. This accidentally broke the edge *before* `pa` instead of the edge *after* `pa`. The array became geometrically scrambled, leading the algorithm to find massive "improvements" to fix its own mistakes, trapping it in an infinite loop.
*   **The Fix:** We aligned the physical array reversal with the mathematical model. The reversal must start strictly at `pa + 1` and end at `pb`. Because `pb > pa` in our loops, modulo was removed entirely in favor of simple `start++` and `end--` pointers.

### Phase 5: Taming Floating-Point Drift
*   **The Concept:** In `main.c`, we updated the tour's total cost by simply adding the negative `best_cost_diff` to `current_sol.cost`.
*   **The Bug:** After hundreds of swaps, microscopic precision errors (`0.000000001`) accumulated. When identical tours were generated from different starting nodes, their accumulated costs slightly differed, causing the system to continuously log "New Best" and plot identical images.
*   **The Fix:** We instituted a two-part safeguard:
    1.  **Tolerance:** Changed the `while` loop exit condition to `best_cost_diff >= -1e-5` to ignore microscopic phantom improvements.
    2.  **Hard Recalculation:** After the `while(1)` loop concludes, the system calls `calculate_cost()` to sweep the final array and compute the exact geometric cost from scratch, destroying any accumulated drift before comparing it to the global best.