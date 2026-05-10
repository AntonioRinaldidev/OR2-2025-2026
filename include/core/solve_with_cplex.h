#include "core/utilities.h"
#include <ilcplex/cplex.h>

void init_cplex(instance *inst);
int xpos(instance *inst, int i, int j);
void build_model(instance *inst, CPXENVptr env, CPXLPptr lp);
int find_components(instance *inst, int *succ, int *comp);
int separate_integer_solution(instance *inst, double *xstar, CPXCALLBACKCONTEXTptr context, separationThreadWorkspace *ws);
int CPXPUBLIC callback_driver(CPXCALLBACKCONTEXTptr context, CPXLONG contextid, void *userhandle);
void add_SECs(instance *inst, int n_comp, int *comp, CPXCALLBACKCONTEXTptr context, bool is_fractional);
void post_heuristic_solution(instance *inst, CPXCALLBACKCONTEXTptr context, int *succ, double obj);
void patching_heuristic(instance *inst, int *succ, int *comp, int n_comp);
void solve_with_cplex(instance *inst);
