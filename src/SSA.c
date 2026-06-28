#include "SSA.h"
#include "Error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

char *strndup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *new = malloc(len + 1);
    if (new == NULL) return NULL;
    memcpy(new, s, len);
    new[len] = '\0';
    return new;
}

typedef struct {
    uint64_t* words;
    int       num_words;
    int       total_bits;
} BitSet;

static BitSet bitset_create(int num_bits) {
    BitSet bs;
    bs.num_words = (num_bits + 63) / 64;
    bs.total_bits = num_bits;
    bs.words = calloc((size_t)bs.num_words, sizeof(uint64_t));
    if (!bs.words) OUT_OF_MEMORY();
    return bs;
}

static void bitset_destroy(BitSet* bs) {
    if (bs && bs->words) { free(bs->words); bs->words = NULL; }
}

static void bitset_set(BitSet* bs, int bit) {
    if (bit >= 0 && bit < bs->total_bits)
        bs->words[bit / 64] |= (1ULL << (bit % 64));
}

static bool bitset_test(const BitSet* bs, int bit) {
    if (bit < 0 || bit >= bs->total_bits) return false;
    return (bs->words[bit / 64] & (1ULL << (bit % 64))) != 0;
}

static void bitset_clear(BitSet* bs, int bit) {
    if (bit >= 0 && bit < bs->total_bits)
        bs->words[bit / 64] &= ~(1ULL << (bit % 64));
}

static void bitset_clear_all(BitSet* bs) {
    memset(bs->words, 0, (size_t)bs->num_words * sizeof(uint64_t));
}

static void bitset_or(BitSet* dst, const BitSet* src) {
    int n = (dst->num_words < src->num_words) ? dst->num_words : src->num_words;
    for (int i = 0; i < n; i++) dst->words[i] |= src->words[i];
}

static bool bitset_equals(const BitSet* a, const BitSet* b) {
    int n = (a->num_words < b->num_words) ? a->num_words : b->num_words;
    for (int i = 0; i < n; i++)
        if (a->words[i] != b->words[i]) return false;
    if (a->num_words > b->num_words) {
        for (int i = n; i < a->num_words; i++)
            if (a->words[i] != 0) return false;
    } else {
        for (int i = n; i < b->num_words; i++)
            if (b->words[i] != 0) return false;
    }
    return true;
}

static char* strip_all_ssa_suffixes(const char* name) {
    if (!name) return NULL;

    const char* p = name + strlen(name);

    while (p > name && isdigit((unsigned char)*(p - 1)))
        p--;

    if (p > name && *(p - 1) == '@') {
        size_t len = (size_t)(p - 1 - name);
        char* inner = strndup(name, len);
        char* result = strip_all_ssa_suffixes(inner);
        free(inner);
        return result;
    }

    return strdup(name);
}

static void bitset_copy(BitSet* dst, const BitSet* src) {
    int n = (dst->num_words < src->num_words) ? dst->num_words : src->num_words;
    for (int i = 0; i < n; i++) dst->words[i] = src->words[i];
    for (int i = n; i < dst->num_words; i++) dst->words[i] = 0;
}

static void bitset_union_df(BitSet* dst, const CFG* cfg, const BitSet* src) {
    for (int bi = 0; bi < cfg->block_count; bi++) {
        if (bitset_test(src, bi)) {
            BasicBlock* bb = cfg->blocks[bi];
            for (int d = 0; d < bb->df_count; d++) {
                bitset_set(dst, bb->df[d]->id);
            }
        }
    }
}

typedef struct {
    char** entries;
    int    top;
    int    capacity;
} RenameStack;

static void stack_init(RenameStack* s) {
    s->capacity = 8;
    s->top = 0;
    s->entries = malloc(sizeof(char*) * (size_t)s->capacity);
    if (!s->entries) OUT_OF_MEMORY();
}

static void stack_push(RenameStack* s, const char* ssa_name) {
    if (s->top >= s->capacity) {
        s->capacity *= 2;
        s->entries = realloc(s->entries, sizeof(char*) * (size_t)s->capacity);
        if (!s->entries) OUT_OF_MEMORY();
    }
    s->entries[s->top++] = strdup(ssa_name);
}

static char* stack_top(const RenameStack* s) {
    return (s->top > 0) ? s->entries[s->top - 1] : NULL;
}

static void stack_pop(RenameStack* s) {
    if (s->top > 0) {
        free(s->entries[s->top - 1]);
        s->entries[s->top - 1] = NULL;
        s->top--;
    }
}

static void stack_destroy(RenameStack* s) {
    for (int i = 0; i < s->top; i++) free(s->entries[i]);
    free(s->entries);
    s->entries = NULL;
    s->capacity = 0;
    s->top = 0;
}

#define VAR_TABLE_CAPACITY 512

typedef struct {
    char*        orig_name;
    RenameStack  stack;
    int          version_count;
    DataType     data_type;
} VarSSAEntry;

typedef struct {
    VarSSAEntry* entries;
    int          count;
    int          capacity;
} VarSSATable;

static VarSSATable* vartable_create(void) {
    VarSSATable* vt = calloc(1, sizeof(VarSSATable));
    if (!vt) OUT_OF_MEMORY();
    vt->capacity = VAR_TABLE_CAPACITY;
    vt->entries = calloc((size_t)vt->capacity, sizeof(VarSSAEntry));
    if (!vt->entries) OUT_OF_MEMORY();
    return vt;
}

static void vartable_destroy(VarSSATable* vt) {
    if (!vt) return;
    for (int i = 0; i < vt->count; i++) {
        free(vt->entries[i].orig_name);
        stack_destroy(&vt->entries[i].stack);
    }
    free(vt->entries);
    free(vt);
}

static int vartable_find(VarSSATable* vt, const char* orig_name) {
    if (!orig_name) return -1;
    for (int i = 0; i < vt->count; i++)
        if (strcmp(vt->entries[i].orig_name, orig_name) == 0)
            return i;
    return -1;
}

