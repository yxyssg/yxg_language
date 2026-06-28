#include "regmap.h"

#include <ctype.h>

#include "cfg.h"
#include "Error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static const Reg AVAILABLE_REGS[] = {
    R_EAX, R_ECX, R_EDX, R_EBX, R_ESI, R_EDI
};
#define NUM_REGS 6

static const char* REG_NAMES[] = {
    "eax", "ecx", "edx", "ebx", "esi", "edi"
};

#define BIAS_HARD_DIV  30
#define BIAS_HARD_RET  25
#define BIAS_SOFT_DIV  20
#define BIAS_SOFT_CALL 15
#define BIAS_SOFT_MUL   8
#define BIAS_CALLEE_L   6
#define BIAS_CALLER_S   3

#define LOOP_WEIGHT_BASE 1
#define LOOP_WEIGHT_MULT 10

typedef struct IGNode {
    char*   var_name;
    int     var_index;
    int     degree;
    int     orig_degree;
    double  spill_cost;
    int     loop_depth;
    Reg     color;
    int     spill_slot;
    int     bias[NUM_REGS];
    bool    is_float;

    bool*   adj;
    int     adj_size;
    int     node_index;
    bool    spilled;
    bool    simplified;
} IGNode;

RegMap* g_regmap = NULL;

static bool looks_like_label(const char* s) {
    if (!s || !*s) return false;
    if (s[0] == '_' || s[0] == '.') return false;
    if (s[0] >= 'A' && s[0] <= 'Z') return true;
    return false;
}

static void compute_live_intervals(CFG* cfg) {
    int n = cfg->var_count;
    if (n == 0) return;

    if (cfg->intervals) {
        for (int i = 0; i < cfg->interval_count; i++)
            free(cfg->intervals[i].var_name);
        free(cfg->intervals);
        cfg->intervals = NULL;
    }

    cfg->interval_count = n;
    cfg->intervals = calloc((size_t)n, sizeof(LiveInterval));
    if (!cfg->intervals) OUT_OF_MEMORY();

    for (int i = 0; i < n; i++) {
        cfg->intervals[i].var_name     = cfg->var_names[i];
        cfg->intervals[i].start        = INT_MAX;
        cfg->intervals[i].end          = -1;
        cfg->intervals[i].assigned_reg = R_NONE;
        cfg->intervals[i].spill_slot   = -1;
    }

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        int local_id   = bb->linear_start;

        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->result && ir->result[0]) {
                const char* name = ir->result;
                bool is_num = true;
                for (const char* p = (*name == '-') ? name + 1 : name; *p; p++)
                    if (!isdigit((unsigned char)*p)) { is_num = false; break; }

                if (!is_num && !looks_like_label(name)) {
                    for (int vi = 0; vi < n; vi++) {
                        if (strcmp(cfg->var_names[vi], name) == 0) {
                            if (local_id < cfg->intervals[vi].start)
                                cfg->intervals[vi].start = local_id;
                            if (local_id > cfg->intervals[vi].end)
                                cfg->intervals[vi].end = local_id;

                            if (ir->data_type == DATA_FLOAT ||
                                ir->data_type == DATA_UFLOAT) {
                                cfg->intervals[vi].is_float = true;
                            }
                            break;
                        }
                    }
                }
            }

            if (ir->args1 && ir->args1[0]) {
                const char* name = ir->args1;
                bool is_num = true;
                for (const char* p = (*name == '-') ? name + 1 : name; *p; p++)
                    if (!isdigit((unsigned char)*p)) { is_num = false; break; }

                if (!is_num && !looks_like_label(name) &&
                    ir->type != IR_JMP && ir->type != IR_LABEL) {
                    for (int vi = 0; vi < n; vi++) {
                        if (strcmp(cfg->var_names[vi], name) == 0) {
                            if (local_id < cfg->intervals[vi].start)
                                cfg->intervals[vi].start = local_id;
                            if (local_id > cfg->intervals[vi].end)
                                cfg->intervals[vi].end = local_id;
                            break;
                        }
                    }
                }
            }

            if (ir->args2 && ir->args2[0]) {
                const char* name = ir->args2;
                bool is_num = true;
                for (const char* p = (*name == '-') ? name + 1 : name; *p; p++)
                    if (!isdigit((unsigned char)*p)) { is_num = false; break; }

                if (!is_num && !looks_like_label(name) &&
                    ir->type != IR_JMPZ && ir->type != IR_JMPNZ) {
                    for (int vi = 0; vi < n; vi++) {
                        if (strcmp(cfg->var_names[vi], name) == 0) {
                            if (local_id < cfg->intervals[vi].start)
                                cfg->intervals[vi].start = local_id;
                            if (local_id > cfg->intervals[vi].end)
                                cfg->intervals[vi].end = local_id;
                            break;
                        }
                    }
                }
            }

            if (ir == bb->last_ir) break;
            ir = ir->next;
            local_id++;
        }
    }

    for (int i = 0; i < n; i++)
        if (cfg->intervals[i].end < cfg->intervals[i].start)
            cfg->intervals[i].start = cfg->intervals[i].end = -1;
}

