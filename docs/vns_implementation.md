# 🚀 Variable Neighborhood Search (VNS) - Implementation

This document tracks step-by-step the development and the reasoning behind the implementation of the "Kick" for the VNS algorithm applied to the TSP.

## Phase 1: Choosing the nature of the Kick and the cut points

**Objective:** Define what it mathematically means to apply a "kick" of size `3` to the tourand correctly generate the points of the tour to operate on.

### Discussed Options:

1.  **Option A: Pure 3-opt / Segment Shuffle (The "Necklace Cut")**
    *   **Idea:** Choose `3` random and unique indices. These indices represent the points where the tour is broken, creating `3` separate segments. The segments are then glued back together in a shuffled order.
    *   **Pros:** Generates a much more "disruptive" structural change that is impossible to reverse with a simple 2-opt. It is the most robust method to escape deep local optima.
    *   **Cons:** Requires managing the extraction of unique numbers, sorting them, and carefully handling the movement of memory blocks using a temporary array.

**Decision and Implementation:**
 **The 3-opt Kick**.
*   **Reasoning:** A 3-opt move is the perfect middle ground. It breaks exactly 3 edges, creating 3 segments (A, B, C). By reconnecting them in the order `A -> C -> B`, we create a structural change that a standard 2-opt local search cannot immediately reverse. It provides   excellent disruption without the extreme complexity of a generalized $n$-opt.
*   **Requirement for Phase 1:** We need to select exactly 3 random, unique indices ($i, j, k$) in the range `[0, nnodes - 1]` and sort them such that $i < j < k$.
*   **Why sort them?** The tour is mathematically a cycle, meaning segments *do* wrap around the array ends. However, because C arrays are linear in memory (from `0` to `N-1`), sorting the random cuts (e.g., converting `8, 2, 5` into `2, 5, 8`) breaks the exact same 3 edges but makes copying the memory blocks much easier. It avoids complex modulo arithmetic during the array reconstruction.
*   **Edges vs. Indices:** Mathematically, breaking 3 edges affects 6 nodes (3 pairs). However, programmatically, because the tour is an ordered sequence in an array, an edge is completely defined by its starting index. Therefore, we only need to randomly generate **3 indices** ($i, j, k$). These implicitly identify the 3 edges to break: $(i, i+1)$, $(j, j+1)$, and $(k, k+1)$.

---

## Phase 2: Reconnecting the Segments (The Block Swap)

**Objective:** Define the new sequence of the nodes to form a valid, perturbed tour.

**Why this specific reconnection?**
When breaking 3 edges, there are 7 valid ways to reconnect the tour. 6 of those ways involve *reversing* the order of nodes in at least one of the segments. Since a standard 2-opt local search also works by reversing segments, a reversing 3-opt kick can often be quickly "undone" by the local search. 
The reconnection we chose is the **only pure non-reversing 3-opt move**. It acts as a pure "block swap", making it highly disruptive and very difficult for a 2-opt to undo.

**Mathematical Reconnection:**
We break the 3 edges and reconnect them as follows: $i \to j+1$, $k \to i+1$, and $j \to k+1$.

**Memory/Array Translation:**
Because our indices are sorted ($i < j < k$), this reconnection beautifully translates into simply swapping the middle two segments in the linear array. If the original array is divided into `[0..i]`, `[i+1..j]`, `[j+1..k]`, and `[k+1..N-1]`, the new array is constructed by placing the blocks in this order:
1.  `[0..i]`
2.  `[j+1..k]`
3.  `[i+1..j]`
4.  `[k+1..N-1]`

**The Step-by-Step Flow:**
To visualize exactly where every segment goes:
1. Travel `0` to `i`.
2. Jump from `i` $\to$ `j+1`.
3. Travel `j+1` to `k`.
4. Jump from `k` $\to$ `i+1`. *(This is where the `i+1..j` segment is inserted!)*
5. Travel `i+1` to `j`.
6. Jump from `j` $\to$ `k+1` and travel to the end.