static int vartable_get_or_create(VarSSATable* vt, const char* orig_name) {
    /* 先剥离 @N 后缀 */
    char* base = strip_all_ssa_suffixes(orig_name);
    int idx = vartable_find(vt, base);
    if (idx >= 0) { free(base); return idx; }

    idx = vartable_find(vt, base);
    if (idx >= 0) { free(base); return idx; }

    if (vt->count >= vt->capacity) {
        vt->capacity *= 2;
        vt->entries = realloc(vt->entries, sizeof(VarSSAEntry) * (size_t)vt->capacity);
        if (!vt->entries) OUT_OF_MEMORY();
        memset(&vt->entries[vt->count], 0,
               sizeof(VarSSAEntry) * (size_t)(vt->capacity - vt->count));
    }

    idx = vt->count++;
    vt->entries[idx].orig_name = base;
    stack_init(&vt->entries[idx].stack);
    vt->entries[idx].version_count = 0;
    vt->entries[idx].data_type = DATA_INT;
    return idx;
}

static char* make_ssa_name(const char* orig_name, int version) {
    int len = (int)strlen(orig_name) + 12;
    char* buf = malloc((size_t)len);
    if (!buf) OUT_OF_MEMORY();
    snprintf(buf, len, "%s@%d", orig_name, version);
    return buf;
}

static bool looks_like_number(const char* s) {
    if (!s || !*s) return false;
    if (*s == '-') s++;
    if (!*s) return false;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!*s || !isxdigit((unsigned char)*s)) return false;
        while (*s && isxdigit((unsigned char)*s)) s++;
        return *s == '\0';
    }

    if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        s += 2;
        if (!*s || (*s != '0' && *s != '1')) return false;
        while (*s == '0' || *s == '1') s++;
        return *s == '\0';
    }

    if (s[0] == '0' && s[1] != '\0' && s[1] != 'x' && s[1] != 'X'
        && s[1] != 'b' && s[1] != 'B' && s[1] != '.') {
        s++;
        while (*s >= '0' && *s <= '7') s++;
        return *s == '\0';
    }

    bool has_digit = false;
    bool has_dot   = false;
    while (*s) {
        if (isdigit((unsigned char)*s)) {
            has_digit = true;
        } else if (*s == '.' && !has_dot) {
            has_dot = true;
        } else {
            return false;
        }
        s++;
    }
    return has_digit;
}

static bool looks_like_label(const char* s) {
    if (!s || !*s) return false;
    if (s[0] == '_' || s[0] == '.') return false;
    return (s[0] >= 'A' && s[0] <= 'Z');
}

static bool looks_like_bool(const char* s) {
    if (!s) return false;
    return (strcmp(s, "false") == 0 || strcmp(s, "true") == 0 ||
            strcmp(s, "False") == 0 || strcmp(s, "True") == 0);
}

static bool needs_ssa_treatment(const char* s) {
    if (!s || !*s) return false;

    if (s[0] == '.') {
        if ((s[1] == 't' || s[1] == 'f' || s[1] == 'd')
            && isdigit((unsigned char)s[2]))
            return false;
    }
    char* base = strip_all_ssa_suffixes(s);

    bool is_number = looks_like_number(base);

    bool is_label = looks_like_label(base);

    bool is_bool = looks_like_bool(base);
    bool result = !is_number && !is_label && !is_bool;
    free(base);
    return result;
}

static bool block_has_phi_for(const BasicBlock* bb, const char* orig_name) {
    IR* ir = bb->first_ir;
    while (ir) {
        if (ir->type == IR_PHI && ir->result) {
            const char* at = strrchr(ir->result, '@');
            if (at) {
                size_t len = (size_t)(at - ir->result);
                if (strncmp(ir->result, orig_name, len) == 0
                    && orig_name[len] == '\0')
                    return true;
            } else {
                if (strcmp(ir->result, orig_name) == 0)
                    return true;
            }
        } else if (ir->type != IR_LABEL && ir->type != IR_PARAM_DEF) {
            break;
        }
        if (ir == bb->last_ir) break;
        ir = ir->next;
    }
    return false;
}

static IR* insert_phi_node(BasicBlock* bb, const char* orig_name,
                           int pred_count, char** pred_labels,
                           DataType dtype) {
    IR* phi = calloc(1, sizeof(IR));
    if (!phi) OUT_OF_MEMORY();
    phi->type = IR_PHI;
    phi->result = strdup(orig_name);
    phi->data_type = dtype;
    phi->phi_arg_count = pred_count;
    phi->label_count = pred_count;

    if (pred_count > 0) {
        phi->label = calloc((size_t)pred_count, sizeof(char*));
        phi->phi_arg_names = calloc((size_t)pred_count, sizeof(char*));
        for (int i = 0; i < pred_count; i++) {
            if (pred_labels && pred_labels[i])
                phi->label[i] = strdup(pred_labels[i]);
            else
                phi->label[i] = strdup("?");
            phi->phi_arg_names[i] = strdup(orig_name);
        }
    }

    assign_def_id(phi);

    IR* insert_after = bb->first_ir;
    if (insert_after && insert_after->type == IR_LABEL) {
        while (insert_after->next &&
               insert_after->next->type == IR_PARAM_DEF) {
            insert_after = insert_after->next;
        }
    }

    if (insert_after == NULL) {
        bb->first_ir = phi;
        bb->last_ir = phi;
    } else {
        phi->next = insert_after->next;
        insert_after->next = phi;
        if (bb->last_ir == insert_after)
            bb->last_ir = phi;
    }

    bb->ir_count++;
    return phi;
}