static void compute_loop_depths(CFG* cfg, int* block_depths) {
    int n = cfg->block_count;
    memset(block_depths, 0, (size_t)n * sizeof(int));

    if (cfg->blocks[0] && !cfg->blocks[0]->idom && n > 1) {
        cfg_compute_dominators(cfg);
    }

    for (int bi = 0; bi < n; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        for (int si = 0; si < bb->succ_count; si++) {
            BasicBlock* succ = bb->succs[si];
            BasicBlock* dom = bb->idom;
            bool is_backedge = (dom == succ);
            while (dom && !is_backedge) {
                dom = dom->idom;
                if (dom == succ) is_backedge = true;
            }

            if (is_backedge) {
                BasicBlock* runner = bb;
                while (runner && runner != succ) {
                    block_depths[runner->id]++;
                    runner = runner->idom;
                }
                block_depths[succ->id]++;
            }
        }
    }
}

static IGNode* build_interference_graph(CFG* cfg, int* out_count) {
    int n = cfg->interval_count;

    int valid = 0;
    for (int i = 0; i < n; i++)
        if (cfg->intervals[i].start >= 0) valid++;

    *out_count = valid;
    if (valid == 0) return NULL;

    IGNode* nodes = calloc((size_t)valid, sizeof(IGNode));
    if (!nodes) OUT_OF_MEMORY();

    int* vi_to_ni = malloc((size_t)n * sizeof(int));
    if (!vi_to_ni) OUT_OF_MEMORY();
    for (int i = 0; i < n; i++) vi_to_ni[i] = -1;

    int ni = 0;
    for (int vi = 0; vi < n; vi++) {
        if (cfg->intervals[vi].start < 0) continue;
        vi_to_ni[vi] = ni;
        nodes[ni].var_name   = cfg->var_names[vi];
        nodes[ni].var_index  = vi;
        nodes[ni].node_index = ni;
        nodes[ni].adj_size   = valid;
        nodes[ni].is_float   = cfg->intervals[vi].is_float;
        nodes[ni].adj        = calloc((size_t)valid, sizeof(bool));
        if (!nodes[ni].adj) OUT_OF_MEMORY();
        ni++;
    }

    for (int a_vi = 0; a_vi < n; a_vi++) {
        if (cfg->intervals[a_vi].start < 0) continue;
        int a_ni = vi_to_ni[a_vi];
        LiveInterval* ia = &cfg->intervals[a_vi];

        for (int b_vi = a_vi + 1; b_vi < n; b_vi++) {
            if (cfg->intervals[b_vi].start < 0) continue;
            int b_ni = vi_to_ni[b_vi];
            LiveInterval* ib = &cfg->intervals[b_vi];

            if (ia->start < ib->end && ib->start < ia->end) {
                nodes[a_ni].adj[b_ni] = true;
                nodes[a_ni].degree++;
                nodes[b_ni].adj[a_ni] = true;
                nodes[b_ni].degree++;
            }
        }
    }

    for (int i = 0; i < valid; i++)
        nodes[i].orig_degree = nodes[i].degree;

    free(vi_to_ni);
    return nodes;
}

