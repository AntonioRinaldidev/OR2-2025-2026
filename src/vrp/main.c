#include "vrp.h"

void print_error(const char *err);

void parse_instance(instance *inst);
void parse_command_line(int argc, char **argv, instance *inst);

void debug(const char *err)
{
    printf("\nDEBUG: %s \n", err);
    fflush(NULL);
}

void free_instance(instance *inst)
{
    free(inst->demand);
    free(inst->xcoord);
    free(inst->ycoord);
    free(inst->load_min);
    free(inst->load_max);
}

int main(int argc, char **argv)
{

    if (VERBOSE >= 2)
    {
        for (int a = 0; a < argc; a++)
            printf("%s ", argv[a]);
        printf("\n");
    }

    instance inst = {0};

    parse_command_line(argc, argv, &inst);

    parse_instance(&inst);

    free_instance(&inst);
    return 0;
}