static int insert_phi_nodes(CFG* cfg, VarSSATable* vt) {
    int n = cfg->block_count;
    if (n == 0) return 0;

    BitSet* def_blocks = malloc(sizeof(BitSet) * (size_t)vt->count);
    for (int vi = 0; vi < vt->count; vi++)
        def_blocks[vi] = bitset_create(n);

    for (int bi = 0; bi < n; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->result && needs_ssa_treatment(ir->result)) {
                int vi = vartable_get_or_create(vt, ir->result);
                bitset_set(&def_blocks[vi], bi);
            }
            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    int phi_inserted = 0;

    for (int vi = 0; vi < vt->count; vi++) {
        BitSet W = bitset_create(n);
        BitSet already = bitset_create(n);
        BitSet new_W = bitset_create(n);

        bitset_copy(&W, &def_blocks[vi]);

        while (true) {
            bitset_clear_all(&new_W);

            for (int bi = 0; bi < n; bi++) {
                if (!bitset_test(&W, bi)) continue;

                BasicBlock* bb = cfg->blocks[bi];
                for (int d = 0; d < bb->df_count; d++) {
                    int did = bb->df[d]->id;
                    if (!bitset_test(&already, did)) {
                        if (!block_has_phi_for(cfg->blocks[did],
                                               vt->entries[vi].orig_name)) {
                            char** pred_labels = malloc(
                                sizeof(char*) * (size_t)cfg->blocks[did]->pred_count);
                            for (int p = 0; p < cfg->blocks[did]->pred_count; p++) {
                                pred_labels[p] = strdup("?");
                            }

                            insert_phi_node(cfg->blocks[did],
                                            vt->entries[vi].orig_name,
                                            cfg->blocks[did]->pred_count,
                                            pred_labels,
                                            vt->entries[vi].data_type);

                            for (int p = 0; p < cfg->blocks[did]->pred_count; p++)
                                free(pred_labels[p]);
                            free(pred_labels);

                            bitset_set(&already, did);
                            bitset_set(&new_W, did);
                            phi_inserted++;
                        }
                    }
                }
            }

            bool any_new = false;
            for (int i = 0; i < new_W.num_words; i++)
                if (new_W.words[i] != 0) { any_new = true; break; }
            if (!any_new) break;

            bitset_copy(&W, &new_W);
        }

        bitset_destroy(&W);
        bitset_destroy(&already);
        bitset_destroy(&new_W);
    }

    for (int vi = 0; vi < vt->count; vi++)
        bitset_destroy(&def_blocks[vi]);
    free(def_blocks);

    return phi_inserted;
}

static const char* get_current_ssa_name(VarSSATable* vt, const char* orig_name) {
    if (!orig_name || !needs_ssa_treatment(orig_name)) return orig_name;

    char* base = strip_all_ssa_suffixes(orig_name);
    int idx = vartable_find(vt, base);

    if (idx < 0) {
        free(base);
        return orig_name;
    }

    const char* top = stack_top(&vt->entries[idx].stack);
    free(base);
    if (top) return top;

    return orig_name;
}

static void rename_ir_operands(IR* ir, VarSSATable* vt) {
    if (ir->args1 && needs_ssa_treatment(ir->args1)) {
        const char* ssa = get_current_ssa_name(vt, ir->args1);
        if (ssa && strcmp(ir->args1, ssa) != 0) {
            free(ir->args1);
            ir->args1 = strdup(ssa);
        }
    }
    if (ir->args2 && needs_ssa_treatment(ir->args2)) {
        const char* ssa = get_current_ssa_name(vt, ir->args2);
        if (ssa && strcmp(ir->args2, ssa) != 0) {
            free(ir->args2);
            ir->args2 = strdup(ssa);
        }
    }
}

static const char* create_new_ssa_version(VarSSATable* vt, const char* orig_name) {
    if (!orig_name || !needs_ssa_treatment(orig_name)) return orig_name;

    char* base = strip_all_ssa_suffixes(orig_name);
    int idx = vartable_get_or_create(vt, base);
    int ver = vt->entries[idx].version_count++;
    char* ssa = make_ssa_name(base, ver);
    stack_push(&vt->entries[idx].stack, ssa);
    free(ssa);
    free(base);
    return stack_top(&vt->entries[idx].stack);
}