static void compute_spill_costs(CFG* cfg, IGNode* nodes, int count,
                                const int* block_depths) {
    if (count == 0) return;

    for (int ni = 0; ni < count; ni++) {
        IGNode* node = &nodes[ni];
        int vi = node->var_index;
        LiveInterval* iv = &cfg->intervals[vi];

        int base_cost = iv->end - iv->start + 1;

        int max_depth = 0;
        for (int bi = 0; bi < cfg->block_count; bi++) {
            BasicBlock* bb = cfg->blocks[bi];
            if (bb->linear_end < iv->start || bb->linear_start > iv->end)
                continue;
            if (block_depths[bi] > max_depth)
                max_depth = block_depths[bi];
        }

        double weight = 1.0;
        for (int d = 0; d < max_depth; d++)
            weight *= (double)LOOP_WEIGHT_MULT;

        node->spill_cost = (double)base_cost * weight;
        node->loop_depth = max_depth;
    }
}

static int find_var_index_cfg(const CFG* cfg, const char* name) {
    if (!name) return -1;
    for (int i = 0; i < cfg->var_count; i++)
        if (strcmp(cfg->var_names[i], name) == 0) return i;
    return -1;
}

static void compute_bias_scores(CFG* cfg, IGNode* nodes, int count) {
    if (count == 0) return;

    int* vi_to_ni = malloc((size_t)cfg->var_count * sizeof(int));
    if (!vi_to_ni) OUT_OF_MEMORY();
    for (int i = 0; i < cfg->var_count; i++) vi_to_ni[i] = -1;
    for (int ni = 0; ni < count; ni++)
        vi_to_ni[nodes[ni].var_index] = ni;

    #define ADD_BIAS(op, reg, score) do {        \
        int _vi = find_var_index_cfg(cfg, (op));  \
        if (_vi >= 0 && vi_to_ni[_vi] >= 0)       \
            nodes[vi_to_ni[_vi]].bias[(reg)] += (score); \
    } while(0)

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;

        while (ir) {
            if (ir->type == IR_BINARY) {
                switch (ir->op) {
                case IR_DIV: case IR_MOD:
                case IR_UDIV: case IR_UMOD:
                    ADD_BIAS(ir->args1,  R_EAX, BIAS_HARD_DIV);
                    ADD_BIAS(ir->result, R_EAX, BIAS_SOFT_DIV);
                    break;
                case IR_MUL:
                    ADD_BIAS(ir->result, R_EAX, BIAS_SOFT_MUL);
                    ADD_BIAS(ir->args1,  R_EAX, 4);
                    break;
                default: break;
                }
            } else if (ir->type == IR_CALL) {
                ADD_BIAS(ir->result, R_EAX, BIAS_SOFT_CALL);
            } else if (ir->type == IR_RET) {
                ADD_BIAS(ir->args1, R_EAX, BIAS_HARD_RET);
            }

            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    #undef ADD_BIAS

    double avg_cost = 0;
    for (int ni = 0; ni < count; ni++)
        avg_cost += nodes[ni].spill_cost;
    if (count > 0) avg_cost /= (double)count;

    for (int ni = 0; ni < count; ni++) {
        IGNode* node = &nodes[ni];
        if (node->spill_cost > avg_cost * 2.0) {
            node->bias[R_EBX] += BIAS_CALLEE_L;
            node->bias[R_ESI] += BIAS_CALLEE_L;
            node->bias[R_EDI] += BIAS_CALLEE_L;
        } else if (node->spill_cost < avg_cost * 0.5) {
            node->bias[R_EAX] += BIAS_CALLER_S;
            node->bias[R_ECX] += BIAS_CALLER_S;
            node->bias[R_EDX] += BIAS_CALLER_S;
        }
    }

    free(vi_to_ni);
}

static void remove_node_from_graph(IGNode* node, IGNode* nodes, int count) {
    for (int j = 0; j < count; j++) {
        if (node->adj[j] && !nodes[j].spilled && !nodes[j].simplified) {
            nodes[j].degree--;
        }
    }
}

static void restore_node_to_graph(IGNode* node, IGNode* nodes, int count) {
    for (int j = 0; j < count; j++) {
        if (node->adj[j] && !nodes[j].spilled && !nodes[j].simplified) {
            nodes[j].degree++;
        }
    }
}

