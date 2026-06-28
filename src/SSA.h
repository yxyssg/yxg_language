#ifndef SSA_H
#define SSA_H

#include "CFG.h"
#include <stdbool.h>

bool ssa_construct(CFG* cfg);

IR* ssa_destruct(CFG* cfg);

int ssa_dead_code_elimination(CFG* cfg);

int ssa_copy_propagation(CFG* cfg);

int ssa_constant_folding(CFG* cfg);

int ssa_constant_propagation(CFG* cfg);

void ssa_print_cfg(const CFG* cfg);

#endif