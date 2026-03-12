# 📚 API Reference & Function Documentation

This document provides a detailed reference for the functions implemented in the TSP Solver project.

---

## 1. Core Logic
**File:** `src/vrp/main.c`

### `int main(int argc, char **argv)`
The main entry point for the TSP/VRP solver.
- **Responsibilities**: 
  - Initializes the `instance` structure.
  - Calls parsers for command-line arguments and input files.
  - Pre-computes the distance matrix.
  - Checks for an existing optimal tour (`.opt.tour`) to validate and plot.
  - Runs the **Multi-Start Greedy Nearest Neighbor** heuristic.
  - Validates and plots the best heuristic solution found.
  - Cleans up memory before exiting.

### `void free_instance(instance *inst)`
Frees all dynamically allocated memory within the `instance` structure.
- **Parameters**: `inst` - Pointer to the instance to clean up.
- **Details**: Safely frees `xcoord`, `ycoord`, `dists`, and the `best_solution.tour` to prevent memory leaks.

---

## 2. Heuristics
**File:** `src/greedyNN.c`

### `void greedyNN(instance *inst, solution *sol, int start_node)`
Computes a TSP tour using the Greedy Nearest Neighbor heuristic starting from a specific node.
- **Parameters**:
  - `inst`: Pointer to the problem instance.
  - `sol`: Pointer to the solution structure to populate.
  - `start_node`: The index (0-based) of the node to start the tour from.
- **Algorithm**:
  1. Starts at `start_node`.
  2. Iteratively finds the nearest unvisited neighbor using **squared Euclidean distance** (for speed).
  3. Swaps nodes in the `sol->tour` array to build the path in-place.
  4. Calculates the final `sol->cost` using standard Euclidean distance.

---

## 3. Utilities & Helpers
**File:** `src/common/utilities.c`

### `void parse_command_line(int argc, char **argv, instance *inst)`
Parses command-line arguments to configure solver settings.
- **Flags handled**:
  - `-file <path>`: Input TSPLIB file.
  - `-threads <n>`: Number of threads (0 for auto).
  - `-seed <n>`: Random seed (required for random generation).
  - `-node_number <n>`: Number of nodes for random generation.
  - `-verbose <n>`: Verbosity level (0-5).
  - `-time_limit <s>` or `-time <s>`: Execution time limit in seconds.

### `void parse_instance(instance *inst)`
Parses the input file (TSPLIB format) to populate the instance structure.
- **Details**: Reads `DIMENSION` to allocate memory and `NODE_COORD_SECTION` to store coordinates. Supports `VERBOSE` logging levels for debugging parsing issues.

### `void compute_distances(instance *inst)`
Pre-computes the distance matrix for the instance.
- **Details**: Calculates the **squared** Euclidean distance between every pair of nodes and stores it in a flattened 1D array (`inst->dists`). This speeds up the Greedy NN lookups significantly.

### `int validate_tour(solution *sol, instance *inst)`
Validates the structural integrity and cost consistency of a given tour.
- **Checks**:
  1. **Bounds**: Are all node indices within `[0, N-1]`?
  2. **Uniqueness**: Is every node visited exactly once?
  3. **Cost**: Does the re-calculated cost match the reported `sol->cost` (within tolerance)?
- **Returns**: `1` if valid, `0` otherwise.

### `void plot_tour(instance *inst, int *tour)`
Generates a visual plot of the TSP tour using **GNUplot**.
- **Output**: Creates a file named `tour_plot.png`.
- **Mechanism**: Opens a pipe to `gnuplot`, sends setup commands, and streams coordinate data.

### `int parse_tour(instance *inst, int *tour)`
Parses a `.opt.tour` solution file (if available).
- **Usage**: Used to load known optimal solutions for comparison.
- **Returns**: `1` if the file was found and parsed, `0` otherwise.

### `void print_tour(int *tour, int num_nodes)`
Prints the sequence of the tour to `stdout`.
- **Format**: Converts internal 0-based indices to 1-based indices for readability.

### `void print_error(const char *err)`
Prints an error message in red to `stdout` and terminates the program immediately with `exit(1)`.

### `instance generate_random_instance(int nnodes, double x_max, double y_max, int seed)`
Generates a random TSP instance with specified dimensions and seed.
- **Parameters**:
  - `nnodes`: Number of nodes.
  - `x_max`, `y_max`: Maximum coordinate values.
  - `seed`: Random seed for reproducibility.
- **Returns**: A fully initialized instance structure with random coordinates. No filename is associated with it.

---

## 4. Inline Functions
**File:** `include/utilities.h`

These functions are defined as `static inline` for performance.

### `double dist(int i, int j, instance *inst)`
Calculates the **Euclidean distance** between node `i` and node `j`.
- **Optimization**: If the distance matrix (`inst->dists`) exists, it returns `sqrt(matrix_value)`. Otherwise, it calculates it on the fly.

### `double dist_sq(int i, int j, instance *inst)`
Calculates the **Squared Euclidean distance** between node `i` and node `j`.
- **Usage**: Used primarily in the inner loops of heuristics (like Greedy NN) to avoid expensive square root operations during comparisons.
- **Optimization**: Returns the value directly from `inst->dists` if available.

---

## 5. Data Structures

### `instance`
- `nnodes`: Number of nodes.
- `xcoord`, `ycoord`: Arrays of coordinates.
- `dists`: Flattened distance matrix.
- `timelimit`, `randomseed`, `num_threads`: Configuration parameters.

### `solution`
- `tour`: Array of node indices representing the path.
- `cost`: Total length of the tour.

---

## 6. Recent Additions

### `double calculate_cost(instance *inst, int *tour)`
Calculates the total Euclidean cost of a tour.
- **Usage**: Centralized function used by `greedyNN` (final step), `validate_tour`, and `main` to ensure cost consistency.

### CLI Updates
- **`-node_number <n>`**: Specifies size for random instances (mutually exclusive with `-file`).
- **`-verbose <n>`**: Sets internal `VERBOSE` level (0=Silent, 1=Info, 2=Default, 3=Detail, 4=Debug, 5=Trace).