static int chaitin_briggs_color(IGNode* nodes, int count, int k) {
    if (count == 0) return 0;

    int active = 0;
    for (int i = 0; i < count; i++)
        if (!nodes[i].spilled && !nodes[i].simplified)
            active++;

    IGNode** stack = malloc((size_t)count * sizeof(IGNode*));
    int stack_top = 0;
    if (!stack) OUT_OF_MEMORY();

    bool progress;
    do {
        progress = false;
        for (int i = 0; i < count; i++) {
            IGNode* node = &nodes[i];
            if (node->spilled || node->simplified) continue;
            if (node->degree >= k) continue;

            remove_node_from_graph(node, nodes, count);
            node->simplified = true;
            stack[stack_top++] = node;
            progress = true;
            break;
        }
    } while (progress);

    int remaining = 0;
    for (int i = 0; i < count; i++)
        if (!nodes[i].spilled && !nodes[i].simplified) remaining++;

    int spilled_count = 0;

    if (remaining > 0) {
        int best_i = -1;
        double best_ratio = 1e100;

        for (int i = 0; i < count; i++) {
            IGNode* node = &nodes[i];
            if (node->spilled || node->simplified) continue;

            if (node->is_float) {
                best_i = i;
                break;
            }

            double ratio = node->spill_cost / (double)(node->degree + 1);
            if (ratio < best_ratio) {
                best_ratio = ratio;
                best_i = i;
            }
        }

        if (best_i >= 0) {
            IGNode* spill_node = &nodes[best_i];
            remove_node_from_graph(spill_node, nodes, count);
            spill_node->spilled = true;
            spill_node->color = R_NONE;
            spilled_count = 1;

            int sub_spilled = chaitin_briggs_color(nodes, count, k);
            spilled_count += sub_spilled;
        }
    }

    while (stack_top > 0) {
        IGNode* node = stack[--stack_top];
        node->simplified = false;
        restore_node_to_graph(node, nodes, count);

        bool used[NUM_REGS] = { false };
        for (int j = 0; j < count; j++) {
            if (node->adj[j] && !nodes[j].spilled && nodes[j].color != R_NONE) {
                for (int r = 0; r < k; r++) {
                    if (AVAILABLE_REGS[r] == nodes[j].color) {
                        used[r] = true;
                        break;
                    }
                }
            }
        }

        int best_reg  = -1;
        int best_bias = -1;
        for (int r = 0; r < k; r++) {
            if (used[r]) continue;
            int b = node->bias[AVAILABLE_REGS[r]];
            if (b > best_bias || (b == best_bias && best_reg < 0)) {
                best_bias = b;
                best_reg  = r;
            }
        }

        if (best_reg >= 0) {
            node->color = AVAILABLE_REGS[best_reg];
        } else {
            node->color    = R_NONE;
            node->spilled  = true;
            spilled_count++;
        }
    }

    free(stack);
    return spilled_count;
}

static void write_results_to_regmap(IGNode* nodes, int count) {
    int slot = 4;
    for (int i = 0; i < count; i++) {
        IGNode* node = &nodes[i];
        if (node->spilled) {
            node->color      = R_NONE;
            node->spill_slot = slot;
            slot += 4;
        } else {
            node->spill_slot = 0;
        }

        regmap_add(g_regmap, node->var_name, node->color, node->is_float,
                   node->spill_slot);
    }
}

static void print_results(IGNode* nodes, int count, int k, int spilled) {
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────┐\n");
    printf("│      Chaitin-Briggs 图着色 + Bias 寄存器分配                 │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    printf("│ 变量: %-4d  寄存器: %-2d  溢出: %-3d (%.1f%%)                     │\n",
           count, k, spilled,
           count > 0 ? 100.0 * (double)spilled / (double)count : 0.0);
    printf("├──────┬──────────────┬──────────┬────────┬────────┬──────────┤\n");
    printf("│ 序号 │ 变量         │ 寄存器   │ 溢出槽  │ 循环深 │ 代价     │\n");
    printf("├──────┼──────────────┼──────────┼────────┼────────┼──────────┤\n");

    for (int i = 0; i < count; i++) {
        IGNode* node = &nodes[i];
        const char* reg_str = (node->color == R_NONE) ? "[溢出]"
                              : REG_NAMES[node->color];

        char name[15];
        int nl = (int)strlen(node->var_name);
        if (nl > 13) { memcpy(name, node->var_name, 13); name[13] = '\0'; }
        else { strcpy(name, node->var_name); if (nl < 13) memset(name + nl, ' ', 13 - nl); name[13] = '\0'; }

        printf("│ %4d │ %-12s │ %-8s │ ebp-%-3d │ %-6d │ %-8.0f │\n",
               i + 1, name, reg_str,
               node->spill_slot > 0 ? node->spill_slot : 0,
               node->loop_depth, node->spill_cost);

        for (int r = 0; r < NUM_REGS; r++) {
            if (node->bias[r] >= BIAS_HARD_DIV) {
                printf("│      │  └─ bias: %-4s=%-2d                             │\n",
                       REG_NAMES[r], node->bias[r]);
            }
        }
    }
    printf("└──────┴──────────────┴──────────┴────────┴────────┴──────────┘\n\n");
}