static void rename_block(BasicBlock* bb, CFG* cfg, VarSSATable* vt) {
    #define MAX_PUSHED 256
    int pushed_indices[MAX_PUSHED];
    int pushed_count = 0;

    IR* ir = bb->first_ir;
    while (ir) {
        if (ir->type == IR_PHI && ir->result) {
            const char* orig = ir->result;
            const char* new_ssa = create_new_ssa_version(vt, orig);
            free(ir->result);
            ir->result = strdup(new_ssa);

            int idx = vartable_find(vt, orig);
            if (idx >= 0 && pushed_count < MAX_PUSHED)
                pushed_indices[pushed_count++] = idx;
        } else if (ir->type != IR_LABEL && ir->type != IR_PARAM_DEF) {
            break;
        }
        ir = ir->next;
    }

    while (ir) {
        switch (ir->type) {
        case IR_PHI:
            break;

        case IR_JMP:
        case IR_LABEL:
            break;

        case IR_PARAM_DEF:
            if (ir->result && needs_ssa_treatment(ir->result)) {
                const char* new_ssa = create_new_ssa_version(vt, ir->result);
                free(ir->result);
                ir->result = strdup(new_ssa);
                int idx = vartable_find(vt,
                    strip_all_ssa_suffixes(ir->result));
                if (idx >= 0 && pushed_count < MAX_PUSHED)
                    pushed_indices[pushed_count++] = idx;
            }
            break;

        case IR_JMPZ:
        case IR_JMPNZ:
            if (ir->args1 && needs_ssa_treatment(ir->args1)) {
                const char* ssa = get_current_ssa_name(vt, ir->args1);
                if (strcmp(ir->args1, ssa) != 0) {
                    free(ir->args1);
                    ir->args1 = strdup(ssa);
                }
            }
            break;

        default:
            rename_ir_operands(ir, vt);
            if (ir->result && needs_ssa_treatment(ir->result)) {
                bool should_rename = true;
                {
                    char* base = strip_all_ssa_suffixes(ir->result);
                    int idx = vartable_find(vt, base);
                    if (idx < 0 || vt->entries[idx].stack.top == 0)
                        should_rename = false;
                    free(base);
                }
                if (should_rename) {
                    const char* new_ssa = create_new_ssa_version(vt, ir->result);
                    free(ir->result);
                    ir->result = strdup(new_ssa);
                    int idx = vartable_find(vt, ir->result);
                    const char* at = strrchr(ir->result, '@');
                    if (at) {
                        size_t len = (size_t)(at - ir->result);
                        char* orig = strndup(ir->result, len);
                        idx = vartable_find(vt, orig);
                        free(orig);
                    }
                    if (idx >= 0 && pushed_count < MAX_PUSHED)
                        pushed_indices[pushed_count++] = idx;
                }
            }
            break;
        }

        if (ir == bb->last_ir) break;
        ir = ir->next;
    }

    for (int s = 0; s < bb->succ_count; s++) {
        BasicBlock* succ = bb->succs[s];
        int pred_idx = -1;
        for (int p = 0; p < succ->pred_count; p++) {
            if (succ->preds[p] == bb) { pred_idx = p; break; }
        }
        if (pred_idx < 0) continue;

        IR* sir = succ->first_ir;

        while (sir && (sir->type == IR_LABEL || sir->type == IR_PARAM_DEF)) {
            sir = sir->next;
        }

        while (sir) {
            if (sir->type == IR_PHI) {
                if (pred_idx < sir->phi_arg_count) {
                    const char* orig = sir->result;
                    const char* at = strrchr(orig, '@');
                    char* base_name = NULL;
                    if (at) {
                        base_name = strndup(orig, (size_t)(at - orig));
                    } else {
                        base_name = strdup(orig);
                    }

                    const char* cur_ssa = get_current_ssa_name(vt, base_name);
                    if (!cur_ssa) {
                        cur_ssa = base_name;
                    }

                    if (sir->phi_arg_names[pred_idx])
                        free(sir->phi_arg_names[pred_idx]);
                    sir->phi_arg_names[pred_idx] = strdup(cur_ssa ? cur_ssa : "?");

                    free(base_name);
                }
            } else {
                break;
            }
            sir = sir->next;
        }
    }

    for (int c = 0; c < bb->dom_child_count; c++) {
        rename_block(bb->dom_children[c], cfg, vt);
    }

    for (int i = 0; i < pushed_count; i++) {
        stack_pop(&vt->entries[pushed_indices[i]].stack);
    }
}

bool ssa_construct(CFG* cfg) {
    if (!cfg || cfg->block_count == 0) return false;

    cfg_compute_dominators(cfg);
    cfg_compute_dominance_frontiers(cfg);

    VarSSATable* vt = vartable_create();

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->result && needs_ssa_treatment(ir->result))
                vartable_get_or_create(vt, ir->result);

            if (ir->result && ir->data_type != DATA_ERROR) {
                int idx = vartable_find(vt, ir->result);
                if (idx >= 0 && vt->entries[idx].data_type == DATA_INT)
                    vt->entries[idx].data_type = ir->data_type;
            }

            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    bool* is_local = calloc((size_t)vt->count, sizeof(bool));
    if (!is_local) OUT_OF_MEMORY();
    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->result && needs_ssa_treatment(ir->result)) {
                if (ir->type == IR_VAR || ir->type == IR_VAR_UNINIT ||
                    ir->type == IR_ARRAY_DECL || ir->type == IR_PARAM_DEF) {
                    int idx = vartable_find(vt, ir->result);
                    if (idx >= 0) is_local[idx] = true;
                    }
            }
            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }
    for (int i = 0; i < vt->count; i++) {
        if (vt->entries[i].orig_name && vt->entries[i].orig_name[0] == '.')
            is_local[i] = true;
    }
    for (int i = 0; i < vt->count; i++) {
        if (!is_local[i]) {
            stack_destroy(&vt->entries[i].stack);
            stack_init(&vt->entries[i].stack);
            vt->entries[i].version_count = 0;
        }
    }
    free(is_local);

    insert_phi_nodes(cfg, vt);

    if (cfg->entry) {
        rename_block(cfg->entry, cfg, vt);
    }

    for (int i = 0; i < cfg->var_count; i++)
        free(cfg->var_names[i]);
    free(cfg->var_names);
    cfg->var_names = NULL;
    cfg->var_count = 0;
    cfg->var_capacity = 0;

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];

        free(bb->def_names);
        bb->def_names = NULL;
        bb->def_count = 0;
        bb->def_capacity = 0;
        free(bb->use_names);
        bb->use_names = NULL;
        bb->use_count = 0;
        bb->use_capacity = 0;

        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->result && needs_ssa_treatment(ir->result)) {
                if (bb->def_count >= bb->def_capacity) {
                    bb->def_capacity = bb->def_capacity ? bb->def_capacity * 2 : 16;
                    bb->def_names = realloc(bb->def_names,
                        sizeof(char*) * (size_t)bb->def_capacity);
                }
                bb->def_names[bb->def_count++] = strdup(ir->result);

                bool found = false;
                for (int j = 0; j < cfg->var_count; j++)
                    if (strcmp(cfg->var_names[j], ir->result) == 0)
                    { found = true; break; }
                if (!found) {
                    if (cfg->var_count >= cfg->var_capacity) {
                        cfg->var_capacity = cfg->var_capacity ? cfg->var_capacity * 2 : 64;
                        cfg->var_names = realloc(cfg->var_names,
                            sizeof(char*) * (size_t)cfg->var_capacity);
                    }
                    cfg->var_names[cfg->var_count++] = strdup(ir->result);
                }
            }

            if (ir->args1 && needs_ssa_treatment(ir->args1)
                && ir->type != IR_JMP && ir->type != IR_LABEL) {
                if (bb->use_count >= bb->use_capacity) {
                    bb->use_capacity = bb->use_capacity ? bb->use_capacity * 2 : 16;
                    bb->use_names = realloc(bb->use_names,
                        sizeof(char*) * (size_t)bb->use_capacity);
                }
                bb->use_names[bb->use_count++] = strdup(ir->args1);

                bool found = false;
                for (int j = 0; j < cfg->var_count; j++)
                    if (strcmp(cfg->var_names[j], ir->args1) == 0)
                    { found = true; break; }
                if (!found) {
                    if (cfg->var_count >= cfg->var_capacity) {
                        cfg->var_capacity = cfg->var_capacity ? cfg->var_capacity * 2 : 64;
                        cfg->var_names = realloc(cfg->var_names,
                            sizeof(char*) * (size_t)cfg->var_capacity);
                    }
                    cfg->var_names[cfg->var_count++] = strdup(ir->args1);
                }
            }

            if (ir->args2 && needs_ssa_treatment(ir->args2)
                && ir->type != IR_JMPZ && ir->type != IR_JMPNZ) {
                if (bb->use_count >= bb->use_capacity) {
                    bb->use_capacity = bb->use_capacity ? bb->use_capacity * 2 : 16;
                    bb->use_names = realloc(bb->use_names,
                        sizeof(char*) * (size_t)bb->use_capacity);
                }
                bb->use_names[bb->use_count++] = strdup(ir->args2);

                bool found = false;
                for (int j = 0; j < cfg->var_count; j++)
                    if (strcmp(cfg->var_names[j], ir->args2) == 0)
                    { found = true; break; }
                if (!found) {
                    if (cfg->var_count >= cfg->var_capacity) {
                        cfg->var_capacity = cfg->var_capacity ? cfg->var_capacity * 2 : 64;
                        cfg->var_names = realloc(cfg->var_names,
                            sizeof(char*) * (size_t)cfg->var_capacity);
                    }
                    cfg->var_names[cfg->var_count++] = strdup(ir->args2);
                }
            }

            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    vartable_destroy(vt);

    cfg_assign_linear_ids(cfg);

    return true;
}

