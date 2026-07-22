#include "construction/construction.h"
#include "core/utilities.h"

/**
 * @brief Computes a TSP tour using the Greedy Nearest Neighbor heuristic.
 *
 * This algorithm starts at the first node (node 0) and iteratively adds the
 * closest unvisited node until all nodes have been visited. The tour is built
 * in-place within a single array by swapping nodes into their correct positions.
 *
 * The selection of the "nearest" neighbor is based on the *squared* Euclidean
 * distance to avoid costly square root operations in the inner loop. The final
 * tour cost, however, is calculated using the standard Euclidean distance.
 *
 * @param inst A pointer to the instance structure, containing node coordinates and counts.
 * @param sol A pointer to the solution structure where the resulting tour and its cost will be stored. * @param start_node The node to start the tour from.
 */
void greedyNN(instance *inst, solution *sol, int start_node)
{
    int n = inst->nnodes;
    if (start_node < 0 || start_node >= n)
    {
        print_error("Greedy NN start node is out of bounds");
    }

    int *path = sol->tour;
    sol->cost = 0.0;

    // 1. Initialize the path with all nodes 0..n-1
    for (int i = 0; i < n; i++)
    {
        path[i] = i;
    }

    // 2. Place the starting node at the beginning of the path
    // Swap the initial node (0) with the chosen start_node
    int temp = path[0];
    path[0] = path[start_node];
    path[start_node] = temp;

    // 3. Iteratively build the tour by finding the nearest neighbor and swapping it into place
    for (int i = 0; i < n - 1; i++)
    {
        int current_node = path[i];
        int best_swap_idx = i + 1;
        double min_dist = INF;

        // Find the nearest unvisited node in the remainder of the array (from i+1 to n-1)
        for (int j = i + 1; j < n; j++)
        {
            int candidate_node = path[j];
            double d = dist_sq(current_node, candidate_node, inst);

            if (d < min_dist)
            {
                min_dist = d;
                best_swap_idx = j;
            }
        }

        // 4. Swap the nearest neighbor into the next position in the tour (i+1)
        temp = path[i + 1];
        path[i + 1] = path[best_swap_idx];
        path[best_swap_idx] = temp;
        sol->cost += dist(path[i], path[i + 1], inst);
    }

    // Add cost to return to the starting node to complete the cycle
    sol->cost += dist(path[n - 1], path[0], inst);
}

