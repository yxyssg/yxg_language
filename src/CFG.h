#ifndef CFG_H
#define CFG_H

#include "IR.h"
#include "regmap.h"
#include <stdbool.h>

typedef struct LiveInterval {
    char*    var_name;
    int      start;
    int      end;
    Reg      assigned_reg;
    bool     is_float;
    int      spill_slot;
} LiveInterval;

typedef struct BasicBlock {
    int    id;
    IR*    first_ir;
    IR*    last_ir;
    int    ir_count;

    struct BasicBlock** succs;
    int  succ_count;
    int  succ_capacity;
    struct BasicBlock** preds;
    int  pred_count;
    int  pred_capacity;

    struct BasicBlock*  idom;
    struct BasicBlock** dom_children;
    int  dom_child_count;
    int  dom_child_capacity;

    struct BasicBlock** df;
    int  df_count;
    int  df_capacity;

    bool* live_gen;
    bool* live_kill;
    bool* live_in;
    bool* live_out;

    IR* phi_nodes;

    char** def_names;
    int    def_count;
    int    def_capacity;
    char** use_names;
    int    use_count;
    int    use_capacity;

    int  linear_start;
    int  linear_end;
} BasicBlock;

typedef struct CFG {
    BasicBlock** blocks;
    int  block_count;
    int  block_capacity;
    BasicBlock* entry;
    BasicBlock* exit_block;

    char** var_names;
    int    var_count;
    int    var_capacity;

    int next_ssa_version;

    LiveInterval* intervals;
    int  interval_count;
} CFG;

CFG* cfg_build(IR* func_ir_start, IR* func_ir_end);
void cfg_destroy(CFG* cfg);
void cfg_print(const CFG* cfg);

void cfg_compute_dominators(CFG* cfg);
void cfg_compute_dominance_frontiers(CFG* cfg);

void cfg_assign_linear_ids(CFG* cfg);

void cfg_liveness_analysis(CFG* cfg);

typedef struct FuncIR {
    char* func_name;
    IR*   first_ir;
    IR*   last_ir;
    struct FuncIR* next;
} FuncIR;

FuncIR* ir_split_by_function(IR* program_ir);
void cfg_remove_unreachable(CFG* cfg);
void    func_ir_list_free(FuncIR* list);

#endif