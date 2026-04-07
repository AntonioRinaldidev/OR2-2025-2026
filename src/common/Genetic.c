#include "Genetic.h"

// TODO: [ ] Implement the "Genetic Correction" of children that checks if the children are feasible. PS: u could also use the 2 opt to optimize them.
void breed(const generation *gen, int *parent1, int *parent2, int *child1, int *child2)
{
    int nnodes = gen->inst->nnodes;

    for (int i = 0; i < nnodes / 2; i++)
    {
        if (child1)
            child1[i] = parent1[i];
        if (child2)
            child2[i] = parent2[i];
    }
    for (int i = nnodes / 2; i < nnodes; i++)
    {
        if (child1)
            child1[i] = parent2[i - nnodes / 2];
        if (child2)
            child2[i] = parent1[i - nnodes / 2];
    }
}
void audit_children_and_repair(const instance *inst, int *child)
{
    if (!child)
        return;
    vertex *vertices = inst->vertices;
    int nnodes = inst->nnodes;
    int *freq = (int *)calloc(nnodes, sizeof(int));
    int *missing = (int *)calloc(nnodes, sizeof(int));
    int missing_count = 0;

    for (int i = 0; i < nnodes; i++)
    {
        freq[child[i]]++;
    }
    for (int i = 0; i < nnodes; i++)
    {
        if (freq[i] == 0)
            missing[missing_count++] = i;
    }
}

void *breed_worker(void *args)
{
    breed_args *arg = (breed_args *)args;
    const generation *gen = arg->gen;
    solution *pool = arg->pool;

    for (int i = arg->start_index; i < arg->end_index; i += 2)
    {
        int p_idx = i / 2;
        int p1 = p_idx % gen->population_size;
        int p2 = (p_idx + 1) % gen->population_size;

        int *child1 = pool[i].tour;
        int *child2 = (i + 1 < arg->end_index) ? pool[i + 1].tour : NULL;

        breed(gen, gen->population[p1].tour, gen->population[p2].tour, child1, child2);

        if (child1)

            pool[i].cost = calculate_cost(gen->inst, child1);
        if (child2)
            pool[i + 1].cost = calculate_cost(gen->inst, child2);
    }
    return NULL;
}

// Comparator function used by qsort to sort solutions by cost (Ascending)
int compare_solutions(const void *a, const void *b)
{
    double cost_a = ((solution *)a)->cost;
    double cost_b = ((solution *)b)->cost;
    if (cost_a < cost_b)
        return -1;
    if (cost_a > cost_b)
        return 1;
    return 0;
}

void natural_selection(generation *gen, generation *new_gen)
{
    int pop_size = new_gen->population_size;
    int num_threads = gen->inst->num_threads <= 0 ? 4 : gen->inst->num_threads;

    // We now have N pairs, producing 2*N children
    int num_pairs = pop_size;
    int pool_size = pop_size * 2;

    // 1. Allocate a temporary pool for all the new children
    solution *pool = malloc(pool_size * sizeof(solution));
    for (int i = 0; i < pool_size; i++)
    {
        pool[i].tour = malloc(gen->inst->nnodes * sizeof(int));
        pool[i].cost = 0.0;
    }

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    breed_args *args = malloc(num_threads * sizeof(breed_args));

    int pairs_per_thread = num_pairs / num_threads;
    int remainder = num_pairs % num_threads;
    int current_pair_start = 0;

    // 2. Creazione dei thread e assegnazione dei blocchi (start_index / end_index)
    for (int t = 0; t < num_threads; t++)
    {
        args[t].gen = gen;
        args[t].pool = pool;
        args[t].start_index = current_pair_start * 2;

        int pairs_for_this_thread = pairs_per_thread + (t < remainder ? 1 : 0);
        current_pair_start += pairs_for_this_thread;

        args[t].end_index = current_pair_start * 2;
        if (args[t].end_index > pool_size)
            args[t].end_index = pool_size;

        pthread_create(&threads[t], NULL, breed_worker, &args[t]);
    }

    for (int t = 0; t < num_threads; t++)
    {
        pthread_join(threads[t], NULL);
    }

    // 3. Sort the pool from best (lowest cost) to worst
    qsort(pool, pool_size, sizeof(solution), compare_solutions);

    // 4. Pure Elitism: Carry over the champion from the previous generation
    memcpy(new_gen->population[0].tour, gen->champion->tour, gen->inst->nnodes * sizeof(int));
    new_gen->population[0].cost = gen->champion->cost;
    new_gen->champion = &new_gen->population[0]; // Start by assuming the old champion remains the best

    // 5. Pool Elitism: Keep the best percentage defined by the user from the new children
    int elites = (pop_size * gen->inst->percentage_elites) / 100;
    if (elites == 0 && pop_size > 0 && gen->inst->percentage_elites > 0)
        elites = 1; // Always keep at least 1 elite if the percentage is > 0

    // Cap elites to avoid overflowing if pop_size is very small (since index 0 is already taken)
    if (elites + 1 > pop_size)
        elites = pop_size - 1;

    for (int i = 0; i < elites; i++)
    {
        memcpy(new_gen->population[i + 1].tour, pool[i].tour, gen->inst->nnodes * sizeof(int));
        new_gen->population[i + 1].cost = pool[i].cost;

        // Update new champion if this elite is better
        if (new_gen->population[i + 1].cost < new_gen->champion->cost)
        {
            new_gen->champion = &new_gen->population[i + 1];
        }
    }

    // 6. Random selection for the remaining children
    for (int i = elites + 1; i < pop_size; i++)
    {
        // Pick a random index from 'elites' up to the end of the pool
        int rand_idx = elites + (rand() % (pool_size - elites));
        memcpy(new_gen->population[i].tour, pool[rand_idx].tour, gen->inst->nnodes * sizeof(int));
        new_gen->population[i].cost = pool[rand_idx].cost;
    }

    // 7. Cleanup the temporary pool
    for (int i = 0; i < pool_size; i++)
    {
        free(pool[i].tour);
    }
    free(pool);
    free(threads);
    free(args);
}
