# 🛠️ TSP Solver Implementation Details

This document outlines the technical implementation of the Traveling Salesman Problem (TSP) solver developed for the Operations Research 2 course. The solution is written in **C** and focuses on memory safety, modularity, and computational efficiency.

## 1. Architecture Overview

The project is structured into three main components:

1.  **Core Logic (`src/vrp/main.c`)**: Orchestrates the program flow, handles command-line arguments, and manages the multi-start heuristic loop.
2.  **Heuristics (`src/greedyNN.c`)**: Contains the implementation of the Greedy Nearest Neighbor algorithm.
3.  **Utilities (`src/common/utilities.c`)**: Handles input parsing (TSPLIB format), memory management, distance calculations, and visualization.

---

## 2. Data Structures

### `instance`
The central structure holding all problem data.
- **Coordinates**: `vertices` array of `vertex` structures storing node positions.
- **Distance Matrix**: `dists`, a flattened 1D array representing the $N \times N$ matrix.
- **Parameters**: `timelimit`, `randomseed`, `num_threads`.

### `solution`
Represents a specific tour found by an algorithm.
- **`tour`**: An integer array of size $N$ representing the sequence of visited nodes.
- **`cost`**: The total Euclidean length of the tour.

---

## 3. Algorithms

### Multi-Start Greedy Nearest Neighbor (NN)

The solver currently employs a **Multi-Start Greedy Heuristic** to construct a feasible solution.

#### Logic
1.  **Initialization**: The algorithm selects a starting node $S$.
2.  **Construction**: From the current node, it identifies the nearest unvisited neighbor.
3.  **Update**: It moves to that neighbor and marks it as visited.
4.  **Termination**: This repeats until all nodes are visited, finally returning to $S$ to close the loop.

#### Multi-Start Strategy
Instead of running the Greedy NN once (which is sensitive to the starting position), the `main` function iterates through **every possible node** as a starting point ($0 \dots N-1$).
- It generates $N$ different tours.
- It retains the tour with the lowest total cost.
- **Complexity**: $O(N^3)$ (since Greedy NN is $O(N^2)$ and runs $N$ times).

---

## 4. Key Optimizations

### A. Squared Euclidean Distance
To avoid the computationally expensive square root operation (`sqrt`) inside the inner loops, the algorithm uses **squared Euclidean distance** for comparisons.

- **Mathematical Basis**: If $d(A, B) < d(A, C)$, then $d(A, B)^2 < d(A, C)^2$.
- **Implementation**: The `greedyNN` function calls `dist_sq` to find the nearest neighbor. The expensive `sqrt` is only calculated $N$ times at the end to compute the final tour cost.

### B. Pre-Computed Distance Matrix
Upon loading an instance, the program pre-calculates distances between all pairs of nodes.

- **Storage**: The matrix stores **squared distances** to align with optimization A.
- **Memory Layout**: Stored as a flattened 1D array (`double *dists`) to improve cache locality compared to a pointer-to-pointer 2D array.
- **Access**:
  ```c
  // Accessing distance between node i and j
  double d = inst->dists[i * inst->nnodes + j];
  ```

### C. Inline Functions
Distance calculations are defined as `static inline` in `utilities.h`. This allows the compiler to replace function calls with the actual arithmetic instructions, reducing overhead in tight loops.

- `dist_sq(i, j, inst)`: Returns the value from the pre-computed matrix (fastest).
- `dist(i, j, inst)`: Returns the square root of the matrix value (used for final cost).

---

## 5. Visualization

The project integrates with **GNUplot** to visualize tours.
- The `plot_tour` function opens a pipe to GNUplot.
- It sends the coordinates of the nodes in the order of the tour.
- Generates a `tour_plot.png` file showing the path.

---

## 6. Usage

### Compilation
```bash
make
```

### Execution
```bash
# Run on a specific instance
./bin/solver -file data/berlin52.tsp
```