IR* ssa_destruct(CFG* cfg) {
    if (!cfg || cfg->block_count == 0) return NULL;

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        if (!bb || !bb->first_ir) continue;

        IR* prev = NULL;
        IR* ir = bb->first_ir;
        while (ir && (ir->type == IR_LABEL || ir->type == IR_PARAM_DEF)) {
            prev = ir;
            ir = ir->next;
        }

        while (ir && ir->type == IR_PHI) {
            for (int p = 0; p < ir->phi_arg_count && p < bb->pred_count; p++) {
                if (!ir->phi_arg_names[p]) continue;

                BasicBlock* pred = bb->preds[p];
                char* src_name = ir->phi_arg_names[p];
                char* dst_name = ir->result;

                if (strcmp(src_name, dst_name) == 0) continue;

                IR* copy = create_IR(IR_VAR_ASS);
                copy->result = strdup(dst_name);
                copy->args1 = strdup(src_name);
                copy->args1_type = ir->data_type;
                copy->data_type = ir->data_type;
                assign_def_id(copy);

                IR* pred_last = pred->last_ir;
                int is_term = (pred_last &&
                    (pred_last->type == IR_JMP ||
                     pred_last->type == IR_JMPZ ||
                     pred_last->type == IR_JMPNZ ||
                     pred_last->type == IR_RET));

                if (is_term) {
                    IR* scan = pred->first_ir;
                    IR* prev_to_term = NULL;
                    while (scan && scan != pred_last) {
                        if (scan->next == pred_last) {
                            prev_to_term = scan;
                            break;
                        }
                        scan = scan->next;
                    }
                    if (prev_to_term) {
                        copy->next = prev_to_term->next;
                        prev_to_term->next = copy;
                    } else {
                        copy->next = pred->first_ir;
                        pred->first_ir = copy;
                    }
                } else {
                    if (pred_last) {
                        copy->next = pred_last->next;
                        pred_last->next = copy;
                        pred->last_ir = copy;
                    } else {
                        pred->first_ir = pred->last_ir = copy;
                    }
                }
                pred->ir_count++;
            }

            IR* next_ir = ir->next;
            if (prev) {
                prev->next = next_ir;
            } else {
                bb->first_ir = next_ir;
            }
            if (bb->last_ir == ir) bb->last_ir = prev;

            if (ir->label) {
                for (int i = 0; i < ir->label_count; i++) free(ir->label[i]);
                free(ir->label);
            }
            if (ir->phi_arg_names) {
                for (int i = 0; i < ir->phi_arg_count; i++)
                    free(ir->phi_arg_names[i]);
                free(ir->phi_arg_names);
            }
            free(ir->result);
            free(ir);
            bb->ir_count--;
            ir = next_ir;
        }
    }

    IR* linear_head = NULL;
    IR* linear_tail = NULL;

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        if (!bb || !bb->first_ir) continue;

        IR* ir = bb->first_ir;
        int collected = 0;
        while (ir) {
            collected++;
            IR* next_ir = ir->next;
            ir->next = NULL;

            if (!linear_head) {
                linear_head = linear_tail = ir;
            } else {
                linear_tail->next = ir;
                linear_tail = ir;
            }

            if (ir == bb->last_ir) break;
            ir = next_ir;
        }

        if (collected != bb->ir_count) {
            bb->ir_count = collected;
        }
    }

    if (cfg->blocks[0]) {
        cfg->blocks[0]->first_ir = linear_head;
        for (int bi = cfg->block_count - 1; bi >= 0; bi--) {
            if (cfg->blocks[bi] && cfg->blocks[bi]->first_ir) {
                cfg->blocks[bi]->last_ir = linear_tail;
                break;
            }
        }
    }

    cfg_liveness_analysis(cfg);
    cfg_assign_linear_ids(cfg);

    return linear_head;
}

static bool ir_has_side_effects(const IR* ir) {
    switch (ir->type) {
    case IR_PRINT:
    case IR_INPUT:
    case IR_FORMAT:
    case IR_CALL:
    case IR_RET:
    case IR_JMP:
    case IR_JMPZ:
    case IR_JMPNZ:
    case IR_ARRAY_STORE:
    case IR_VAR_ASS:
    case IR_PUSH:
    case IR_BOUNDS_CHECK:
    case IR_ALLOC_ARRAY:
    case IR_ALLOC_CLASS:
    case IR_LABEL:
    case IR_PARAM_DEF:
    case IR_ARRAY_DECL:
    case IR_VAR:
    case IR_VAR_UNINIT:
    case IR_ARRAY_LENGTH:
        return true;
    default:
        return false;
    }
}

