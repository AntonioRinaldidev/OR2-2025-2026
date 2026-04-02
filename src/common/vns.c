

#include "vns.h"
#include "utilities.h"

/**
 * @brief Generates three unique and sorted random indices within the tour.
 *
 * This function selects 3 random cut points for the 3-opt kick. It ensures
 * that all three indices are strictly unique to avoid degenerate moves (like 0-length blocks).
 * Finally, it sorts the indices so that i < j < k, which simplifies the memory
 * copying process by keeping the array accesses strictly left-to-right.
 *
 * @param nnodes The total number of nodes in the tour.
 * @param indexes An array of size 3 where the generated indices will be stored.
 */
void find_3_indexes(int nnodes, int *indexes)
{
    int i = 0;
    while (i < 3)
    {
        indexes[i] = rand() % nnodes;
        if (i > 0) // Only check for duplicates if we have generated at least one number
        {
            for (int j = 0; j < i; j++)
            {
                if (indexes[j] == indexes[i])
                {
                    // Duplicate found! Decrement 'i' to repeat this step and break inner loop
                    i--;
                    break;
                }
            }
        }
        i++;
    }

    // Sort the indices to ensure i < j < k
    if (indexes[0] > indexes[1])
        swap(&indexes[0], &indexes[1]);
    if (indexes[1] > indexes[2])
        swap(&indexes[1], &indexes[2]);
    if (indexes[0] > indexes[1])
        swap(&indexes[0], &indexes[1]);
}

/**
 * @brief Applies a pure, non-reversing 3-opt kick (Block Swap) to the current solution.
 *
 * This move cuts the tour into 4 blocks based on 3 sorted indices (i, j, k):
 * Block A: [0...i]
 * Block B: [i+1...j]
 * Block C: [j+1...k]
 * Block D: [k+1...N-1]
 *
 * It reconnects them in the order A -> C -> B -> D. Because no segments are reversed,
 * a standard 2-opt local search cannot easily undo this move.
 *
 * @param inst Pointer to the problem instance.
 * @param sol Pointer to the solution to be mutated.
 */
void apply_3_opt_kick(instance *inst, solution *sol)
{
    int indexes[3];
    solution new_sol = {0};
    new_sol.tour = (int *)calloc(inst->nnodes, sizeof(int));

    find_3_indexes(inst->nnodes, indexes);

    int i = indexes[0];
    int j = indexes[1];
    int k = indexes[2];
    int curr = 0;

    // Copy Block A: 0 up to i
    for (int x = 0; x <= i; x++)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // Copy Block C: j+1 up to k (Jumping ahead!)
    for (int x = j + 1; x < k + 1; x++)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // Copy Block B: i+1 up to j (Jumping back!)
    for (int x = i + 1; x <= j; x++)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // Copy Block D: k+1 to the end
    for (int x = k + 1; x < inst->nnodes; x++)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // 1. Copy the assembled tour back into the original solution
    memcpy(sol->tour, new_sol.tour, inst->nnodes * sizeof(int));

    // 2. Free the temporary memory
    free(new_sol.tour);

    // 3. Recalculate the cost since the tour has been significantly altered
    sol->cost = calculate_cost(inst, sol->tour);
}

/**
 * @brief Applies a reversing 3-opt kick to the current solution.
 *
 * Similar to the standard 3-opt kick, this function cuts the tour into 4 blocks.
 * However, it reconnects them by reversing the two middle blocks:
 * A -> Reversed(B) -> Reversed(C) -> D
 *
 * Reconnections: i connects to j, i+1 connects to k, and j+1 connects to k+1.
 * Note: Because this move relies on reversing segments, a 2-opt local search
 * might frequently undo parts of this kick.
 *
 * @param inst Pointer to the problem instance.
 * @param sol Pointer to the solution to be mutated.
 */
void apply_3_opt_kick_reversing(instance *inst, solution *sol)
{
    int indexes[3];
    solution new_sol = {0};
    new_sol.tour = (int *)calloc(inst->nnodes, sizeof(int));

    find_3_indexes(inst->nnodes, indexes);

    int i = indexes[0];
    int j = indexes[1];
    int k = indexes[2];
    int curr = 0;

    // Copy Block A: 0 up to i (Normal order)
    for (int x = 0; x <= i; x++)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // Copy Block B REVERSED: j down to i+1
    for (int x = j; x >= i + 1; x--)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // Copy Block C REVERSED: k down to j+1
    for (int x = k; x >= j + 1; x--)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    // Copy Block D: k+1 to the end (Normal order)
    for (int x = k + 1; x < inst->nnodes; x++)
    {
        new_sol.tour[curr] = sol->tour[x];
        curr++;
    }

    memcpy(sol->tour, new_sol.tour, inst->nnodes * sizeof(int));
    free(new_sol.tour);
    sol->cost = calculate_cost(inst, sol->tour);
}

/**
 * @brief Applies a randomly selected 3-opt kick to the current solution.
 *
 * This function generates 3 random cuts to form 4 blocks (A, B, C, D).
 * It keeps Block A at the start and Block D at the end to maintain the cycle,
 * but randomly selects one of the 7 valid ways to reconnect the middle blocks (B and C).
 *
 * @param inst Pointer to the problem instance.
 * @param sol Pointer to the solution to be mutated.
 */
void apply_random_3_opt_kick(instance *inst, solution *sol)
{
    int indexes[3];
    solution new_sol = {0};
    new_sol.tour = (int *)calloc(inst->nnodes, sizeof(int));

    find_3_indexes(inst->nnodes, indexes);

    int i = indexes[0];
    int j = indexes[1];
    int k = indexes[2];
    int curr = 0;

    // 1. Always copy Block A normally
    for (int x = 0; x <= i; x++)
        new_sol.tour[curr++] = sol->tour[x];

    // 2. Randomly select one of the 7 valid reconnections for the middle blocks
    int move_type = rand() % 7;

    switch (move_type)
    {
    case 0: // C -> B (Pure Block Swap)
        for (int x = j + 1; x <= k; x++)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = i + 1; x <= j; x++)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    case 1: // B' -> C' (Reversing both, original order)
        for (int x = j; x >= i + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = k; x >= j + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    case 2: // B' -> C (Reverse B only)
        for (int x = j; x >= i + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = j + 1; x <= k; x++)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    case 3: // B -> C' (Reverse C only)
        for (int x = i + 1; x <= j; x++)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = k; x >= j + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    case 4: // C' -> B (Reverse C, swap order)
        for (int x = k; x >= j + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = i + 1; x <= j; x++)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    case 5: // C -> B' (Reverse B, swap order)
        for (int x = j + 1; x <= k; x++)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = j; x >= i + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    case 6: // C' -> B' (Reverse both, swap order)
        for (int x = k; x >= j + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        for (int x = j; x >= i + 1; x--)
            new_sol.tour[curr++] = sol->tour[x];
        break;
    }

    // 3. Always copy Block D normally at the end
    for (int x = k + 1; x < inst->nnodes; x++)
        new_sol.tour[curr++] = sol->tour[x];

    // 4. Finalize the move
    memcpy(sol->tour, new_sol.tour, inst->nnodes * sizeof(int));
    free(new_sol.tour);
    sol->cost = calculate_cost(inst, sol->tour);
}