void extra_mileage(instance *inst, solution *sol, int start_node)
{
    if (start_node < 0 || start_node >= inst->nnodes)
        print_error("Extra Mileage start node is out of bounds");

    int n = inst->nnodes;
    int *path = sol->tour;
    int temp;

    // 1. Initialize path with all nodes 0..n-1.
    for (int i = 0; i < n; i++)
        path[i] = i;

    // 2. Move start_node to position 0.
    temp = path[0];
    path[0] = path[start_node];
    path[start_node] = temp;

    // 3. Find the farthest node from start_node among the rest, move it to position 1.
    //    Uses dist (not dist_sq): unlike a single nearest-neighbor comparison,
    //    extra mileage compares SUMS of distances later on, and A^2+B^2 < C^2+D^2
    //    does not imply A+B < C+D -- same pitfall documented for 2-opt.
    int farthest_pos = 1;
    double best_d = dist(path[0], path[1], inst);
    for (int k = 2; k < n; k++)
    {
        double d = dist(path[0], path[k], inst);
        if (d > best_d)
        {
            best_d = d;
            farthest_pos = k;
        }
    }
    temp = path[1];
    path[1] = path[farthest_pos];
    path[farthest_pos] = temp;

    // path[0..1] is now the initial 2-node "cycle" (start_node -> farthest -> start_node);
    // path[2..n-1] holds the unvisited nodes.
    int tour_size = 2;
    sol->cost = 2.0 * dist(path[0], path[1], inst); // there-and-back

    // 4. Cheapest-insertion loop.
    while (tour_size < n)
    {
        int best_cand_pos = -1;     // index (>= tour_size) in path of the chosen unvisited node
        int best_insert_after = -1; // insert into the edge (path[t], path[(t+1) % tour_size])
        double best_extra = INF;

        for (int cand = tour_size; cand < n; cand++)
        {
            int k = path[cand];
            for (int t = 0; t < tour_size; t++)
            {
                int i = path[t];
                int j = path[(t + 1) % tour_size];
                double extra = dist(i, k, inst) + dist(k, j, inst) - dist(i, j, inst);
                if (extra < best_extra)
                {
                    best_extra = extra;
                    best_cand_pos = cand;
                    best_insert_after = t;
                }
            }
        }

        // Swap the chosen candidate to the tour/unvisited boundary slot.
        temp = path[tour_size];
        path[tour_size] = path[best_cand_pos];
        path[best_cand_pos] = temp;
        int chosen_node = path[tour_size];

        // Shift the tail of the tour right by one to open up the insertion point,
        // then splice the chosen node in.
        int insert_at = best_insert_after + 1;
        for (int shift = tour_size; shift > insert_at; shift--)
            path[shift] = path[shift - 1];
        path[insert_at] = chosen_node;

        sol->cost += best_extra;
        tour_size++;
    }
}
void cardinality_grasp(instance *inst, solution *sol, int cardinality, int start_node, unsigned int *seed)
{
    if (start_node < 0 || start_node >= inst->nnodes)
    {
        print_error("Cardinality Grasp start node is out of bounds");
    }

    if (cardinality < 1)
    {
        print_error("Cardinality needs to be at least 1");
    }

    int *path = sol->tour;
    sol->cost = 0.0;

    int *best_k_nodes = (int *)malloc(cardinality * sizeof(int));
    double *best_k_dists = (double *)malloc(cardinality * sizeof(double));

    //)
    // 1. Initialize the path with all nodes 0..n-1
    for (int i = 0; i < inst->nnodes; i++)
    {
        path[i] = i;
    }

    int temp = path[0];
    path[0] = path[start_node];
    path[start_node] = temp;

    for (int i = 0; i < inst->nnodes - 1; i++)
    {
        int current_node = path[i];
        int count = 0;

        for (int j = i + 1; j < inst->nnodes; j++)
        {
            int candidate_node = path[j];
            double d = dist_sq(current_node, candidate_node, inst);
            if (count < cardinality)
            {
                best_k_nodes[count] = j;
                best_k_dists[count] = d;
                count++;
            }
            else
            {
                int worst_dist_idx = 0;
                for (int k = 1; k < cardinality; k++)
                {

                    if (best_k_dists[k] > best_k_dists[worst_dist_idx])
                    {
                        worst_dist_idx = k;
                    }
                }
                if (d < best_k_dists[worst_dist_idx])
                {
                    best_k_nodes[worst_dist_idx] = j;
                    best_k_dists[worst_dist_idx] = d;
                }
            }
        }
        int pick = rand_r(seed) % count;

        int path_index = best_k_nodes[pick];

        int selected_node = path[path_index];
        path[path_index] = path[i + 1];
        path[i + 1] = selected_node;

        sol->cost += dist(path[i], path[i + 1], inst);
    }
    sol->cost += dist(path[inst->nnodes - 1], path[0], inst);
    free(best_k_nodes);
    free(best_k_dists);
}

void value_based_grasp(instance *inst, double alpha, solution *sol, int start_node, unsigned int *seed)
{
    if (start_node < 0 || start_node >= inst->nnodes)
    {
        print_error("Value Grasp start node is out of bounds");
    }

    if (alpha < 0 || alpha > 1)
    {
        print_error("Value Grasp alpha needs to be between 0 and 1");
    }

    int *path = sol->tour;
    sol->cost = 0.0;

    // 1. Initialize the path with all nodes 0..n-1
    for (int i = 0; i < inst->nnodes; i++)
    {
        path[i] = i;
    }

    int temp = path[0];
    path[0] = path[start_node];
    path[start_node] = temp;

    // RCL buffer, reused every iteration (sized for the worst case: all candidates qualify)
    int *rcl = (int *)malloc(inst->nnodes * sizeof(int));

    for (int i = 0; i < inst->nnodes - 1; i++)
    {
        int current_node = path[i];

        // --- Pass 1: find d_min and d_max among remaining candidates ---
        double d_min = INF;
        double d_max = -INF;

        for (int j = i + 1; j < inst->nnodes; j++)
        {
            int candidate_node = path[j];
            double d = dist_sq(current_node, candidate_node, inst);

            if (d < d_min)
                d_min = d;
            if (d > d_max)
                d_max = d;
        }

        double threshold = d_min + alpha * (d_max - d_min);

        // --- Pass 2: build the RCL from candidates within the threshold ---
        int rcl_count = 0;
        for (int j = i + 1; j < inst->nnodes; j++)
        {
            int candidate_node = path[j];
            double d = dist_sq(current_node, candidate_node, inst);

            if (d <= threshold)
            {
                rcl[rcl_count++] = j;
            }
        }

        // --- Pick uniformly at random from the RCL ---
        int pick = rand_r(seed) % rcl_count;
        int best_swap_idx = rcl[pick];

        // --- Swap the chosen node into position i+1 ---
        int selected_node = path[best_swap_idx];
        path[best_swap_idx] = path[i + 1];
        path[i + 1] = selected_node;

        sol->cost += dist(path[i], path[i + 1], inst);
    }

    // Close the loop
    sol->cost += dist(path[inst->nnodes - 1], path[0], inst);

    free(rcl);
}