int ssa_dead_code_elimination(CFG* cfg) {
    if (!cfg) return 0;
    int removed = 0;

    typedef struct { IR* ir; bool live; } IRMark;
    int total_ir = 0, cap = 1024;
    IRMark* marks = malloc(sizeof(IRMark) * (size_t)cap);
    if (!marks) OUT_OF_MEMORY();

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        if (!bb || !bb->first_ir) continue;
        IR* ir = bb->first_ir;
        while (ir) {
            if (total_ir >= cap) {
                cap *= 2;
                marks = realloc(marks, sizeof(IRMark) * (size_t)cap);
                if (!marks) OUT_OF_MEMORY();
            }
            marks[total_ir].ir = ir;
            marks[total_ir].live = ir_has_side_effects(ir);
            total_ir++;
            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    bool changed;
    do {
        changed = false;
        for (int i = 0; i < total_ir; i++) {
            if (!marks[i].live) continue;
            IR* ir = marks[i].ir;
            if (ir->args1 && !looks_like_number(ir->args1) && !looks_like_label(ir->args1)) {
                for (int j = 0; j < total_ir; j++) {
                    if (marks[j].ir->result &&
                        strcmp(marks[j].ir->result, ir->args1) == 0) {
                        if (!marks[j].live) { marks[j].live = true; changed = true; }
                        break;
                    }
                }
            }
            if (ir->args2 && !looks_like_number(ir->args2) && !looks_like_label(ir->args2)) {
                for (int j = 0; j < total_ir; j++) {
                    if (marks[j].ir->result &&
                        strcmp(marks[j].ir->result, ir->args2) == 0) {
                        if (!marks[j].live) { marks[j].live = true; changed = true; }
                        break;
                    }
                }
            }
            if (ir->type == IR_PHI && ir->phi_arg_names) {
                for (int p = 0; p < ir->phi_arg_count; p++) {
                    if (ir->phi_arg_names[p] && !looks_like_number(ir->phi_arg_names[p])
                        && !looks_like_label(ir->phi_arg_names[p])) {
                        for (int j = 0; j < total_ir; j++) {
                            if (marks[j].ir->result &&
                                strcmp(marks[j].ir->result,
                                       ir->phi_arg_names[p]) == 0) {
                                if (!marks[j].live) { marks[j].live = true; changed = true; }
                                break;
                            }
                        }
                    }
                }
            }
        }
    } while (changed);

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        if (!bb || !bb->first_ir) continue;

        IR* prev = NULL;
        IR* ir = bb->first_ir;
        while (ir) {
            bool is_block_end = (ir == bb->last_ir);

            int idx = -1;
            for (int i = 0; i < total_ir; i++)
                if (marks[i].ir == ir) { idx = i; break; }

            IR* next = ir->next;

            if (idx >= 0 && !marks[idx].live) {
                if (prev)
                    prev->next = next;
                else
                    bb->first_ir = next;

                if (bb->last_ir == ir)
                    bb->last_ir = prev;

                ir->next   = NULL;
                ir->args1  = NULL;
                ir->args2  = NULL;
                ir->result = NULL;

                free(ir);
                bb->ir_count--;
                removed++;
            } else {
                prev = ir;
            }

            if (is_block_end)
                break;

            ir = next;
        }
    }

    cfg_assign_linear_ids(cfg);

    free(marks);
    return removed;
}