void regmap_allocate_from_cfg(void* cfg_ptr, int num_regs) {
    CFG* cfg = (CFG*)cfg_ptr;
    if (!cfg) return;

    if (!g_regmap) regmap_init();
    g_regmap->count = 0;
    g_regmap->callee_saved_mask = 0;

    int k = (num_regs <= 0 || num_regs > NUM_REGS) ? NUM_REGS : num_regs;

    compute_live_intervals(cfg);

    int* block_depths = calloc((size_t)cfg->block_count, sizeof(int));
    if (block_depths) compute_loop_depths(cfg, block_depths);

    int count = 0;
    IGNode* nodes = build_interference_graph(cfg, &count);
    if (!nodes || count == 0) { free(nodes); free(block_depths); return; }

    compute_spill_costs(cfg, nodes, count, block_depths);

    compute_bias_scores(cfg, nodes, count);

    int spilled = chaitin_briggs_color(nodes, count, k);

    write_results_to_regmap(nodes, count);

    print_results(nodes, count, k, spilled);

    for (int i = 0; i < count; i++) free(nodes[i].adj);
    free(nodes);
    free(block_depths);
}

void regmap_init(void) {
    if (g_regmap) regmap_cleanup();
    g_regmap = calloc(1, sizeof(RegMap));
    if (!g_regmap) OUT_OF_MEMORY();
    g_regmap->capacity = 256;
    g_regmap->names    = calloc((size_t)g_regmap->capacity, sizeof(char*));
    g_regmap->entries  = calloc((size_t)g_regmap->capacity, sizeof(RegMapEntry));
    if (!g_regmap->names || !g_regmap->entries) OUT_OF_MEMORY();
}

void regmap_cleanup(void) {
    if (!g_regmap) return;
    for (int i = 0; i < g_regmap->count; i++) free(g_regmap->names[i]);
    free(g_regmap->names);
    free(g_regmap->entries);
    free(g_regmap);
    g_regmap = NULL;
}

void regmap_add(RegMap* map, const char* name, Reg reg,
                bool is_float, int spill_slot) {
    if (!map || !name) return;
    if (map->count >= map->capacity) {
        map->capacity *= 2;
        map->names   = realloc(map->names, (size_t)map->capacity * sizeof(char*));
        map->entries = realloc(map->entries, (size_t)map->capacity * sizeof(RegMapEntry));
        if (!map->names || !map->entries) OUT_OF_MEMORY();
    }
    map->names[map->count] = strdup(name);
    map->entries[map->count].reg        = reg;
    map->entries[map->count].is_float   = is_float;
    map->entries[map->count].spill_slot = spill_slot;

    if (reg == R_EBX) map->callee_saved_mask |= CALLEE_SAVED_EBX;
    if (reg == R_ESI) map->callee_saved_mask |= CALLEE_SAVED_ESI;
    if (reg == R_EDI) map->callee_saved_mask |= CALLEE_SAVED_EDI;

    map->count++;
}

const RegMapEntry* regmap_get(const RegMap* map, const char* name) {
    if (!map || !name) return NULL;
    for (int i = 0; i < map->count; i++)
        if (strcmp(map->names[i], name) == 0)
            return &map->entries[i];
    return NULL;
}

unsigned regmap_callee_saved_mask(const RegMap* map) {
    return map ? map->callee_saved_mask : 0;
}