---

## Phase 3: Array Mechanics (In-place vs Temporary)

**Objective:** Determine how to physically move the memory blocks in C without corrupting the data.

**The Problem with In-Place:**
Because the two middle segments (`[i+1..j]` and `[j+1..k]`) are almost always of different sizes, attempting to swap them directly inside the original `tour` array would cause data to be overwritten before it can be moved. 

**The Solution: Temporary Array**
We allocate a temporary array (`new_tour`). We read the blocks sequentially from the original `tour` and write them into `new_tour` in our newly defined order (Block 1, Block 3, Block 2, Block 4). Once the `new_tour` is fully assembled, we copy its contents back into the original `tour` and free the temporary memory. This guarantees memory safety and keeps the logic very simple.

**Copying Sequences vs Swapping Elements:**
A crucial conceptual distinction: we do not use element-by-element swaps (like `temp = a; a = b;`). Swapping elements would leave the nodes "in between" stuck in their original positions. 
Instead, we use loops to **copy entire sequences** of nodes from the read-only original array into the new array. By copying sequence $j+1 \dots k$ immediately followed by sequence $i+1 \dots j$, the entire blocks of nodes are physically relocated in memory as whole units.

---

## Phase 4: The Copying Mechanism

**Objective:** Choose the C programming construct to copy sequences into the new array.

**The Write-Head Index:**
To build the new array sequentially, we need a tracker variable (e.g., `int curr = 0;`) that always points to the next available empty slot in the temporary array. Every time we copy a node (or a block of nodes), we advance this index.

**Method 1: `for` loops vs Method 2: `memcpy`**
We can use simple `for` loops to iterate through the bounds of each block, assigning `new_tour[curr] = tour[x]` and incrementing `curr++`. Alternatively, we can use `memcpy(&new_tour[curr], &tour[start], size * sizeof(int))` to move whole blocks at once, updating `curr += size`. Both are valid, but `memcpy` is more concise and optimized in C.

---

## Phase 5: Randomizing the 3-opt Move (The "Swiss Army Knife")

**Objective:** Increase diversification to better escape local optima, especially on unstructured (random) datasets.

**The Topology of 3 Cuts:**
When making exactly 3 cuts, the wrap-around edge connecting the end of the array (Block D) to the beginning (Block A) remains intact. Therefore, Block A must always remain at the front, and Block D at the back. This leaves only the middle blocks (B and C) to be manipulated. 
There are exactly 7 valid ways to reconnect B and C without reverting to the original tour:
1.  `C -> B` (Pure Block Swap)
2.  `Reversed(B) -> Reversed(C)`
3.  `Reversed(B) -> C`
4.  `B -> Reversed(C)`
5.  `Reversed(C) -> B`
6.  `C -> Reversed(B)`
7.  `Reversed(C) -> Reversed(B)`

**Implementation:** We developed `apply_random_3_opt_kick` which generates a random integer from `0` to `6` and uses a `switch` statement to apply one of these 7 topological permutations. This statistical variety prevents the solver from getting stuck in cyclical loops when dealing with chaotic point clouds.

---

## Phase 6: VNS Loop Architecture

**Objective:** Integrate the Kick and Local Search into a continuous optimization cycle.

**The Meta-Heuristic Flow:**
The VNS loop operates on the principle of `Perturbation -> Local Search -> Acceptance Criterion`.
1.  **Perturbation:** A copy of the current best local minimum is made (`working_sol`), and a random 3-opt kick is applied.
2.  **Local Search:** The standard 2-opt algorithm is run on the kicked tour to let it settle into its *new* local minimum (valley).
3.  **Acceptance:** If the new valley has a strictly lower cost than the previous one, the new tour replaces the current best, and the `kicks_without_improvement` counter resets to `0`. If it is worse, the kicked tour is discarded, and the counter increments. The loop terminates when the counter hits `MAX_KICKS` (e.g., 5).
