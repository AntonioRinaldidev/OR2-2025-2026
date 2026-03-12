#include "greedyNN.h"
#include "utilities.h"

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