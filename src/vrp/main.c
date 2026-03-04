#include "vrp.h"

double second();
void print_error(const char *err);
void check_dblsort();
double random01();
void parse_instance(instance *inst);
void parse_command_line(int argc, char **argv, instance *inst);

void debug(const char *err)
{
    printf("\nDEBUG: %s \n", err);
    fflush(NULL);
}

int number_of_nonempty_lines(const char *file) // warning: the last line NOT counted if it is does not terminate with \n (as it happens with some editors)
{
    FILE *fin = fopen(file, "r");
    if (fin == NULL)
        return 0;
    char line[123456];
    int count = 0;
    while (fgets(line, sizeof(line), fin) != NULL)
    {
        printf(" len %4d\n", (int)strlen(line));
        if (strlen(line) > 1)
            count++;
    }
    fclose(fin);
    return count;
}

void free_instance(instance *inst)
{
    free(inst->demand);
    free(inst->xcoord);
    free(inst->ycoord);
    free(inst->load_min);
    free(inst->load_max);
}

int VRPopt(instance *inst);

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s -help for help\n", argv[0]);
        exit(1);
    }
    if (VERBOSE >= 2)
    {
        for (int a = 0; a < argc; a++)
            printf("%s ", argv[a]);
        printf("\n");
    }

    double t1 = second();
    instance inst;

    parse_command_line(argc, argv, &inst);

    // printf(" file %s has %d non-empty lines\n", inst.input_file, number_of_nonempty_lines(inst.input_file)); exit(1);

    parse_instance(&inst);
    if (VRPopt(&inst))
        print_error(" error within VRPopt()");
    double t2 = second();

    if (VERBOSE >= 1)
    {
        printf("... VRP problem solved in %lf sec.s\n", t2 - t1);
    }

    free_instance(&inst);
    return 0;
}