int ssa_copy_propagation(CFG* cfg) {
    if (!cfg) return 0;

    int propagated = 0;
    int deleted = 0;

    typedef struct {
        char* src_name;
        char* dst_name;
        IR*   ir;
        bool  can_delete;
    } CopyPair;

    int copy_cap = 256, copy_cnt = 0;
    CopyPair* copies = malloc(sizeof(CopyPair) * (size_t)copy_cap);
    if (!copies) OUT_OF_MEMORY();

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;
        while (ir) {
            bool is_copy = false;
            const char* src = NULL;
            const char* dst = NULL;

            if (ir->type == IR_LITERAL && ir->args1 &&
                needs_ssa_treatment(ir->args1) && ir->result) {
                is_copy = true;
                src = ir->args1;
                dst = ir->result;
            } else if (ir->type == IR_VAR_ASS && ir->args1 &&
                       needs_ssa_treatment(ir->args1) && ir->result) {
                is_copy = true;
                src = ir->args1;
                dst = ir->result;
            }

            if (is_copy && src && dst && strcmp(src, dst) != 0) {
                if (copy_cnt >= copy_cap) {
                    copy_cap *= 2;
                    copies = realloc(copies,
                                     sizeof(CopyPair) * (size_t)copy_cap);
                    if (!copies) OUT_OF_MEMORY();
                }
                copies[copy_cnt].src_name = strdup(src);
                copies[copy_cnt].dst_name = strdup(dst);
                copies[copy_cnt].ir = ir;
                copies[copy_cnt].can_delete =
                    !ir_has_side_effects(ir) || ir->type == IR_LITERAL;
                copy_cnt++;
            }

            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    if (copy_cnt == 0) {
        printf("  · 没有可传播的复制\n");
        free(copies);
        return 0;
    }

    for (int ci = 0; ci < copy_cnt; ci++) {
        const char* src = copies[ci].src_name;
        const char* dst = copies[ci].dst_name;

        for (int bi = 0; bi < cfg->block_count; bi++) {
            BasicBlock* bb = cfg->blocks[bi];
            IR* ir = bb->first_ir;
            while (ir) {
                if (ir == copies[ci].ir) {
                    if (ir == bb->last_ir) break;
                    ir = ir->next;
                    continue;
                }

                if (ir->args1 && strcmp(ir->args1, dst) == 0) {
                    free(ir->args1);
                    ir->args1 = strdup(src);
                    propagated++;
                }
                if (ir->args2 && strcmp(ir->args2, dst) == 0) {
                    free(ir->args2);
                    ir->args2 = strdup(src);
                    propagated++;
                }

                if (ir->type == IR_PHI && ir->phi_arg_names) {
                    for (int p = 0; p < ir->phi_arg_count; p++) {
                        if (ir->phi_arg_names[p] &&
                            strcmp(ir->phi_arg_names[p], dst) == 0) {
                            free(ir->phi_arg_names[p]);
                            ir->phi_arg_names[p] = strdup(src);
                            propagated++;
                        }
                    }
                }

                if (ir == bb->last_ir) break;
                ir = ir->next;
            }
        }
    }

    IR** to_delete = malloc(sizeof(IR*) * (size_t)copy_cnt);
    int delete_count = 0;

    for (int ci = 0; ci < copy_cnt; ci++) {
        if (!copies[ci].can_delete) continue;

        bool still_in_list = false;
        for (int bi = 0; bi < cfg->block_count && !still_in_list; bi++) {
            BasicBlock* bb = cfg->blocks[bi];
            if (bb->first_ir == copies[ci].ir) {
                still_in_list = true;
                break;
            }
            IR* ir = bb->first_ir;
            while (ir) {
                if (ir->next == copies[ci].ir) {
                    still_in_list = true;
                    break;
                }
                if (ir == bb->last_ir) break;
                ir = ir->next;
            }
        }

        if (still_in_list) {
            to_delete[delete_count++] = copies[ci].ir;
        }
    }

    for (int d = 0; d < delete_count; d++) {
        IR* dead_ir = to_delete[d];
        for (int bi = 0; bi < cfg->block_count; bi++) {
            BasicBlock* bb = cfg->blocks[bi];
            if (!bb || !bb->first_ir) continue;

            IR* prev = NULL;
            IR* ir = bb->first_ir;
            while (ir) {
                if (ir == dead_ir) {
                    IR* next = ir->next;
                    if (prev) prev->next = next;
                    else bb->first_ir = next;
                    if (bb->last_ir == ir) bb->last_ir = prev;
                    bb->ir_count--;
                    ir->next = NULL;
                    ir->args1  = NULL;
                    ir->args2  = NULL;
                    ir->result = NULL;
                    free(ir);
                    deleted++;
                    break;
                }
                prev = ir;
                if (ir == bb->last_ir) break;
                ir = ir->next;
            }
        }
    }
    free(to_delete);

    for (int i = 0; i < copy_cnt; i++) {
        free(copies[i].src_name);
        free(copies[i].dst_name);
    }
    free(copies);

    return propagated;
}

int ssa_constant_folding(CFG* cfg) {
    if (!cfg) return 0;

    int folded = 0;

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->type == IR_BINARY) {
                bool arg1_const = looks_like_number(ir->args1);
                bool arg2_const = looks_like_number(ir->args2);

                bool arg1_is_float = (strchr(ir->args1, '.') != NULL);
                bool arg2_is_float = (strchr(ir->args2, '.') != NULL);

                if (arg1_is_float != arg2_is_float) {
                    if (ir == bb->last_ir) break;
                    ir = ir->next;
                    continue;
                }

                if (arg1_const && arg2_const) {
                    int val1 = parse_integer(ir->args1);
                    int val2 = parse_integer(ir->args2);
                    int result = 0;
                    bool valid = true;

                    switch (ir->op) {
                    case IR_ADD: result = val1 + val2; break;
                    case IR_SUB: result = val1 - val2; break;
                    case IR_MUL: result = val1 * val2; break;
                    case IR_DIV:
                        if (val2 == 0) { valid = false; break; }
                        result = val1 / val2; break;
                    case IR_MOD:
                        if (val2 == 0) { valid = false; break; }
                        result = val1 % val2; break;
                    case IR_JE:  result = (val1 == val2) ? 1 : 0; break;
                    case IR_JNE: result = (val1 != val2) ? 1 : 0; break;
                    case IR_JL:  result = (val1 <  val2) ? 1 : 0; break;
                    case IR_JG:  result = (val1 >  val2) ? 1 : 0; break;
                    case IR_JLE: result = (val1 <= val2) ? 1 : 0; break;
                    case IR_JGE: result = (val1 >= val2) ? 1 : 0; break;
                    case IR_SHL:
                        if (val2 < 0 || val2 >= 32) { valid = false; break; }
                        result = val1 << val2;
                        break;
                    case IR_SHR:
                        if (val2 < 0 || val2 >= 32) { valid = false; break; }
                        result = (unsigned int)val1 >> val2;
                        break;
                    case IR_SAR:
                        if (val2 < 0 || val2 >= 32) { valid = false; break; }
                        result = val1 >> val2;
                        break;
                    case IR_BITWISE_AND: result = val1 & val2; break;
                    case IR_BITWISE_OR:  result = val1 | val2; break;
                    case IR_BITWISE_XOR: result = val1 ^ val2; break;
                    default: valid = false; break;
                    }

                    if (valid) {
                        ir->type = IR_LITERAL;
                        if (ir->args1) free(ir->args1);
                        if (ir->args2) free(ir->args2);
                        char* new_val = malloc(16);
                        snprintf(new_val, 16, "%d", result);
                        ir->args1 = new_val;
                        ir->args1_type = DATA_INT;
                        ir->data_type = DATA_INT;
                        ir->args2 = NULL;
                        ir->args2_type = DATA_ERROR;
                        folded++;
                    }
                }
            }

            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    return folded;
}

