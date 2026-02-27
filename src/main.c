#include <stdio.h>
#include <stdlib.h>

int main()
{
    // Alloco spazio per 10 interi
    int *array = malloc(10 * sizeof(int));

    // Errore 1: Accedo all'indice 100 (fuori dai bordi!)
    array[9] = 42;

        // Errore 2: Non libero la memoria (Memory Leak)
    // free(array);
    printf("Se leggi questo, ASan non sta funzionando.\n");

    return 0;
}