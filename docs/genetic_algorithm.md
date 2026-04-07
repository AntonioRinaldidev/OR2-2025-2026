# 🧬 Genetic Algorithm - Implementation Details

This document outlines the architecture and step-by-step logic of the Genetic Algorithm (GA) implemented for the TSP solver.

## 1. Core Architecture Overview

The GA module is designed to iteratively improve a population of TSP tours (solutions) by simulating the process of natural evolution. It relies heavily on **multi-threading** for computational efficiency and a mixed **Elitism/Randomness** strategy to avoid premature convergence.

The main functions driving this process reside in `src/common/Genetic.c`.

---

## 2. Functions & Responsibilities (Updated)

### `crossover` & `ox1_crossover` (Formerly `breed`)
**Purpose:** Acts as the crossover operator. It takes two parent tours and generates two children.
* **Update from Previous Version:** The original `breed` function generated mathematically invalid TSP tours. This has been updated: `crossover` now uses a naive split accompanied by an `audit_children_and_repair` function to fix duplicate/missing cities. Additionally, an advanced `ox1_crossover` (Order Crossover) has been implemented to natively guarantee permutation safety.

### `crossover_worker` (Formerly `breed_worker`)
**Purpose:** The thread-safe worker function that parallelizes the generation of new children.
* **Chunking:** Instead of using mutex locks (which are slow), each thread is assigned a specific `start_index` and `end_index`. It only writes into its exclusively owned block of the temporary pool array.
* **Parent Selection:** The loop advances by 2 (since every pairing produces 2 children). It sequentializes parent pairings using modulo arithmetic (`i / 2`) to ensure valid parent indices.
* **Update:** Thread workloads now dynamically switch between Naive crossover (with repair) and OX1 crossover based on the user's `inst->crossover_type` configuration.
* **Thread-Safe Evaluation:** After calling `breed`, the worker immediately calculates the geometric cost of the newly generated children. Since `calculate_cost` only reads from the read-only `instance` structure, this $O(N)$ operation runs concurrently across all threads with zero race conditions.

### `compare_solutions`
**Purpose:** A standard comparator function utilized by `qsort`.
* **Logic:** Evaluates two `solution` structs and sorts them in **ascending order** based on their `cost` (so the lowest cost/best solutions appear at the beginning of the array).

### `natural_selection`
**Purpose:** The orchestrator of the generational step. It manages memory, dispatches threads, and decides which children survive into the next generation.

---

## 3. The Natural Selection Lifecycle

The `natural_selection` function implements a highly robust evolutionary strategy:

#### Step 1: Overproduction (The Pool)
Instead of generating exactly `N` children for a population of size `N`, the algorithm allocates a temporary pool of size `2 * N`. This allows the algorithm to explore a wider genetic neighborhood before deciding who survives.

#### Step 2: Parallel Breeding
The `2 * N` generation workload is divided evenly among the available CPU threads. The threads execute `crossover_worker`, populating the temporary pool and evaluating the fitness (cost) of every single child.

#### Step 3: Sorting
Once all threads are joined and the pool is fully populated and evaluated, `qsort` is applied to rank all `2 * N` children from best (lowest cost) to worst.

#### Step 4: Pure Elitism (The Champion)
To guarantee **Monotonic Convergence** (meaning the algorithm's global best can never get worse), the algorithm uses the `champion` pointer to instantly access the absolute best individual of the previous generation. This "Champion" is explicitly copied into index `0` of the new generation in $O(1)$ time.

#### Step 5: Pool Elitism
The top `X%` (configurable via `-elites`, default 10%) of the *newly bred* children are skimmed off the top of the sorted pool. These elite children are guaranteed survival and are copied directly into the new generation, preserving the best newly discovered genetic traits.

#### Step 6: Random Survival
If we only kept the absolute best children, the population would quickly become identical (premature convergence), trapping the solver in a local minimum. To maintain **Genetic Diversity**, the remaining slots in the new generation are filled by picking purely at random from the non-elite children in the pool.

#### Step 7: Memory Cleanup
The temporary pool of `2 * N` children is safely destroyed, preventing memory leaks, and the new generation is ready for the next iteration.

---

## 4. Future Improvements Planned
1. **Tournament Selection:** Replace the sequential parent selection (`parent[i]` mating with `parent[i+1]`) with a tournament model that favors fitter parents.
2. **Mutation / Kicks:** Introduce a small probability for children to undergo a random mutation (like a 2-opt or 3-opt kick) to further explore the solution space.