int ssa_constant_propagation(CFG* cfg) {
    if (!cfg) return 0;

    int propagated = 0;

    typedef struct {
        char* name;
        char* const_value;
        bool  is_int;
    } ConstDef;

    int const_cap = 256, const_cnt = 0;
    ConstDef* consts = malloc(sizeof(ConstDef) * (size_t)const_cap);
    if (!consts) OUT_OF_MEMORY();

    for (int bi = 0; bi < cfg->block_count; bi++) {
        BasicBlock* bb = cfg->blocks[bi];
        IR* ir = bb->first_ir;
        while (ir) {
            if (ir->result && ir->type == IR_LITERAL &&
                ir->args1 && looks_like_number(ir->args1) &&
                needs_ssa_treatment(ir->result)) {
                if (const_cnt >= const_cap) {
                    const_cap *= 2;
                    consts = realloc(consts,
                                     sizeof(ConstDef) * (size_t)const_cap);
                    if (!consts) OUT_OF_MEMORY();
                }
                consts[const_cnt].name = strdup(ir->result);
                consts[const_cnt].const_value = strdup(ir->args1);
                consts[const_cnt].is_int = true;
                const_cnt++;
            }
            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
    }

    for (int ci = 0; ci < const_cnt; ci++) {
        for (int bi = 0; bi < cfg->block_count; bi++) {
            BasicBlock* bb = cfg->blocks[bi];
            IR* ir = bb->first_ir;
            while (ir) {
                if (ir->result && strcmp(ir->result, consts[ci].name) == 0) {
                    if (ir == bb->last_ir) break;
                    ir = ir->next;
                    continue;
                }

                if (ir->args1 && strcmp(ir->args1, consts[ci].name) == 0)
                {
                    DataType expected_type = ir->args1_type;
                    bool is_float_use = (expected_type == DATA_FLOAT ||
                                         expected_type == DATA_UFLOAT);
                    bool const_is_int = (strchr(consts[ci].const_value, '.') == NULL &&
                                         strcmp(consts[ci].const_value, "false") != 0 &&
                                         strcmp(consts[ci].const_value, "true") != 0);

                    if (!(is_float_use && const_is_int)) {
                        free(ir->args1);
                        ir->args1 = strdup(consts[ci].const_value);
                        propagated++;
                    }
                }

                if (ir->args2 && strcmp(ir->args2, consts[ci].name) == 0) {
                    DataType expected_type = ir->args2_type;
                    bool is_float_use = (expected_type == DATA_FLOAT ||
                                         expected_type == DATA_UFLOAT);
                    bool const_is_int = (strchr(consts[ci].const_value, '.') == NULL &&
                                         strcmp(consts[ci].const_value, "false") != 0 &&
                                         strcmp(consts[ci].const_value, "true") != 0);

                    if (!(is_float_use && const_is_int)) {
                        free(ir->args2);
                        ir->args2 = strdup(consts[ci].const_value);
                        propagated++;
                    }
                }

                if (ir->type == IR_PHI && ir->phi_arg_names) {
                    for (int p = 0; p < ir->phi_arg_count; p++) {
                        if (ir->phi_arg_names[p] &&
                            strcmp(ir->phi_arg_names[p], consts[ci].name) == 0) {
                            free(ir->phi_arg_names[p]);
                            ir->phi_arg_names[p] = strdup(consts[ci].const_value);
                            propagated++;
                        }
                    }
                }

                if (ir == bb->last_ir) break;
                ir = ir->next;
            }
        }
    }

    printf("  · 传播了 %d 个常量到 %d 处使用\n", const_cnt, propagated);

    for (int i = 0; i < const_cnt; i++) {
        free(consts[i].name);
        free(consts[i].const_value);
    }
    free(consts);
    return propagated;
}

void ssa_print_cfg(const CFG* cfg) {
    if (!cfg) { printf("SSA CFG: (null)\n"); return; }

    printf("══════════════════════════════════════\n");
    printf("  SSA CFG: %d blocks, %d variables\n", cfg->block_count, cfg->var_count);
    printf("══════════════════════════════════════\n");

    for (int bi = 0; bi < cfg->block_count; bi++) {
        const BasicBlock* bb = cfg->blocks[bi];
        printf("\n┌─ BB%d ───────────────────────────────\n", bb->id);
        printf("│ preds: ");
        for (int p = 0; p < bb->pred_count; p++)
            printf("BB%d ", bb->preds[p]->id);
        printf("\n│ succs: ");
        for (int s = 0; s < bb->succ_count; s++)
            printf("BB%d ", bb->succs[s]->id);
        printf("\n│ idom: BB%s\n",
               bb->idom ? (bb->idom->id >= 0 ? "?" : "(none)") : "(none)");
        if (bb->idom) printf("%d", bb->idom->id);
        printf("\n├──────────────────────────────────────\n");

        IR* ir = bb->first_ir;
        while (ir) {
            printf("│ ");
            switch (ir->type) {
            case IR_PHI:
                printf("φ %s = (", ir->result);
                for (int i = 0; i < ir->phi_arg_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%s", ir->phi_arg_names[i] ? ir->phi_arg_names[i] : "?");
                }
                printf(")");
                break;
            case IR_LITERAL:
                printf("%s = %s", ir->result, ir->args1);
                break;
            case IR_BINARY:
                printf("%s = %s %s %s", ir->result, ir->args1,
                       ir->op == IR_ADD ? "+" :
                       ir->op == IR_SUB ? "-" :
                       ir->op == IR_MUL ? "*" :
                       ir->op == IR_DIV ? "/" : "?", ir->args2);
                break;
            case IR_VAR_ASS:
                printf("%s = %s", ir->result, ir->args1);
                break;
            case IR_JMPZ:
                printf("if %s == 0 goto %s", ir->args1, ir->args2);
                break;
            case IR_JMP:
                printf("goto %s", ir->args1);
                break;
            case IR_RET:
                printf("ret %s", ir->args1 ? ir->args1 : "");
                break;
            case IR_LABEL:
                printf("%s:", ir->args1);
                break;
            default:
                printf("%s = ... (type %d)", ir->result ? ir->result : "?", ir->type);
                break;
            }
            printf("\n");

            if (ir == bb->last_ir) break;
            ir = ir->next;
        }
        printf("└──────────────────────────────────────\n");
    }
    printf("\n");
    fflush(stdout);
}