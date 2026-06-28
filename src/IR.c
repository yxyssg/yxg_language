#include "IR.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Error.h"

int temp_i = 0;
static int label_i = 0;
static int foreach_idx_counter = 0;
static int global_def_id = 0;

static LoopContext* ctx_stack = NULL;
static int ctx_top = -1;
static int ctx_capacity = 0;

int assign_def_id(IR* ir) {
    if (!ir) return -1;
    ir->def_id = ++global_def_id;
    return ir->def_id;
}

int get_def_id_by_name(const char* name, IR* head, IR* stop_before) {
    if (!name || !head) return -1;
    IR* found = NULL;
    for (IR* cur = head; cur && cur != stop_before; cur = cur->next) {
        if (cur->result && strcmp(cur->result, name) == 0) {
            found = cur;
        }
    }
    return found ? found->def_id : -1;
}

static void push_context(const char* break_lbl, const char* continue_lbl) {
    if (ctx_top + 1 >= ctx_capacity) {
        ctx_capacity = (ctx_capacity == 0) ? 8 : ctx_capacity * 2;
        ctx_stack = realloc(ctx_stack, sizeof(LoopContext) * ctx_capacity);
    }
    ctx_top++;
    ctx_stack[ctx_top].break_label = strdup(break_lbl);
    ctx_stack[ctx_top].continue_label = continue_lbl ? strdup(continue_lbl) : NULL;
}

static void pop_context(void) {
    if (ctx_top >= 0) {
        free(ctx_stack[ctx_top].break_label);
        free(ctx_stack[ctx_top].continue_label);
        ctx_top--;
    }
}

static const char* current_break_label(void) {
    return (ctx_top >= 0) ? ctx_stack[ctx_top].break_label : NULL;
}

static const char* current_continue_label(void) {
    return (ctx_top >= 0) ? ctx_stack[ctx_top].continue_label : NULL;
}

static void flatten_init_list(ASTNode* node, ASTNode*** out, int* count, int* capacity) {
    if (node == NULL) return;
    if (node->type == AST_INIT_LIST) {
        AST_InitList* list = (AST_InitList*)node;
        for (int i = 0; i < list->count; i++)
            flatten_init_list(list->elements[i], out, count, capacity);
    } else {
        if (*count >= *capacity) {
            *capacity = (*capacity == 0) ? 16 : (*capacity) * 2;
            *out = realloc(*out, sizeof(ASTNode*) * (*capacity));
        }
        (*out)[(*count)++] = node;
    }
}

IR* parse_IR(ASTNode* ast_node)
{
    if (ast_node == NULL)
    {
        INTERNAL_ERROR("IR: parse failed");
    }

    return start_generate_IR(ast_node);
}

IR* create_IR(IRType ir_type)
{
    IR* ir = calloc(1,sizeof(IR));
    if (ir == NULL)
    {
        INTERNAL_ERROR("IR: generation failed");
    }
    ir->args1 = NULL;
    ir->args2 = NULL;
    ir->next = NULL;
    ir->result = NULL;
    ir->label = NULL;
    ir->type = ir_type;
    ir->def_id = -1;
    ir->args1_def = -1;
    ir->args2_def = -1;
    return ir;
}

char* create_func_temp_name(int index) {
    int len = snprintf(NULL, 0, ".f%d",index);
    char* temp = malloc(len + 1);
    if (temp) {
        snprintf(temp, len + 1, ".f%d",index);
    }
    return temp;
}

char* create_temp() {
    int len = snprintf(NULL, 0, ".t%d",++temp_i);
    char* temp = malloc(len + 1);
    if (temp) {
        snprintf(temp, len + 1, ".t%d",temp_i);
    }
    return temp;
}

char* create_label()
{
    int len = snprintf(NULL, 0, "l%d", ++label_i);
    char* temp = malloc(len + 1);
    if (temp) {
        snprintf(temp, len + 1, "l%d", label_i);
    }
    return temp;
}

IR* connect_IR(IR* ir1, IR* ir2)
{
    if (ir1 == NULL)
    {
        return ir2;
    }
    IR* temp = ir1;
    while (temp->next != NULL)
    {
        temp = temp->next;
    }
    temp->next = ir2;
    return ir1;
}

static int count_local_vars(ASTNode* node) {
    if (node == NULL) return 0;

    int count = 0;

    switch (node->type) {
    case AST_VAR:
    case AST_UNINIT_VAR:
        count = 1;
        break;

    case AST_IF: {
            AST_If* if_node = (AST_If*)node;
            count = count_local_vars(if_node->code);
            for (int i = 0; i < if_node->elif_Count; i++) {
                count += count_local_vars(if_node->Elif[i]);
            }
            if (if_node->has_else) {
                count += count_local_vars(if_node->Else);
            }
            break;
    }

    case AST_WHILE: {
            AST_While* while_node = (AST_While*)node;
            count = count_local_vars(while_node->code);
            break;
    }

    case AST_FOR: {
            AST_For* f = (AST_For*)node;
            count += count_local_vars(f->init);
            count += count_local_vars(f->code);
            break;
    }
    case AST_FOREACH: {
            AST_ForEach* fe = (AST_ForEach*)node;
            count += 1;
            count += count_local_vars(fe->code);
            break;
    }

    case AST_FUNCTION:
        break;

    default:
        break;
    }

    count += count_local_vars(node->next);
    return count;
}

char* get_last_result(IR* ir) {
    if (ir == NULL) {
        INTERNAL_ERROR("IR: get_last_result with NULL chain");
    }

    IR* current = ir;
    while (current->next != NULL) {
        current = current->next;
    }

    if (current->result == NULL) {
        INTERNAL_ERROR("IR: last node has NULL result");
    }

    return current->result;
}

static char* gen_multidim_address(IR** chain_out, const char* base_temp,
                                   int* dim_sizes, int dim_count,
                                   ASTNode** indices, int index_count,
                                   int elem_size) {
    if (base_temp == NULL || dim_sizes == NULL || indices == NULL)
        return NULL;
    if (dim_count < index_count || index_count <= 0)
        return NULL;

    for (int i = 0; i < index_count; i++) {
        if (indices[i] == NULL) return NULL;
    }

    IR* chain = NULL;

    IR* idx0_ir = generate_IR(indices[0]);
    IR* idx0_last = idx0_ir;
    while (idx0_last->next) idx0_last = idx0_last->next;
    chain = idx0_ir;
    char* current_offset = idx0_last->result;

    for (int d = 1; d < index_count; d++) {
        IR* idx_ir = generate_IR(indices[d]);
        IR* idx_last = idx_ir;
        while (idx_last->next) idx_last = idx_last->next;

        IR* mul = create_IR(IR_BINARY);
        mul->op = IR_MUL;
        mul->args1 = strdup(current_offset);
        mul->args1_type = DATA_INT;
        char dim_str[16];
        sprintf(dim_str, "%d", dim_sizes[d]);
        mul->args2 = strdup(dim_str);
        mul->args2_type = DATA_INT;
        mul->result = create_temp();
        assign_def_id(mul);
        mul->data_type = DATA_INT;

        IR* add = create_IR(IR_BINARY);
        add->op = IR_ADD;
        add->args1 = strdup(mul->result);
        add->args1_type = DATA_INT;
        add->args2 = strdup(idx_last->result);
        add->args2_type = DATA_INT;
        add->result = create_temp();
        assign_def_id(add);
        add->data_type = DATA_INT;

        chain = connect_IR(chain, idx_ir);
        chain = connect_IR(chain, mul);
        chain = connect_IR(chain, add);

        current_offset = add->result;
    }

    IR* mul_esz = create_IR(IR_BINARY);
    mul_esz->op = IR_MUL;
    mul_esz->args1 = strdup(current_offset);
    mul_esz->args1_type = DATA_INT;
    char esz_str[16];
    sprintf(esz_str, "%d", elem_size);
    mul_esz->args2 = strdup(esz_str);
    mul_esz->args2_type = DATA_INT;
    mul_esz->result = create_temp();
    assign_def_id(mul_esz);
    mul_esz->data_type = DATA_INT;
    chain = connect_IR(chain, mul_esz);

    IR* add_base = create_IR(IR_BINARY);
    add_base->op = IR_ADD;
    add_base->args1 = strdup(base_temp);
    add_base->args1_type = DATA_INT;
    add_base->args2 = strdup(mul_esz->result);
    add_base->args2_type = DATA_INT;
    add_base->result = create_temp();
    assign_def_id(add_base);
    add_base->data_type = DATA_INT;
    chain = connect_IR(chain, add_base);

    *chain_out = chain;
    return strdup(add_base->result);
}

IR* start_generate_IR(ASTNode* ast_node)
{
    if (ast_node == NULL)
    {
        INTERNAL_ERROR("IR: generation failed");
    }

    IR* ir_chain = NULL;
    IR* last_ir = NULL;
    ASTNode* current_node = ast_node;

    while (current_node != NULL)
    {
        IR* node_ir = generate_IR(current_node);

        if (node_ir == NULL) {
            current_node = current_node->next;
            continue;
        }

        if (ir_chain == NULL) {
            ir_chain = node_ir;
        } else {
            if (last_ir == NULL) {
                last_ir = ir_chain;
                while (last_ir->next != NULL) {
                    last_ir = last_ir->next;
                }
            }
            last_ir->next = node_ir;
        }

        last_ir = node_ir;
        while (last_ir != NULL && last_ir->next != NULL) {
            last_ir = last_ir->next;
        }

        current_node = current_node->next;
    }

    return ir_chain;
}

IR_OP_Type Type_TO_OP(TokenType type, bool is_unsigned) {
    switch (type) {
    case TOKEN_ADD: return IR_ADD;
    case TOKEN_SUB: return IR_SUB;
    case TOKEN_MUL: return IR_MUL;
    case TOKEN_DIV: return is_unsigned ? IR_UDIV : IR_DIV;
    case TOKEN_MOD: return is_unsigned ? IR_UMOD : IR_MOD;
    case TOKEN_EQ:  return IR_JE;
    case TOKEN_NE:  return IR_JNE;
    case TOKEN_LT:  return is_unsigned ? IR_ULT : IR_JL;
    case TOKEN_LE:  return is_unsigned ? IR_ULE : IR_JLE;
    case TOKEN_GT:  return is_unsigned ? IR_UGT : IR_JG;
    case TOKEN_GE:  return is_unsigned ? IR_UGE : IR_JGE;
    case TOKEN_BITWISE_AND: return IR_BITWISE_AND;
    case TOKEN_BITWISE_OR:  return IR_BITWISE_OR;
    case TOKEN_SHIFT_LEFT:  return IR_SHL;
    case TOKEN_SHIFT_RIGHT: return is_unsigned ? IR_SHR : IR_SAR;
    case TOKEN_BITWISE_XOR: return IR_BITWISE_XOR;
    default: return IR_OP_ERROR;
    }
}

int get_type_size(DataType type) {
    switch (type) {
    case DATA_CHAR:
    case DATA_BOOL:   return 1;
    case DATA_INT:
    case DATA_UINT:
    case DATA_FLOAT:
    case DATA_UFLOAT:
    case DATA_STRING: return 4;
    default:          return 4;
    }
}

static IR* gen_array_access_ir(AST_ArrayAccess* acc, bool gen_load, char** addr_out) {
    if (acc->index_count == 0) return NULL;

    IR* base_ir = generate_IR(acc->array);
    IR* base_last = base_ir;
    while (base_last->next) base_last = base_last->next;
    char* base_temp = base_last->result;

    int elem_size = get_type_size(acc->elementType);
    IR* chain = base_ir;

    char* offset_temp = NULL;
    for (int d = 0; d < acc->index_count; d++) {
        IR* idx_ir = generate_IR(acc->indices[d]);
        IR* idx_last = idx_ir;
        while (idx_last->next) idx_last = idx_last->next;
        char* idx_val = idx_last->result;
        chain = connect_IR(chain, idx_ir);

        if (acc->full_dim_sizes != NULL && d < acc->full_dim_count) {
            int dim = acc->full_dim_sizes[d];
            char dimStr[16];
            sprintf(dimStr, "%d", dim);

            IR* dim_lit = create_IR(IR_LITERAL);
            dim_lit->args1 = strdup(dimStr);
            dim_lit->args1_type = DATA_INT;
            dim_lit->data_type = DATA_INT;
            dim_lit->result = create_temp();
            assign_def_id(dim_lit);
            chain = connect_IR(chain, dim_lit);

            IR* check = create_IR(IR_BOUNDS_CHECK);
            check->args1 = strdup(idx_val);
            check->args2 = strdup(dim_lit->result);
            check->data_type = DATA_INT;
            chain = connect_IR(chain, check);
        }

        if (d == 0) {
            offset_temp = idx_val;
        } else {
            int cur_dim = acc->full_dim_sizes[d];
            char dim_str[16];
            sprintf(dim_str, "%d", cur_dim);

            IR* mul = create_IR(IR_BINARY);
            mul->op = IR_MUL;
            mul->args1 = strdup(offset_temp);
            mul->args1_type = DATA_INT;
            mul->args2 = strdup(dim_str);
            mul->args2_type = DATA_INT;
            mul->result = create_temp();
            assign_def_id(mul);
            mul->data_type = DATA_INT;
            chain = connect_IR(chain, mul);

            IR* add = create_IR(IR_BINARY);
            add->op = IR_ADD;
            add->args1 = strdup(mul->result);
            add->args1_type = DATA_INT;
            add->args2 = strdup(idx_val);
            add->args2_type = DATA_INT;
            add->result = create_temp();
            assign_def_id(add);
            add->data_type = DATA_INT;
            chain = connect_IR(chain, add);

            offset_temp = add->result;
        }
    }

    if (!offset_temp) {
        return chain;
    }

    IR* mul_elem = create_IR(IR_BINARY);
    mul_elem->op = IR_MUL;
    mul_elem->args1 = strdup(offset_temp);
    mul_elem->args1_type = DATA_INT;
    char esz[16];
    sprintf(esz, "%d", elem_size);
    mul_elem->args2 = strdup(esz);
    mul_elem->args2_type = DATA_INT;
    mul_elem->result = create_temp();
    assign_def_id(mul_elem);
    mul_elem->data_type = DATA_INT;
    chain = connect_IR(chain, mul_elem);

    IR* add_base = create_IR(IR_BINARY);
    add_base->op = IR_ADD;
    add_base->args1 = strdup(base_temp);
    add_base->args1_type = DATA_INT;
    add_base->args2 = strdup(mul_elem->result);
    add_base->args2_type = DATA_INT;
    add_base->result = create_temp();
    assign_def_id(add_base);
    add_base->data_type = DATA_INT;
    chain = connect_IR(chain, add_base);

    char* addr_temp = add_base->result;

    if (gen_load) {
        IR* load_ir = create_IR(IR_ARRAY_LOAD);
        load_ir->result = create_temp();
        assign_def_id(load_ir);
        load_ir->args1 = strdup(addr_temp);
        load_ir->args1_type = DATA_INT;
        load_ir->data_type = acc->elementType;
        load_ir->elem_size = elem_size;
        chain = connect_IR(chain, load_ir);
        return chain;
    } else {
        if (addr_out) *addr_out = addr_temp;
        return chain;
    }
}

IR* generate_IR(ASTNode* ast_node)
{
    if (ast_node == NULL)
    {
        INTERNAL_ERROR("IR: generate_IR called with NULL");
    }

    switch (ast_node->type)
    {
    case AST_LITERAL:
        AST_Literal* ast_literal = (AST_Literal*)ast_node;
        IR* Literal_ir;

        if (ast_literal->is_call_function) {
            Literal_ir = create_IR(IR_CALL);
        } else {
            Literal_ir = create_IR(IR_LITERAL);
        }

        Literal_ir->result = create_temp();
        assign_def_id(Literal_ir);

        if (ast_literal->base.value.type == TOKEN_IDENTIFIER) {
            Literal_ir->args1 = strdup(ast_literal->internal_name);
        } else if (ast_literal->base.value.type == TOKEN_NUMBER &&
                   !strchr(ast_literal->base.value.value, '.')) {
            Literal_ir->args1 = normalize_number_str(ast_literal->base.value.value);
        } else {
            Literal_ir->args1 = strdup(ast_literal->base.value.value);
        }

        Literal_ir->args2 = NULL;
        Literal_ir->data_type = ast_literal->dataType;

        if (ast_literal->base.value.type == TOKEN_IDENTIFIER) {
            Literal_ir->args1_type = DATA_IDENTIFIER;
        } else {
            Literal_ir->args1_type = ast_literal->dataType;
        }

        Literal_ir->call_function_Parameter = NULL;

        if (ast_literal->is_call_function)
        {
            if (ast_literal->call_function_Parameter != NULL &&
                ast_literal->call_function_Parameter->type == AST_MEMBER_ACCESS)
            {
                AST_MemberAccess* ma = (AST_MemberAccess*)ast_literal->call_function_Parameter;

                IR* obj_chain = generate_IR(ma->object);
                if (obj_chain == NULL) return NULL;

                IR* obj_last = obj_chain;
                while (obj_last->next != NULL) {
                    obj_last = obj_last->next;
                }
                char* this_temp = obj_last->result;

                int total_arg_count = 1 + ast_literal->method_arg_count;
                char** arg_temps = malloc(sizeof(char*) * total_arg_count);
                DataType* arg_types = malloc(sizeof(DataType) * total_arg_count);

                arg_temps[0] = strdup(this_temp);
                arg_types[0] = DATA_INT;

                IR* after_obj_chain = obj_chain;
                ASTNode* arg_node = ast_literal->method_arguments;
                int idx = 1;
                while (arg_node) {
                    IR* arg_ir = generate_IR(arg_node);
                    if (arg_ir == NULL) {
                        for (int j = 0; j < idx; j++) free(arg_temps[j]);
                        free(arg_temps); free(arg_types);
                        return NULL;
                    }

                    IR* last = arg_ir;
                    while (last->next) last = last->next;

                    arg_temps[idx] = strdup(last->result);
                    arg_types[idx] = last->data_type;
                    idx++;

                    after_obj_chain = connect_IR(after_obj_chain, arg_ir);
                    arg_node = arg_node->next;
                }

                IR* push_chain = NULL;
                IR* push_last = NULL;
                for (int i = total_arg_count - 1; i >= 0; i--) {
                    IR* push_ir = create_IR(IR_PUSH);
                    push_ir->args1 = strdup(arg_temps[i]);
                    push_ir->args1_type = arg_types[i];
                    push_ir->next = NULL;

                    if (push_chain == NULL) {
                        push_chain = push_ir;
                        push_last = push_ir;
                    } else {
                        push_last->next = push_ir;
                        push_last = push_ir;
                    }
                }

                if (push_last) {
                    after_obj_chain = connect_IR(after_obj_chain, push_chain);
                }

                IR* call_ir = create_IR(IR_CALL);
                call_ir->args1 = strdup(ast_literal->internal_name);
                call_ir->result = create_temp();
                assign_def_id(call_ir);
                call_ir->data_type = ast_literal->dataType;
                call_ir->call_func_args_num = total_arg_count;

                after_obj_chain = connect_IR(after_obj_chain, call_ir);

                for (int i = 0; i < total_arg_count; i++) {
                    free(arg_temps[i]);
                }
                free(arg_temps);
                free(arg_types);

                return after_obj_chain;
            }
            if (strcmp(ast_literal->base.value.value, "_arrayToString") == 0) {
                AST_Literal* arr_lit = (AST_Literal*)ast_literal->call_function_Parameter;
                if (arr_lit && arr_lit->base.type == AST_ARRAY_ACCESS) {
                    AST_ArrayAccess* acc = (AST_ArrayAccess*)arr_lit;
                    if (acc->dim_count > 0) {
                        IR* base_ir = generate_IR(acc->array);
                        IR* base_last = base_ir;
                        while (base_last->next) base_last = base_last->next;
                        char* base_temp = base_last->result;

                        int totalDims = acc->full_dim_count;
                        int* dimSizes = acc->full_dim_sizes;

                        IR* addr_chain = NULL;
                        char* addr_temp = gen_multidim_address(&addr_chain,
                                            base_temp,
                                            dimSizes, totalDims,
                                            acc->indices, acc->index_count,
                                            get_type_size(acc->elementType));
                        if (!addr_temp) return NULL;

                        IR* chain = connect_IR(base_ir, addr_chain);

                        int sub_len = acc->dim_sizes[0];

                        IR* tostr = create_IR(IR_ARRAY_TOSTRING);
                        tostr->result = create_temp();
                        assign_def_id(tostr);
                        tostr->args1 = strdup(addr_temp);
                        tostr->args1_type = acc->elementType;
                        char lenStr[16];
                        sprintf(lenStr, "%d", sub_len);
                        tostr->args2 = strdup(lenStr);
                        tostr->data_type = DATA_STRING;

                        chain = connect_IR(chain, tostr);
                        free(addr_temp);
                        return chain;
                    }
                }
                if (!arr_lit) return NULL;
                char* internal_name = arr_lit->internal_name;
                if (!internal_name) internal_name = arr_lit->base.value.value;

                IR* base_ir = generate_IR(ast_literal->call_function_Parameter);
                IR* base_last = base_ir;
                while (base_last->next) base_last = base_last->next;

                IR* tostr_ir = create_IR(IR_ARRAY_TOSTRING);
                tostr_ir->args1 = strdup(base_last->result);
                tostr_ir->args1_type = arr_lit->dataType;
                tostr_ir->args2 = strdup(internal_name);
                tostr_ir->data_type = DATA_STRING;
                tostr_ir->result = create_temp();
                assign_def_id(tostr_ir);

                return connect_IR(base_ir, tostr_ir);
            }
            if (strcmp(ast_literal->base.value.value, "format") == 0 ||
                strcmp(ast_literal->base.value.value, "_format") == 0)
            {
                IR *chain = NULL, *last = NULL;
                ASTNode *arg = ast_literal->call_function_Parameter;
                int count = 0;
                char **names = NULL;
                DataType *types = NULL;

                while (arg) {
                    bool is_direct = false;
                    if (arg->type == AST_LITERAL) {
                        AST_Literal *lit = (AST_Literal*)arg;
                        if (lit->base.value.type == TOKEN_IDENTIFIER && !lit->is_call_function) {
                            is_direct = true;
                        }
                    }

                    names = realloc(names, sizeof(char*) * (count + 1));
                    types = realloc(types, sizeof(DataType) * (count + 1));

                    if (is_direct) {
                        AST_Literal *lit = (AST_Literal*)arg;
                        names[count] = strdup(lit->internal_name);
                        types[count] = lit->dataType;
                    } else {
                        IR *arg_ir = generate_IR(arg);
                        if (arg_ir == NULL) {
                            INTERNAL_ERROR("IR: format argument IR is NULL");
                            free(names);
                            free(types);
                            return chain;
                        }
                        IR *tail = arg_ir;
                        while (tail->next && tail != NULL) tail = tail->next;

                        names[count] = strdup(tail->result);
                        types[count] = tail->data_type;

                        if (!chain) chain = arg_ir;
                        else last->next = arg_ir;
                        last = tail;
                    }

                    arg = arg->next;
                    count++;
                }

                IR *fmt_ir = create_IR(IR_FORMAT);
                fmt_ir->result = create_temp();
                assign_def_id(fmt_ir);
                fmt_ir->data_type = DATA_STRING;

                if (count > 0) {
                    fmt_ir->args1 = strdup(names[0]);
                    fmt_ir->call_func_args_num = count - 1;
                    if (count - 1 > 0) {
                        fmt_ir->call_func_args_type = malloc(sizeof(DataType) * (count - 1));
                        memcpy(fmt_ir->call_func_args_type, &types[1], sizeof(DataType) * (count - 1));
                        fmt_ir->label = malloc(sizeof(char*) * (count - 1));
                        for (int i = 1; i < count; i++) {
                            fmt_ir->label[i - 1] = strdup(names[i]);
                        }
                    }
                } else {
                    fmt_ir->args1 = NULL;
                    fmt_ir->call_func_args_num = 0;
                }

                for (int i = 0; i < count; i++) free(names[i]);
                free(names);
                free(types);

                if (last) last->next = fmt_ir;
                else chain = fmt_ir;

                return chain;
            }
            IR* calc_chain = NULL;
            IR* calc_last = NULL;
            int arg_count = 0;

            int max_args = ast_literal->call_function_ParameterCount;
            char** arg_temps = malloc(max_args * sizeof(char*));
            DataType* arg_types = malloc(max_args * sizeof(DataType));

            ASTNode* arg_node = ast_literal->call_function_Parameter;
            while (arg_node) {
                IR* arg_ir = generate_IR(arg_node);
                IR* last = arg_ir;
                while (last->next) last = last->next;

                arg_temps[arg_count] = strdup(last->result);
                arg_types[arg_count] = last->data_type;

                if (calc_chain == NULL) {
                    calc_chain = arg_ir;
                    calc_last = last;
                } else {
                    calc_last->next = arg_ir;
                    calc_last = last;
                }

                arg_node = arg_node->next;
                arg_count++;
            }

            IR* push_chain = NULL;
            IR* push_last = NULL;
            for (int i = arg_count - 1; i >= 0; i--) {
                IR* push_ir = create_IR(IR_PUSH);
                push_ir->args1 = strdup(arg_temps[i]);
                push_ir->args1_type = arg_types[i];
                push_ir->next = NULL;

                if (push_chain == NULL) {
                    push_chain = push_ir;
                    push_last = push_ir;
                } else {
                    push_last->next = push_ir;
                    push_last = push_ir;
                }
            }

            if (calc_last) {
                calc_last->next = push_chain;
            } else {
                calc_chain = push_chain;
                calc_last = push_last;
            }

            IR* call_ir = create_IR(IR_CALL);
            call_ir->args1 = strdup(ast_literal->internal_name);
            call_ir->result = create_temp();
            assign_def_id(call_ir);
            call_ir->data_type = ast_literal->dataType;
            call_ir->call_func_args_num = arg_count;

            if (push_last) {
                push_last->next = call_ir;
            } else if (calc_last) {
                calc_last->next = call_ir;
            } else {
                calc_chain = call_ir;
            }

            for (int i = 0; i < arg_count; i++) {
                free(arg_temps[i]);
            }
            free(arg_temps);
            free(arg_types);

            return calc_chain;
        } else if (ast_literal->is_field_ref) {
            IR* this_ir = create_IR(IR_LOAD_THIS);
            this_ir->result = create_temp();
            assign_def_id(this_ir);
            IR* load_member = create_IR(IR_LOAD_MEMBER);
            load_member->result = create_temp();
            assign_def_id(load_member);
            load_member->args1 = strdup(this_ir->result);
            load_member->args1_type = DATA_INT;
            char off_str[16];
            sprintf(off_str, "%d", ast_literal->field_offset);
            load_member->args2 = strdup(off_str);
            load_member->data_type = ast_literal->dataType;
            IR* chain = connect_IR(this_ir, load_member);
            return chain;
        }

        return Literal_ir;
    case AST_BINARY:
        AST_Binary* ast_binary = (AST_Binary*)ast_node;
        if (ast_binary->op.type == TOKEN_LOGICAL_AND)
        {
            IR* left_chain  = generate_IR(ast_binary->left);
            IR* right_chain = generate_IR(ast_binary->right);
            if (!left_chain || !right_chain) return NULL;

            IR* left_last  = left_chain;  while(left_last->next)  left_last  = left_last->next;
            IR* right_last = right_chain; while(right_last->next) right_last = right_last->next;

            char* result      = create_temp();
            char* false_label = create_label();
            char* end_label   = create_label();

            IR* jz_left = create_IR(IR_JMPZ);
            jz_left->args1 = strdup(left_last->result);
            jz_left->args2 = strdup(false_label);

            IR* jz_right = create_IR(IR_JMPZ);
            jz_right->args1 = strdup(right_last->result);
            jz_right->args2 = strdup(false_label);

            IR* set_true = create_IR(IR_LITERAL);
            set_true->args1 = strdup("1");
            set_true->args1_type = DATA_INT;
            set_true->data_type  = DATA_BOOL;
            set_true->result = strdup(result);
            assign_def_id(set_true);

            IR* jmp_end = create_IR(IR_JMP);
            jmp_end->args1 = strdup(end_label);

            IR* label_false = create_IR(IR_LABEL);
            label_false->args1 = strdup(false_label);

            IR* set_false = create_IR(IR_LITERAL);
            set_false->args1 = strdup("0");
            set_false->args1_type = DATA_INT;
            set_false->data_type  = DATA_BOOL;
            set_false->result = strdup(result);
            assign_def_id(set_false);

            IR* label_end = create_IR(IR_LABEL);
            label_end->args1 = strdup(end_label);
                        IR* chain = left_chain;
            chain = connect_IR(chain, jz_left);
            chain = connect_IR(chain, right_chain);
            chain = connect_IR(chain, jz_right);
            chain = connect_IR(chain, set_true);
            chain = connect_IR(chain, jmp_end);
            chain = connect_IR(chain, label_false);
            chain = connect_IR(chain, set_false);
            chain = connect_IR(chain, label_end);
            IR* result_pass = create_IR(IR_LITERAL);
            result_pass->result = strdup(result);
            assign_def_id(result_pass);
            result_pass->args1 = strdup(result);
            result_pass->args1_type = DATA_BOOL;
            result_pass->data_type  = DATA_BOOL;

            chain = connect_IR(chain, result_pass);
            return chain;
        }

    if (ast_binary->op.type == TOKEN_LOGICAL_OR) {
        IR* left_chain  = generate_IR(ast_binary->left);
        IR* right_chain = generate_IR(ast_binary->right);
        if (!left_chain || !right_chain) return NULL;

        IR* left_last  = left_chain;  while(left_last->next)  left_last  = left_last->next;
        IR* right_last = right_chain; while(right_last->next) right_last = right_last->next;

        char* result     = create_temp();
        char* true_label = create_label();
        char* end_label  = create_label();

        IR* jnz_left = create_IR(IR_JMPNZ);
        jnz_left->args1 = strdup(left_last->result);
        jnz_left->args2 = strdup(true_label);

        IR* jnz_right = create_IR(IR_JMPNZ);
        jnz_right->args1 = strdup(right_last->result);
        jnz_right->args2 = strdup(true_label);

        IR* set_false = create_IR(IR_LITERAL);
        set_false->args1 = strdup("0");
        set_false->args1_type = DATA_INT;
        set_false->data_type  = DATA_BOOL;
        set_false->result = strdup(result);
        assign_def_id(set_false);

        IR* jmp_end = create_IR(IR_JMP);
        jmp_end->args1 = strdup(end_label);

        IR* label_true = create_IR(IR_LABEL);
        label_true->args1 = strdup(true_label);

        IR* set_true = create_IR(IR_LITERAL);
        set_true->args1 = strdup("1");
        set_true->args1_type = DATA_INT;
        set_true->data_type  = DATA_BOOL;
        set_true->result = strdup(result);
        assign_def_id(set_true);

        IR* label_end = create_IR(IR_LABEL);
        label_end->args1 = strdup(end_label);

        IR* chain = left_chain;
        chain = connect_IR(chain, jnz_left);
        chain = connect_IR(chain, right_chain);
        chain = connect_IR(chain, jnz_right);
        chain = connect_IR(chain, set_false);
        chain = connect_IR(chain, jmp_end);
        chain = connect_IR(chain, label_true);
        chain = connect_IR(chain, set_true);
        chain = connect_IR(chain, label_end);
        IR* result_pass = create_IR(IR_LITERAL);
        result_pass->result = strdup(result);
        assign_def_id(result_pass);
        result_pass->args1 = strdup(result);
        result_pass->args1_type = DATA_BOOL;
        result_pass->data_type  = DATA_BOOL;

        chain = connect_IR(chain, result_pass);
        return chain;
    }

        IR* Binary_ir = create_IR(IR_BINARY);

        IR* left_ir_chain = generate_IR(ast_binary->left);
        IR* left_last = left_ir_chain;
        while (left_last->next != NULL) {
            left_last = left_last->next;
        }

        IR* right_ir_chain = generate_IR(ast_binary->right);

        IR* right_last = right_ir_chain;
        while (right_last->next != NULL) {
            right_last = right_last->next;
        }

        Binary_ir->op = Type_TO_OP(ast_binary->op.type,ast_binary->is_unsigned);
        Binary_ir->result = create_temp();
        assign_def_id(Binary_ir);

        Binary_ir->args1 = strdup(left_last->result);
        Binary_ir->args1_type = left_last->data_type;
        Binary_ir->args2 = strdup(right_last->result);
        Binary_ir->args2_type = right_last->data_type;

        IR* binary_ir_chain = connect_IR(left_ir_chain,right_ir_chain);
        Binary_ir->data_type = ast_binary->resultType;
        return connect_IR(binary_ir_chain,Binary_ir);

        case AST_UNARY:
            {
                AST_Unary* un = (AST_Unary*)ast_node;

                if (un->op.type == TOKEN_SUB) {
                    IR* right_ir = generate_IR(un->right);
                    if (right_ir == NULL) return NULL;

                    IR* right_last = right_ir;
                    while (right_last->next != NULL)
                        right_last = right_last->next;

                    IR* zero_ir = create_IR(IR_LITERAL);
                    if (right_last->data_type == DATA_FLOAT || right_last->data_type == DATA_UFLOAT) {
                        zero_ir->args1 = strdup("0.0");
                        zero_ir->args1_type = DATA_FLOAT;
                        zero_ir->data_type = DATA_FLOAT;
                    } else {
                        zero_ir->args1 = strdup("0");
                        zero_ir->args1_type = DATA_INT;
                        zero_ir->data_type = DATA_INT;
                    }
                    zero_ir->result = create_temp();
                    assign_def_id(zero_ir);

                    IR* sub_ir = create_IR(IR_BINARY);
                    sub_ir->op = IR_SUB;
                    sub_ir->args1 = strdup(zero_ir->result);
                    sub_ir->args1_type = zero_ir->data_type;
                    sub_ir->args2 = strdup(right_last->result);
                    sub_ir->args2_type = right_last->data_type;
                    sub_ir->result = create_temp();
                    assign_def_id(sub_ir);
                    sub_ir->data_type = (right_last->data_type == DATA_FLOAT) ? DATA_FLOAT : DATA_INT;

                    IR* chain = connect_IR(zero_ir, right_ir);
                    chain = connect_IR(chain, sub_ir);
                    return chain;
                }

                else if (un->op.type == TOKEN_OPPOSE) {
                    IR* right_ir = generate_IR(un->right);
                    if (right_ir == NULL) return NULL;

                    IR* right_last = right_ir;
                    while (right_last->next != NULL)
                        right_last = right_last->next;

                    IR* eq_ir = create_IR(IR_BINARY);
                    eq_ir->op = IR_JE;
                    eq_ir->args1 = strdup(right_last->result);
                    eq_ir->args1_type = right_last->data_type;
                    eq_ir->args2 = strdup("0");
                    eq_ir->args2_type = DATA_INT;
                    eq_ir->result = create_temp();
                    assign_def_id(eq_ir);
                    eq_ir->data_type = DATA_BOOL;

                    return connect_IR(right_ir, eq_ir);
                }

                else if (un->op.type == TOKEN_BITWISE_NOT) {
                    IR* right_ir = generate_IR(un->right);
                    if (right_ir == NULL) return NULL;

                    IR* right_last = right_ir;
                    while (right_last->next != NULL)
                        right_last = right_last->next;

                    IR* not_ir = create_IR(IR_BITWISE_NOT);
                    not_ir->args1 = strdup(right_last->result);
                    not_ir->args1_type = right_last->data_type;
                    not_ir->result = create_temp();
                    assign_def_id(not_ir);
                    not_ir->data_type = un->dataType;

                    return connect_IR(right_ir, not_ir);
                } else if (un->op.type == TOKEN_INCREMENT || un->op.type == TOKEN_DECREMENT)
                {
                    bool is_inc = (un->op.type == TOKEN_INCREMENT);
                    ASTNode* operand = un->right;
                    if (operand->type == AST_LITERAL) {
                        AST_Literal* lit = (AST_Literal*)operand;
                        const char* var_name = lit->internal_name;
                        IR* load_ir = create_IR(IR_LITERAL);
                        load_ir->result = create_temp();
                        assign_def_id(load_ir);
                        load_ir->args1 = strdup(var_name);
                        load_ir->args1_type = DATA_IDENTIFIER;
                        load_ir->data_type = lit->dataType;
                        IR* one_ir = create_IR(IR_LITERAL);
                        one_ir->result = create_temp();
                        assign_def_id(one_ir);
                        one_ir->args1 = strdup("1");
                        one_ir->args1_type = DATA_INT;
                        one_ir->data_type = DATA_INT;
                        IR* bin_ir = create_IR(IR_BINARY);
                        bin_ir->op = is_inc ? IR_ADD : IR_SUB;
                        bin_ir->args1 = strdup(load_ir->result);
                        bin_ir->args1_type = load_ir->data_type;
                        bin_ir->args2 = strdup(one_ir->result);
                        bin_ir->args2_type = DATA_INT;
                        bin_ir->result = create_temp();
                        assign_def_id(bin_ir);
                        bin_ir->data_type = lit->dataType;
                        IR* store_ir = create_IR(IR_VAR_ASS);
                        store_ir->result = strdup(var_name);
                        store_ir->args1 = strdup(bin_ir->result);
                        store_ir->args1_type = bin_ir->data_type;
                        store_ir->data_type = lit->dataType;
                        IR* result_ir = create_IR(IR_LITERAL);
                        result_ir->result = create_temp();
                        assign_def_id(result_ir);
                        result_ir->args1 = strdup(bin_ir->result);
                        result_ir->args1_type = bin_ir->data_type;
                        result_ir->data_type = lit->dataType;
                        IR* chain = load_ir;
                        chain = connect_IR(chain, one_ir);
                        chain = connect_IR(chain, bin_ir);
                        chain = connect_IR(chain, store_ir);
                        chain = connect_IR(chain, result_ir);
                        return chain;
                    }
                    else if (operand->type == AST_ARRAY_ACCESS) {
                        AST_ArrayAccess* acc = (AST_ArrayAccess*)operand;
                        char* addr_temp = NULL;
                        IR* addr_chain = gen_array_access_ir(acc, false, &addr_temp);
                        if (!addr_chain || !addr_temp) return NULL;
                        IR* load_ir = create_IR(IR_ARRAY_LOAD);
                        load_ir->result = create_temp();
                        assign_def_id(load_ir);
                        load_ir->args1 = strdup(addr_temp);
                        load_ir->args1_type = DATA_INT;
                        load_ir->data_type = acc->elementType;
                        load_ir->elem_size = get_type_size(acc->elementType);
                        IR* one_ir = create_IR(IR_LITERAL);
                        one_ir->result = create_temp();
                        assign_def_id(one_ir);
                        one_ir->args1 = strdup("1");
                        one_ir->args1_type = DATA_INT;
                        one_ir->data_type = DATA_INT;
                        IR* bin_ir = create_IR(IR_BINARY);
                        bin_ir->op = is_inc ? IR_ADD : IR_SUB;
                        bin_ir->args1 = strdup(load_ir->result);
                        bin_ir->args1_type = load_ir->data_type;
                        bin_ir->args2 = strdup(one_ir->result);
                        bin_ir->args2_type = DATA_INT;
                        bin_ir->result = create_temp();
                        assign_def_id(bin_ir);
                        bin_ir->data_type = acc->elementType;
                        IR* store_ir = create_IR(IR_ARRAY_STORE);
                        store_ir->args1 = strdup(addr_temp);
                        store_ir->args1_type = DATA_INT;
                        store_ir->args2 = strdup(bin_ir->result);
                        store_ir->args2_type = bin_ir->data_type;
                        store_ir->data_type = acc->elementType;
                        store_ir->elem_size = get_type_size(acc->elementType);
                        IR* result_ir = create_IR(IR_LITERAL);
                        result_ir->result = create_temp();
                        assign_def_id(result_ir);
                        result_ir->args1 = strdup(bin_ir->result);
                        result_ir->args1_type = bin_ir->data_type;
                        result_ir->data_type = acc->elementType;
                        IR* chain = addr_chain;
                        chain = connect_IR(chain, load_ir);
                        chain = connect_IR(chain, one_ir);
                        chain = connect_IR(chain, bin_ir);
                        chain = connect_IR(chain, store_ir);
                        chain = connect_IR(chain, result_ir);
                        return chain;
                    } else if (operand->type == AST_UNARY) {
                        IR* inner_ir = generate_IR(operand);
                        IR* inner_last = inner_ir;
                        while (inner_last->next) inner_last = inner_last->next;
                        ASTNode* inner_op = ((AST_Unary*)operand)->right;
                        char* var_name = NULL;
                        if (inner_op->type == AST_LITERAL) {
                            var_name = ((AST_Literal*)inner_op)->internal_name;
                        }
                        if (var_name == NULL) {
                            IR* one_ir = create_IR(IR_LITERAL);
                            one_ir->result = create_temp();
                            assign_def_id(one_ir);
                            one_ir->args1 = strdup("1");
                            one_ir->args1_type = DATA_INT;
                            one_ir->data_type = DATA_INT;
                            IR* bin_ir = create_IR(IR_BINARY);
                            bin_ir->op = is_inc ? IR_ADD : IR_SUB;
                            bin_ir->args1 = strdup(inner_last->result);
                            bin_ir->args1_type = inner_last->data_type;
                            bin_ir->args2 = strdup(one_ir->result);
                            bin_ir->args2_type = DATA_INT;
                            bin_ir->result = create_temp();
                            assign_def_id(bin_ir);
                            bin_ir->data_type = inner_last->data_type;
                            IR* result_ir = create_IR(IR_LITERAL);
                            result_ir->result = create_temp();
                            assign_def_id(result_ir);
                            result_ir->args1 = strdup(bin_ir->result);
                            result_ir->args1_type = bin_ir->data_type;
                            result_ir->data_type = inner_last->data_type;
                            IR* chain = connect_IR(inner_ir, one_ir);
                            chain = connect_IR(chain, bin_ir);
                            chain = connect_IR(chain, result_ir);
                            return chain;
                        }
                        IR* one_ir = create_IR(IR_LITERAL);
                        one_ir->result = create_temp();
                        assign_def_id(one_ir);
                        one_ir->args1 = strdup("1");
                        one_ir->args1_type = DATA_INT;
                        one_ir->data_type = DATA_INT;
                        IR* bin_ir = create_IR(IR_BINARY);
                        bin_ir->op = is_inc ? IR_ADD : IR_SUB;
                        bin_ir->args1 = strdup(inner_last->result);
                        bin_ir->args1_type = inner_last->data_type;
                        bin_ir->args2 = strdup(one_ir->result);
                        bin_ir->args2_type = DATA_INT;
                        bin_ir->result = create_temp();
                        assign_def_id(bin_ir);
                        bin_ir->data_type = inner_last->data_type;
                        IR* store_ir = create_IR(IR_VAR_ASS);
                        store_ir->result = strdup(var_name);
                        store_ir->args1 = strdup(bin_ir->result);
                        store_ir->args1_type = bin_ir->data_type;
                        store_ir->data_type = inner_last->data_type;
                        IR* result_ir = create_IR(IR_LITERAL);
                        result_ir->result = create_temp();
                        assign_def_id(result_ir);
                        result_ir->args1 = strdup(bin_ir->result);
                        result_ir->args1_type = bin_ir->data_type;
                        result_ir->data_type = inner_last->data_type;
                        IR* chain = inner_ir;
                        chain = connect_IR(chain, one_ir);
                        chain = connect_IR(chain, bin_ir);
                        chain = connect_IR(chain, store_ir);
                        chain = connect_IR(chain, result_ir);
                        return chain;
                    }
                    return NULL;
                }

                break;
            }

        case AST_CONDITIONAL: {
            AST_Conditional* tern = (AST_Conditional*)ast_node;

            IR* cond_chain = generate_IR(tern->condition);
            IR* cond_last  = cond_chain;
            while (cond_last->next) cond_last = cond_last->next;

            char* else_label = create_label();
            char* end_label  = create_label();
            char* result     = create_temp();

            IR* jz = create_IR(IR_JMPZ);
            jz->args1 = strdup(cond_last->result);
            jz->args2 = strdup(else_label);

            IR* then_chain = generate_IR(tern->then_expr);
            IR* then_last  = then_chain;
            while (then_last->next) then_last = then_last->next;

            IR* assign_then = create_IR(IR_LITERAL);
            assign_then->args1      = strdup(then_last->result);
            assign_then->args1_type = then_last->data_type;
            assign_then->result     = strdup(result);
            assign_def_id(assign_then);
            assign_then->data_type  = tern->resultType;

            IR* jmp_end = create_IR(IR_JMP);
            jmp_end->args1 = strdup(end_label);

            IR* label_else = create_IR(IR_LABEL);
            label_else->args1 = strdup(else_label);

            IR* else_chain = generate_IR(tern->else_expr);
            IR* else_last  = else_chain;
            while (else_last->next) else_last = else_last->next;

            IR* assign_else = create_IR(IR_LITERAL);
            assign_else->args1      = strdup(else_last->result);
            assign_else->args1_type = else_last->data_type;
            assign_else->result     = strdup(result);
            assign_def_id(assign_else);
            assign_else->data_type  = tern->resultType;

            IR* label_end = create_IR(IR_LABEL);
            label_end->args1 = strdup(end_label);

            IR* result_pass = create_IR(IR_LITERAL);
            result_pass->result     = strdup(result);
            assign_def_id(result_pass);
            result_pass->args1      = strdup(result);
            result_pass->args1_type = tern->resultType;
            result_pass->data_type  = tern->resultType;

            IR* chain = cond_chain;
            chain = connect_IR(chain, jz);
            chain = connect_IR(chain, then_chain);
            chain = connect_IR(chain, assign_then);
            chain = connect_IR(chain, jmp_end);
            chain = connect_IR(chain, label_else);
            chain = connect_IR(chain, else_chain);
            chain = connect_IR(chain, assign_else);
            chain = connect_IR(chain, label_end);
            chain = connect_IR(chain, result_pass);

            return chain;
        }

        case AST_ARRAY_DECL:
            {
            AST_ArrayDecl* arr = (AST_ArrayDecl*)ast_node;

            int total_elements = 1;
            for (int i = 0; i < arr->dim_count; i++) {
                if (arr->dim_sizes[i] > 0)
                    total_elements *= arr->dim_sizes[i];
            }

            IR* decl_ir = create_IR(IR_ARRAY_DECL);
            decl_ir->result = strdup(arr->internal_name);
            decl_ir->data_type = arr->elementType;
            decl_ir->array_size = total_elements;
            decl_ir->elem_size = get_type_size(arr->elementType);

            IR* chain = decl_ir;

            if (arr->init_list != NULL) {
                ASTNode** flat_elems = NULL;
                int flat_count = 0, flat_cap = 0;
                flatten_init_list(arr->init_list, &flat_elems, &flat_count, &flat_cap);

                if (flat_count > total_elements) {
                    INTERNAL_ERROR("IR: too many initializers");
                    flat_count = total_elements;
                }

                IR* base_lit = create_IR(IR_LITERAL);
                base_lit->result = create_temp();
                assign_def_id(base_lit);
                base_lit->args1 = strdup(arr->internal_name);
                base_lit->args1_type = DATA_IDENTIFIER;
                base_lit->data_type = DATA_INT;
                chain = connect_IR(chain, base_lit);
                char* base_temp = base_lit->result;

                int elem_sz = get_type_size(arr->elementType);

                for (int k = 0; k < flat_count; k++) {
                    char k_str[16];
                    sprintf(k_str, "%d", k);
                    IR* idx_lit = create_IR(IR_LITERAL);
                    idx_lit->result = create_temp();
                    assign_def_id(idx_lit);
                    idx_lit->args1 = strdup(k_str);
                    idx_lit->args1_type = DATA_INT;
                    idx_lit->data_type = DATA_INT;

                    IR* mul = create_IR(IR_BINARY);
                    mul->op = IR_MUL;
                    mul->args1 = strdup(idx_lit->result);
                    mul->args1_type = DATA_INT;
                    char sz_str[16];
                    sprintf(sz_str, "%d", elem_sz);
                    mul->args2 = strdup(sz_str);
                    mul->args2_type = DATA_INT;
                    mul->result = create_temp();
                    assign_def_id(mul);
                    mul->data_type = DATA_INT;

                    IR* add = create_IR(IR_BINARY);
                    add->op = IR_ADD;
                    add->args1 = strdup(base_temp);
                    add->args1_type = DATA_INT;
                    add->args2 = strdup(mul->result);
                    add->args2_type = DATA_INT;
                    add->result = create_temp();
                    assign_def_id(add);
                    add->data_type = DATA_INT;

                    IR* val_ir = generate_IR(flat_elems[k]);
                    IR* val_last = val_ir;
                    while (val_last->next) val_last = val_last->next;

                    IR* store = create_IR(IR_ARRAY_STORE);
                    store->args1 = strdup(add->result);
                    store->args1_type = DATA_INT;
                    store->args2 = strdup(val_last->result);
                    store->args2_type = val_last->data_type;
                    store->data_type = arr->elementType;
                    store->elem_size = elem_sz;

                    chain = connect_IR(chain, idx_lit);
                    chain = connect_IR(chain, mul);
                    chain = connect_IR(chain, add);
                    chain = connect_IR(chain, val_ir);
                    chain = connect_IR(chain, store);
                }
                free(flat_elems);
            }

            return chain;
        }

    case AST_ARRAY_ACCESS:
        return gen_array_access_ir((AST_ArrayAccess*)ast_node, true, NULL);

    case AST_ARRAY_ASS:
        {
            AST_ArrayAss* ass = (AST_ArrayAss*)ast_node;
            AST_ArrayAccess* acc = (AST_ArrayAccess*)ass->target;

            char* addr_temp = NULL;
            IR* addr_chain = gen_array_access_ir(acc, false, &addr_temp);
            if (!addr_chain || !addr_temp) return NULL;

            IR* val_ir = generate_IR(ass->value);
            IR* val_last = val_ir;
            while (val_last->next) val_last = val_last->next;

            IR* store_ir = create_IR(IR_ARRAY_STORE);
            store_ir->args1 = strdup(addr_temp);
            store_ir->args1_type = DATA_INT;
            store_ir->args2 = strdup(val_last->result);
            store_ir->args2_type = val_last->data_type;
            store_ir->data_type = acc->elementType;
            store_ir->elem_size = get_type_size(acc->elementType);

            IR* chain = addr_chain;
            chain = connect_IR(chain, val_ir);
            chain = connect_IR(chain, store_ir);
            return chain;
        }

    case AST_NEW_ARRAY:
        {
            AST_NewArray* narr = (AST_NewArray*)ast_node;
            int total_elements = 1;
            for (int i = 0; i < narr->dim_count; i++)
                total_elements *= narr->dim_sizes[i];

            IR* alloc_ir = create_IR(IR_ALLOC_ARRAY);
            alloc_ir->result = create_temp();
            assign_def_id(alloc_ir);
            char cnt_str[16];
            sprintf(cnt_str, "%d", total_elements);
            alloc_ir->args1 = strdup(cnt_str);
            alloc_ir->args1_type = DATA_INT;
            alloc_ir->elem_size = get_type_size(narr->elementType);
            alloc_ir->data_type = DATA_INT;
            return alloc_ir;
        }

    case AST_ARRAY_LENGTH:
        {
            AST_ArrayLength* len = (AST_ArrayLength*)ast_node;
            if (len->array->type == AST_ARRAY_ACCESS) {
                AST_ArrayAccess* acc = (AST_ArrayAccess*)len->array;
                if (acc->dim_count > 0 && acc->dim_sizes != NULL) {
                    IR* ir = create_IR(IR_LITERAL);
                    ir->result = create_temp();
                    assign_def_id(ir);
                    char lenStr[16];
                    sprintf(lenStr, "%d", acc->dim_sizes[0]);
                    ir->args1 = strdup(lenStr);
                    ir->args1_type = DATA_INT;
                    ir->data_type = DATA_INT;
                    return ir;
                } else {
                    INTERNAL_ERROR("IR: invalid sub-array in .length");
                }
            }
            AST_Literal* lit = (AST_Literal*)len->array;

            char* internal_name = lit->internal_name;
            if (internal_name == NULL) {
                internal_name = lit->base.value.value;
            }

            IR* len_ir = create_IR(IR_ARRAY_LENGTH);
            len_ir->result = create_temp();
            assign_def_id(len_ir);
            len_ir->args1 = strdup(internal_name);
            len_ir->data_type = DATA_INT;
            return len_ir;
        }

    case AST_PRINT: {
            AST_Print* ast_print = (AST_Print*)ast_node;
            IR* head = NULL;
            IR* tail = NULL;

            ASTNode* param = ast_print->Parameter;
            while (param) {
                IR* param_ir = generate_IR(param);

                IR* last = param_ir;
                while (last->next) last = last->next;

                IR* print_ir = create_IR(IR_PRINT);
                print_ir->args1 = strdup(last->result);
                print_ir->call_func_args_type = malloc(sizeof(DataType));
                print_ir->call_func_args_type[0] = last->data_type;
                print_ir->call_func_args_num = 1;

                if (!head) {
                    head = param_ir;
                    tail = param_ir;
                    while (tail->next) tail = tail->next;
                } else {
                    tail->next = param_ir;
                    while (tail->next) tail = tail->next;
                }
                tail->next = print_ir;
                tail = print_ir;

                param = param->next;
            }
            return head;
    }

    case AST_INPUT:
        {
            AST_Input* inp = (AST_Input*)ast_node;
            IR* fmt_chain = generate_IR(inp->format_string);
            IR* fmt_last = fmt_chain;
            while (fmt_last->next) fmt_last = fmt_last->next;

            IR* ir = create_IR(IR_INPUT);
            ir->args1 = strdup(fmt_last->result);
            ir->call_func_args_num = inp->arg_count;
            ir->call_func_args_type = malloc(sizeof(DataType) * inp->arg_count);
            ir->label = malloc(sizeof(char*) * inp->arg_count);
            for (int i = 0; i < inp->arg_count; i++) {
                AST_Literal* var = (AST_Literal*)inp->args[i];
                ir->call_func_args_type[i] = var->dataType;
                ir->label[i] = strdup(var->internal_name);
            }
            return connect_IR(fmt_chain, ir);
        }
    case AST_VAR:
        AST_Var* ast_var = (AST_Var*)ast_node;
        if (ast_var->is_field_ref && ast_var->Value != NULL) {
            IR* this_ir = create_IR(IR_LOAD_THIS);
            this_ir->result = create_temp();
            assign_def_id(this_ir);
            IR* add_ir = create_IR(IR_BINARY);
            add_ir->op = IR_ADD;
            add_ir->args1 = strdup(this_ir->result);
            add_ir->args1_type = DATA_INT;
            char off_str[16];
            sprintf(off_str, "%d", ast_var->field_offset);
            add_ir->args2 = strdup(off_str);
            add_ir->args2_type = DATA_INT;
            add_ir->result = create_temp();
            assign_def_id(add_ir);
            add_ir->data_type = DATA_INT;
            IR* val_ir = generate_IR(ast_var->Value);
            IR* val_last = val_ir;
            while (val_last->next) val_last = val_last->next;
            IR* store_ir = create_IR(IR_ARRAY_STORE);
            store_ir->args1 = strdup(add_ir->result);
            store_ir->args1_type = DATA_INT;
            store_ir->args2 = strdup(val_last->result);
            store_ir->args2_type = val_last->data_type;
            store_ir->data_type = ast_var->dataType;
            store_ir->elem_size = get_type_size(ast_var->dataType);
            IR* chain = connect_IR(this_ir, add_ir);
            chain = connect_IR(chain, val_ir);
            chain = connect_IR(chain, store_ir);
            return chain;
        }
        if (ast_var->Value && ast_var->Value->type == AST_INIT_LIST)
        {
            AST_InitList* list = (AST_InitList*)ast_var->Value;
            int elem_size = get_type_size(ast_var->dataType);
            int total_bytes = elem_size * list->count;

            IR* alloc = create_IR(IR_ALLOC_ARRAY);
            alloc->result = create_temp();
            assign_def_id(alloc);
            char cnt_str[16];
            sprintf(cnt_str, "%d", list->count);
            alloc->args1 = strdup(cnt_str);
            alloc->args1_type = DATA_INT;
            alloc->elem_size = elem_size;
            alloc->data_type = DATA_INT;

            IR* chain = alloc;
            IR* last = alloc;

            for (int i = 0; i < list->count; i++) {
                IR* idx_lit = create_IR(IR_LITERAL);
                idx_lit->result = create_temp();
                assign_def_id(idx_lit);
                char istr[16];
                sprintf(istr, "%d", i);
                idx_lit->args1 = strdup(istr);
                idx_lit->args1_type = DATA_INT;
                idx_lit->data_type = DATA_INT;

                IR* mul = create_IR(IR_BINARY);
                mul->op = IR_MUL;
                mul->args1 = strdup(idx_lit->result);
                mul->args1_type = DATA_INT;
                char es[16];
                sprintf(es, "%d", elem_size);
                mul->args2 = strdup(es);
                mul->args2_type = DATA_INT;
                mul->result = create_temp();
                assign_def_id(mul);
                mul->data_type = DATA_INT;

                IR* add = create_IR(IR_BINARY);
                add->op = IR_ADD;
                add->args1 = strdup(alloc->result);
                add->args1_type = DATA_INT;
                add->args2 = strdup(mul->result);
                add->args2_type = DATA_INT;
                add->result = create_temp();
                assign_def_id(add);
                add->data_type = DATA_INT;

                IR* elem_ir = generate_IR(list->elements[i]);
                IR* elem_last = elem_ir;
                while (elem_last->next) elem_last = elem_last->next;

                IR* store = create_IR(IR_ARRAY_STORE);
                store->args1 = strdup(add->result);
                store->args1_type = DATA_INT;
                store->args2 = strdup(elem_last->result);
                store->args2_type = elem_last->data_type;
                store->data_type = ast_var->dataType;
                store->elem_size = elem_size;

                chain = connect_IR(chain, idx_lit);
                chain = connect_IR(chain, mul);
                chain = connect_IR(chain, add);
                chain = connect_IR(chain, elem_ir);
                chain = connect_IR(chain, store);
            }

            IR* var_ir = create_IR(IR_VAR);
            var_ir->result = strdup(ast_var->internal_name);
            var_ir->args1 = strdup(alloc->result);
            var_ir->args1_type = DATA_INT;
            var_ir->data_type = DATA_INT;
            chain = connect_IR(chain, var_ir);

            return chain;
        }
        IR* Var_ir = create_IR(IR_VAR);
        IR* Var_Value_IR = generate_IR(ast_var->Value);
        Var_ir->data_type = ast_var->dataType;
        Var_ir->result = strdup(ast_var->internal_name);

        IR* Var_Value_IR_temp = Var_Value_IR;
        while (Var_Value_IR_temp->next != NULL)
            Var_Value_IR_temp = Var_Value_IR_temp->next;

        if (Var_Value_IR_temp->type == IR_LITERAL)
        {
            Var_ir->args1 = strdup(Var_Value_IR_temp->args1);
            Var_ir->args1_type = Var_Value_IR_temp->args1_type;
        }
        else if (Var_Value_IR_temp->type == IR_BINARY ||
                 Var_Value_IR_temp->type == IR_CALL ||
                 Var_Value_IR_temp->type == IR_ARRAY_LOAD)
        {
            Var_ir->args1 = strdup(Var_Value_IR_temp->result);
            Var_ir->args1_type = Var_Value_IR_temp->data_type;
        }
        else
        {
            if (Var_Value_IR_temp->result == NULL && Var_Value_IR_temp->args1 != NULL) {
                Var_ir->args1 = strdup(Var_Value_IR_temp->args1);
            } else {
                Var_ir->args1 = Var_Value_IR_temp->result ? strdup(Var_Value_IR_temp->result) : NULL;
            }
            Var_ir->args1_type = Var_Value_IR_temp->data_type;
        }
        connect_IR(Var_Value_IR, Var_ir);
        return Var_Value_IR;
    case AST_VAR_ASS:
        AST_Var* ast_var_ass = (AST_Var*)ast_node;
        if (ast_var_ass->is_field_ref) {
            IR* this_ir = create_IR(IR_LOAD_THIS);
            this_ir->result = create_temp();
            assign_def_id(this_ir);
            IR* add_ir = create_IR(IR_BINARY);
            add_ir->op = IR_ADD;
            add_ir->args1 = strdup(this_ir->result);
            add_ir->args1_type = DATA_INT;
            char off_str[16];
            sprintf(off_str, "%d", ast_var_ass->field_offset);
            add_ir->args2 = strdup(off_str);
            add_ir->args2_type = DATA_INT;
            add_ir->result = create_temp();
            assign_def_id(add_ir);
            add_ir->data_type = DATA_INT;
            IR* val_ir = generate_IR(ast_var_ass->Value);
            IR* val_last = val_ir;
            while (val_last->next) val_last = val_last->next;
            IR* store_ir = create_IR(IR_ARRAY_STORE);
            store_ir->args1 = strdup(add_ir->result);
            store_ir->args1_type = DATA_INT;
            store_ir->args2 = strdup(val_last->result);
            store_ir->args2_type = val_last->data_type;
            store_ir->data_type = ast_var_ass->dataType;
            store_ir->elem_size = get_type_size(ast_var_ass->dataType);
            IR* chain = connect_IR(this_ir, add_ir);
            chain = connect_IR(chain, val_ir);
            chain = connect_IR(chain, store_ir);
            return chain;
        }
        IR* Var_ir_ass = create_IR(IR_VAR_ASS);
        IR* Var_ass_Value_IR = generate_IR(ast_var_ass->Value);
        Var_ir_ass->data_type = ast_var_ass->dataType;
        Var_ir_ass->result = strdup(ast_var_ass->internal_name);
        IR* Var_ass_Value_IR_temp = Var_ass_Value_IR;
        while (Var_ass_Value_IR_temp->next != NULL)
            Var_ass_Value_IR_temp = Var_ass_Value_IR_temp->next;
                if (Var_ass_Value_IR_temp->type == IR_LITERAL)
        {
            Var_ir_ass->args1 = strdup(Var_ass_Value_IR_temp->args1);
            Var_ir_ass->args1_type = Var_ass_Value_IR_temp->args1_type;
        }
        else if (Var_ass_Value_IR_temp->type == IR_BINARY ||
                 Var_ass_Value_IR_temp->type == IR_CALL ||
                 Var_ass_Value_IR_temp->type == IR_ARRAY_LOAD)
        {
            Var_ir_ass->args1 = strdup(Var_ass_Value_IR_temp->result);
            Var_ir_ass->args1_type = Var_ass_Value_IR_temp->data_type;
        }
        else
        {
            Var_ir_ass->args1 = Var_ass_Value_IR_temp->result ? strdup(Var_ass_Value_IR_temp->result) : NULL;
            Var_ir_ass->args1_type = Var_ass_Value_IR_temp->data_type;
        }
        connect_IR(Var_ass_Value_IR, Var_ir_ass);
        return Var_ass_Value_IR;
    case AST_UNINIT_VAR:
        AST_Var* ast_var_uninit = (AST_Var*)ast_node;
        IR* ir_var_uninit = create_IR(IR_VAR_UNINIT);
        ir_var_uninit->result = ast_var_uninit->internal_name;
        if (ast_var_uninit->dim_count > 0) {
            ir_var_uninit->data_type = DATA_INT;
        } else {
            ir_var_uninit->data_type = ast_var_uninit->dataType;
        }
        return ir_var_uninit;
    case AST_IF:
        AST_If* ast_if = (AST_If*)ast_node;
        IR* ir_chain = NULL;

        char* endif_label = create_label();
        int elif_count = ast_if->elif_Count;
        char** elif_labels = malloc(elif_count * sizeof(char*));
        for (int i = 0; i < elif_count; i++) {
            elif_labels[i] = create_label();
        }
        char* else_label = ast_if->has_else ? create_label() : endif_label;

        if (ast_if->Condition == NULL) {
            INTERNAL_ERROR("IR: if condition is NULL");
        }
        IR* cond_ir = generate_IR(ast_if->Condition);
        ir_chain = cond_ir;
        IR* last = cond_ir;
        while (last->next) last = last->next;
        char* cond_result = last->result;

        IR* jmpz = create_IR(IR_JMPZ);
        jmpz->args1 = strdup(cond_result);
        jmpz->args2 = strdup(elif_count > 0 ? elif_labels[0] : else_label);
        connect_IR(ir_chain, jmpz);

        if (ast_if->code != NULL) {
            IR* then_ir = start_generate_IR(ast_if->code);
            connect_IR(ir_chain, then_ir);
        }
        IR* jmp_to_endif = create_IR(IR_JMP);
        jmp_to_endif->args1 = strdup(endif_label);
        connect_IR(ir_chain, jmp_to_endif);

        for (int i = 0; i < elif_count; i++) {
            IR* label = create_IR(IR_LABEL);
            label->args1 = strdup(elif_labels[i]);
            connect_IR(ir_chain, label);

            if (ast_if->elif_condition[i] == NULL) {
                INTERNAL_ERROR("IR: elif condition %d is NULL", i);
            }
            IR* elif_cond_ir = generate_IR(ast_if->elif_condition[i]);
            connect_IR(ir_chain, elif_cond_ir);
            last = elif_cond_ir;
            while (last->next) last = last->next;
            char* elif_cond_result = last->result;

            char* next_label = (i+1 < elif_count) ? elif_labels[i+1] : else_label;
            IR* jmpz_elif = create_IR(IR_JMPZ);
            jmpz_elif->args1 = strdup(elif_cond_result);
            jmpz_elif->args2 = strdup(next_label);
            connect_IR(ir_chain, jmpz_elif);

            if (ast_if->Elif[i] != NULL) {
                IR* elif_code_ir = start_generate_IR(ast_if->Elif[i]);
                connect_IR(ir_chain, elif_code_ir);
            }
            IR* jmp_elif_to_endif = create_IR(IR_JMP);
            jmp_elif_to_endif->args1 = strdup(endif_label);
            connect_IR(ir_chain, jmp_elif_to_endif);
        }

        if (ast_if->has_else) {
            IR* else_label_ir = create_IR(IR_LABEL);
            else_label_ir->args1 = strdup(else_label);
            connect_IR(ir_chain, else_label_ir);

            if (ast_if->Else != NULL) {
                IR* else_ir = start_generate_IR(ast_if->Else);
                connect_IR(ir_chain, else_ir);
            }
        }

        IR* endif_label_ir = create_IR(IR_LABEL);
        endif_label_ir->args1 = strdup(endif_label);
        connect_IR(ir_chain, endif_label_ir);

        free(elif_labels);
        return ir_chain;

        case AST_SWITCH:
            {
                AST_Switch* sw = (AST_Switch*)ast_node;

                IR* cond_chain = generate_IR(sw->condition);
                IR* cond_last = cond_chain;
                while (cond_last->next) cond_last = cond_last->next;
                char* cond_temp = cond_last->result;

                IR* chain = cond_chain;

                char* end_label = create_label();
                push_context(end_label, NULL);

                char** case_labels = malloc(sizeof(char*) * sw->case_count);
                for (int i = 0; i < sw->case_count; i++) {
                    case_labels[i] = create_label();
                    AST_Case* cs = sw->cases[i];
                    if (cs->is_default) continue;

                    IR* val_ir = generate_IR(cs->value);
                    IR* val_last = val_ir;
                    while (val_last->next) val_last = val_last->next;
                    char* val_temp = val_last->result;
                    chain = connect_IR(chain, val_ir);

                    IR* cmp = create_IR(IR_BINARY);
                    cmp->op = IR_JE;
                    cmp->args1 = strdup(cond_temp);
                    cmp->args1_type = cond_last->data_type;
                    cmp->args2 = strdup(val_temp);
                    cmp->args2_type = val_last->data_type;
                    cmp->result = create_temp();
                    assign_def_id(cmp);
                    cmp->data_type = DATA_BOOL;
                    chain = connect_IR(chain, cmp);

                    IR* jmp = create_IR(IR_JMPNZ);
                    jmp->args1 = strdup(cmp->result);
                    jmp->args2 = strdup(case_labels[i]);
                    chain = connect_IR(chain, jmp);
                }

                char* default_label = NULL;
                if (sw->has_default) {
                    default_label = create_label();
                }
                IR* jmp_no_match = create_IR(IR_JMP);
                jmp_no_match->args1 = strdup(sw->has_default ? default_label : end_label);
                chain = connect_IR(chain, jmp_no_match);

                for (int i = 0; i < sw->case_count; i++) {
                    AST_Case* cs = sw->cases[i];
                    if (cs->is_default) continue;

                    IR* lbl = create_IR(IR_LABEL);
                    lbl->args1 = strdup(case_labels[i]);
                    chain = connect_IR(chain, lbl);

                    if (cs->body) {
                        IR* body_ir = start_generate_IR(cs->body);
                        chain = connect_IR(chain, body_ir);
                    }
                }
                free(case_labels);

                if (sw->has_default) {
                    IR* def_lbl = create_IR(IR_LABEL);
                    def_lbl->args1 = strdup(default_label);
                    chain = connect_IR(chain, def_lbl);

                    if (sw->default_body) {
                        IR* def_ir = start_generate_IR(sw->default_body);
                        chain = connect_IR(chain, def_ir);
                    }
                }

                IR* end_lbl = create_IR(IR_LABEL);
                end_lbl->args1 = strdup(end_label);
                chain = connect_IR(chain, end_lbl);

                pop_context();
                return chain;
            }

    case AST_WHILE:
        AST_While* ast_while = (AST_While*)ast_node;
        if (ast_while->has_do) {
            IR* chain = NULL;

            char* end_label = create_label();
            char* start_label = create_label();
            push_context(end_label, start_label);
            IR* label_start = create_IR(IR_LABEL);
            label_start->args1 = strdup(start_label);
            chain = label_start;

            if (ast_while->code != NULL) {
                IR* body_ir = start_generate_IR(ast_while->code);
                chain = connect_IR(chain, body_ir);
            }

            IR* cond_ir = generate_IR(ast_while->Condition);
            IR* cond_last = cond_ir;
            while (cond_last->next) cond_last = cond_last->next;
            chain = connect_IR(chain, cond_ir);

            IR* jmpnz = create_IR(IR_JMPNZ);
            jmpnz->args1 = strdup(cond_last->result);
            jmpnz->args2 = strdup(start_label);
            chain = connect_IR(chain, jmpnz);

            IR* label_end = create_IR(IR_LABEL);
            label_end->args1 = strdup(end_label);
            chain = connect_IR(chain, label_end);

            pop_context();
            return chain;
        }
        IR* while_condition_ir = generate_IR(ast_while->Condition);
        IR* while_last_ir = while_condition_ir;
        while (while_last_ir->next != NULL)
        {
            while_last_ir = while_last_ir->next;
        }

        IR* while_jmpz = create_IR(IR_JMPZ);
        while_jmpz->args1 = strdup(while_last_ir->result);
        char* while_end_label = create_label();
        while_jmpz->args2 = strdup(while_end_label);
        char* while_start_label = create_label();
        push_context(while_end_label, while_start_label);
        IR* while_start_ir = create_IR(IR_LABEL);
        while_start_ir->args1 = strdup(while_start_label);
        connect_IR(while_start_ir,while_condition_ir);
        connect_IR(while_condition_ir, while_jmpz);

        if (ast_while->code != NULL)
        {
            IR* while_code_ir = start_generate_IR(ast_while->code);
            connect_IR(while_condition_ir, while_code_ir);
        }

        IR* while_code_ir = create_IR(IR_JMP);
        while_code_ir->args1 = strdup(while_start_label);
        connect_IR(while_condition_ir, while_code_ir);

        IR* while_end_ir = create_IR(IR_LABEL);
        while_end_ir->args1 = strdup(while_end_label);
        connect_IR(while_condition_ir, while_end_ir);
        pop_context();
        return while_start_ir;

    case AST_FOR:
        {
            AST_For* for_node = (AST_For*)ast_node;
            IR* chain = NULL;

            if (for_node->init) {
                chain = start_generate_IR(for_node->init);
            }

            char* start_label = create_label();
            char* end_label   = create_label();
            char* cont_label  = create_label();
            push_context(end_label, cont_label);

            IR* label_start = create_IR(IR_LABEL);
            label_start->args1 = strdup(start_label);
            chain = connect_IR(chain, label_start);

            if (for_node->condition) {
                IR* cond_ir = generate_IR(for_node->condition);
                IR* cond_last = cond_ir;
                while (cond_last->next) cond_last = cond_last->next;
                chain = connect_IR(chain, cond_ir);

                IR* jmpz = create_IR(IR_JMPZ);
                jmpz->args1 = strdup(cond_last->result);
                jmpz->args2 = strdup(end_label);
                chain = connect_IR(chain, jmpz);
            }

            if (for_node->code) {
                IR* body_ir = start_generate_IR(for_node->code);
                chain = connect_IR(chain, body_ir);
            }

            IR* cont_lbl_ir = create_IR(IR_LABEL);
            cont_lbl_ir->args1 = strdup(cont_label);
            chain = connect_IR(chain, cont_lbl_ir);

            if (for_node->increment) {
                IR* incr_ir = start_generate_IR(for_node->increment);
                chain = connect_IR(chain, incr_ir);
            }

            IR* jmp_back = create_IR(IR_JMP);
            jmp_back->args1 = strdup(start_label);
            chain = connect_IR(chain, jmp_back);

            IR* label_end = create_IR(IR_LABEL);
            label_end->args1 = strdup(end_label);
            chain = connect_IR(chain, label_end);

            pop_context();
            return chain;
        }

    case AST_FOREACH:
        {
            AST_ForEach* foreach_node = (AST_ForEach*)ast_node;
            IR* chain = NULL;

            IR* iter_decl = create_IR(IR_VAR_UNINIT);
            iter_decl->result = strdup(foreach_node->internal_iterator);
            iter_decl->data_type = foreach_node->elemType;
            chain = iter_decl;

            IR* array_ir = generate_IR(foreach_node->array);
            IR* array_last = array_ir;
            while (array_last->next) array_last = array_last->next;
            char* array_base = array_last->result;
            chain = connect_IR(chain, array_ir);

            char* length_temp = NULL;
            if (foreach_node->array->type == AST_ARRAY_ACCESS) {
                AST_ArrayAccess* acc = (AST_ArrayAccess*)foreach_node->array;
                if (acc->dim_count > 0 && acc->dim_sizes != NULL) {
                    char len_str[16];
                    sprintf(len_str, "%d", acc->dim_sizes[0]);
                    IR* len_lit = create_IR(IR_LITERAL);
                    len_lit->result = create_temp();
                    assign_def_id(len_lit);
                    len_lit->args1 = strdup(len_str);
                    len_lit->args1_type = DATA_INT;
                    len_lit->data_type = DATA_INT;
                    chain = connect_IR(chain, len_lit);
                    length_temp = len_lit->result;
                } else {
                    INTERNAL_ERROR("IR: foreach subarray has unknown size");
                }
            } else {
                AST_Literal* lit = (AST_Literal*)foreach_node->array;
                const char* internal_name = lit->internal_name;
                if (internal_name == NULL) {
                    internal_name = lit->base.value.value;
                }
                IR* len_ir = create_IR(IR_ARRAY_LENGTH);
                len_ir->result = create_temp();
                assign_def_id(len_ir);
                len_ir->args1 = strdup(internal_name);
                len_ir->data_type = DATA_INT;
                chain = connect_IR(chain, len_ir);
                length_temp = len_ir->result;
            }

            char idx_var_name[32];
            snprintf(idx_var_name, sizeof(idx_var_name),
                     "_foreach_idx%d", foreach_idx_counter++);

            IR* idx_init = create_IR(IR_VAR);
            idx_init->result = strdup(idx_var_name);
            idx_init->args1 = strdup("0");
            idx_init->args1_type = DATA_INT;
            idx_init->data_type = DATA_INT;
            chain = connect_IR(chain, idx_init);

            char* start_label = create_label();
            char* end_label   = create_label();
            char* continue_label = create_label();
            push_context(end_label, continue_label);

            IR* label_start = create_IR(IR_LABEL);
            label_start->args1 = strdup(start_label);
            chain = connect_IR(chain, label_start);

            IR* idx_load = create_IR(IR_LITERAL);
            idx_load->result = create_temp();
            assign_def_id(idx_load);
            idx_load->args1 = strdup(idx_var_name);
            idx_load->args1_type = DATA_INT;
            idx_load->data_type = DATA_INT;
            chain = connect_IR(chain, idx_load);

            IR* cmp = create_IR(IR_BINARY);
            cmp->op = IR_JL;
            cmp->args1 = strdup(idx_load->result);
            cmp->args1_type = DATA_INT;
            cmp->args2 = strdup(length_temp);
            cmp->args2_type = DATA_INT;
            cmp->result = create_temp();
            assign_def_id(cmp);
            cmp->data_type = DATA_BOOL;
            chain = connect_IR(chain, cmp);

            IR* jmpz = create_IR(IR_JMPZ);
            jmpz->args1 = strdup(cmp->result);
            jmpz->args2 = strdup(end_label);
            chain = connect_IR(chain, jmpz);

            int elem_size = get_type_size(foreach_node->elemType);

            IR* idx_val = create_IR(IR_LITERAL);
            idx_val->result = create_temp();
            assign_def_id(idx_val);
            idx_val->args1 = strdup(idx_var_name);
            idx_val->args1_type = DATA_INT;
            idx_val->data_type = DATA_INT;
            chain = connect_IR(chain, idx_val);

            IR* mul = create_IR(IR_BINARY);
            mul->op = IR_MUL;
            mul->args1 = strdup(idx_val->result);
            mul->args1_type = DATA_INT;
            char esz[16];
            sprintf(esz, "%d", elem_size);
            mul->args2 = strdup(esz);
            mul->args2_type = DATA_INT;
            mul->result = create_temp();
            assign_def_id(mul);
            mul->data_type = DATA_INT;
            chain = connect_IR(chain, mul);

            IR* add = create_IR(IR_BINARY);
            add->op = IR_ADD;
            add->args1 = strdup(array_base);
            add->args1_type = DATA_INT;
            add->args2 = strdup(mul->result);
            add->args2_type = DATA_INT;
            add->result = create_temp();
            assign_def_id(add);
            add->data_type = DATA_INT;
            chain = connect_IR(chain, add);

            IR* load = create_IR(IR_ARRAY_LOAD);
            load->args1 = strdup(add->result);
            load->args1_type = DATA_INT;
            load->result = create_temp();
            assign_def_id(load);
            load->data_type = foreach_node->elemType;
            load->elem_size = elem_size;
            chain = connect_IR(chain, load);

            IR* assign = create_IR(IR_VAR_ASS);
            assign->result = strdup(foreach_node->internal_iterator);
            assign->args1 = strdup(load->result);
            assign->args1_type = load->data_type;
            assign->data_type = load->data_type;
            chain = connect_IR(chain, assign);

            if (foreach_node->code) {
                IR* body_ir = start_generate_IR(foreach_node->code);
                chain = connect_IR(chain, body_ir);
            }

            IR* cont_lbl = create_IR(IR_LABEL);
            cont_lbl->args1 = strdup(continue_label);
            chain = connect_IR(chain, cont_lbl);

            IR* inc_load = create_IR(IR_LITERAL);
            inc_load->result = create_temp();
            assign_def_id(inc_load);
            inc_load->args1 = strdup(idx_var_name);
            inc_load->args1_type = DATA_INT;
            inc_load->data_type = DATA_INT;
            chain = connect_IR(chain, inc_load);

            IR* inc = create_IR(IR_BINARY);
            inc->op = IR_ADD;
            inc->args1 = strdup(inc_load->result);
            inc->args1_type = DATA_INT;
            inc->args2 = strdup("1");
            inc->args2_type = DATA_INT;
            inc->result = create_temp();
            assign_def_id(inc);
            inc->data_type = DATA_INT;
            chain = connect_IR(chain, inc);

            IR* inc_assign = create_IR(IR_VAR_ASS);
            inc_assign->result = strdup(idx_var_name);
            inc_assign->args1 = strdup(inc->result);
            inc_assign->args1_type = DATA_INT;
            inc_assign->data_type = DATA_INT;
            chain = connect_IR(chain, inc_assign);

            IR* jmp_back = create_IR(IR_JMP);
            jmp_back->args1 = strdup(start_label);
            chain = connect_IR(chain, jmp_back);

            IR* label_end = create_IR(IR_LABEL);
            label_end->args1 = strdup(end_label);
            chain = connect_IR(chain, label_end);

            pop_context();
            return chain;
        }

    case AST_BREAK:
        {
            const char* label = current_break_label();
            if (!label) {
                INTERNAL_ERROR("IR: break outside loop/switch");
            }
            IR* jmp = create_IR(IR_JMP);
            jmp->args1 = strdup(label);
            return jmp;
        }

    case AST_CONTINUE:
        {
            const char* label = current_continue_label();
            if (!label) {
                INTERNAL_ERROR("IR: continue outside loop");
            }
            IR* jmp = create_IR(IR_JMP);
            jmp->args1 = strdup(label);
            return jmp;
        }

    case AST_FUNCTION:
        {
            int saved_temp_i = temp_i;
            int saved_label_i = label_i;
            temp_i = 0;
            label_i = 0;

            AST_Function* ast_function = (AST_Function*)ast_node;

            int func_local_count = count_local_vars(ast_function->Code);
            IR* start_function_ir = create_IR(IR_LABEL);
            if (strcmp(ast_function->name.value, "main") == 0)
                start_function_ir->args1 = "_main";
            else
                start_function_ir->args1 = strdup(ast_function->internal_name);
            start_function_ir->has_return = true;
            start_function_ir->func_local_count = func_local_count;
            IR* func_entry = start_function_ir;

            IR* param_chain = NULL;
            IR* last_param = NULL;
            ASTNode* param_node = ast_function->Parameter;
            while (param_node) {
                char* internal_name = NULL;
                DataType param_type = DATA_ERROR;

                if (param_node->type == AST_VAR || param_node->type == AST_UNINIT_VAR) {
                    AST_Var* var = (AST_Var*)param_node;
                    internal_name = var->internal_name;
                    param_type = var->dataType;
                } else if (param_node->type == AST_ARRAY_DECL) {
                    AST_ArrayDecl* arr = (AST_ArrayDecl*)param_node;
                    internal_name = arr->internal_name;
                    param_type = arr->elementType;
                } else {
                    INTERNAL_ERROR("IR: unexpected parameter node type");
                }

                IR* param_def = create_IR(IR_PARAM_DEF);
                param_def->result = strdup(internal_name);
                param_def->data_type = param_type;
                param_def->next = NULL;

                if (param_chain == NULL) {
                    param_chain = param_def;
                    last_param = param_def;
                } else {
                    last_param->next = param_def;
                    last_param = param_def;
                }
                param_node = param_node->next;
            }

            IR* func_body_ir = NULL;
            if (ast_function->Code != NULL) {
                func_body_ir = start_generate_IR(ast_function->Code);
            }
            if (ast_function->Return != NULL && ast_function->Code == NULL) {
                IR* ret_ir = generate_IR((ASTNode*)ast_function->Return);
                if (func_body_ir) {
                    func_body_ir = connect_IR(func_body_ir, ret_ir);
                } else {
                    func_body_ir = ret_ir;
                }
            }

            IR* func_last = func_body_ir;
            while (func_last && func_last->next) func_last = func_last->next;
            if (func_last == NULL || func_last->type != IR_RET) {
                IR* default_ret = create_IR(IR_RET);
                default_ret->has_return = false;
                func_body_ir = connect_IR(func_body_ir, default_ret);
            }

            start_function_ir->next = param_chain;
            IR* full_chain = start_function_ir;
            if (param_chain) {
                last_param->next = func_body_ir;
            } else {
                start_function_ir->next = func_body_ir;
            }

            func_entry->func_temp_count = temp_i;

            label_i = saved_label_i;
            temp_i = saved_temp_i;

            return full_chain;
        }

    case AST_CLASS_DEF: {
            AST_ClassDef* cls = (AST_ClassDef*)ast_node;
            IR* chain = NULL;
            ASTNode* m = cls->members;
            while (m) {
                if (m->type == AST_FUNCTION) {
                    ASTNode* saved_next = m->next;
                    m->next = NULL;
                    IR* method_ir = generate_IR(m);
                    m->next = saved_next;
                    chain = connect_IR(chain, method_ir);
                } else if (m->type == AST_CLASS_DEF) {
                    ASTNode* saved_next = m->next;
                    m->next = NULL;
                    IR* nested_ir = generate_IR(m);
                    m->next = saved_next;
                    chain = connect_IR(chain, nested_ir);
                }
                m = m->next;
            }
            if (chain == NULL) {
                IR* placeholder = create_IR(IR_LITERAL);
                placeholder->result = create_temp();
                assign_def_id(placeholder);
                placeholder->args1 = strdup("0");
                placeholder->args1_type = DATA_INT;
                placeholder->data_type = DATA_INT;
                chain = placeholder;
            }
            return chain;
    }

    case AST_NEW_INSTANCE: {
            AST_NewInstance* new_inst = (AST_NewInstance*)ast_node;

            IR* alloc_ir = create_IR(IR_ALLOC_CLASS);
            alloc_ir->result = create_temp();
            assign_def_id(alloc_ir);
            char size_str[16];
            sprintf(size_str, "%d", new_inst->instance_size);
            alloc_ir->args1 = strdup(size_str);
            alloc_ir->args1_type = DATA_INT;
            alloc_ir->data_type = DATA_INT;

            IR* chain = alloc_ir;
            IR* last = alloc_ir;

            char* obj_temp = strdup(alloc_ir->result);

            for (int i = 0; i < new_inst->field_init_count; i++) {
                FieldInit* fi = &new_inst->field_inits[i];

                IR* add_ir = create_IR(IR_BINARY);
                add_ir->op = IR_ADD;
                add_ir->args1 = strdup(obj_temp);
                add_ir->args1_type = DATA_INT;
                char off_str[16];
                sprintf(off_str, "%d", fi->offset);
                add_ir->args2 = strdup(off_str);
                add_ir->args2_type = DATA_INT;
                add_ir->result = create_temp();
                assign_def_id(add_ir);
                add_ir->data_type = DATA_INT;

                char* init_val = fi->value_str ? strdup(fi->value_str) : strdup("0");
                IR* val_ir = create_IR(IR_LITERAL);
                val_ir->result = create_temp();
                assign_def_id(val_ir);
                val_ir->args1 = init_val;
                val_ir->args1_type = fi->type;
                val_ir->data_type = fi->type;

                IR* store_ir = create_IR(IR_ARRAY_STORE);
                store_ir->args1 = strdup(add_ir->result);
                store_ir->args1_type = DATA_INT;
                store_ir->args2 = strdup(val_ir->result);
                store_ir->args2_type = fi->type;
                store_ir->data_type = fi->type;
                store_ir->elem_size = get_type_size(fi->type);

                last->next = add_ir;
                add_ir->next = val_ir;
                val_ir->next = store_ir;
                last = store_ir;
            }

            if (new_inst->has_constructor && new_inst->constructor_name) {
                int total_args = 1 + new_inst->constructor_arg_count;
                char** arg_temps = malloc(sizeof(char*) * total_args);
                DataType* arg_types = malloc(sizeof(DataType) * total_args);

                arg_temps[0] = strdup(obj_temp);
                arg_types[0] = DATA_INT;

                ASTNode* arg_node = new_inst->arguments;
                int idx = 1;
                while (arg_node && idx < total_args) {
                    IR* arg_ir = generate_IR(arg_node);
                    if (!arg_ir) {
                        for (int j = 0; j < idx; j++) free(arg_temps[j]);
                        free(arg_temps); free(arg_types);
                        free(obj_temp);
                        return chain;
                    }
                    IR* arg_last = arg_ir;
                    while (arg_last->next) arg_last = arg_last->next;

                    arg_temps[idx] = strdup(arg_last->result);
                    arg_types[idx] = arg_last->data_type;

                    last->next = arg_ir;
                    last = arg_last;
                    idx++;
                    arg_node = arg_node->next;
                }

                for (int i = total_args - 1; i >= 0; i--) {
                    IR* push_ir = create_IR(IR_PUSH);
                    push_ir->args1 = strdup(arg_temps[i]);
                    push_ir->args1_type = arg_types[i];
                    last->next = push_ir;
                    last = push_ir;
                }

                IR* call_ir = create_IR(IR_CALL);
                call_ir->args1 = strdup(new_inst->constructor_name);
                call_ir->result = create_temp();
                assign_def_id(call_ir);
                call_ir->data_type = DATA_VOID;
                call_ir->call_func_args_num = total_args;
                last->next = call_ir;
                last = call_ir;

                for (int i = 0; i < total_args; i++) free(arg_temps[i]);
                free(arg_temps);
                free(arg_types);
            }

            {
                IR* pass = create_IR(IR_LITERAL);
                pass->result = create_temp();
                assign_def_id(pass);
                pass->args1 = strdup(obj_temp);
                pass->args1_type = DATA_INT;
                pass->data_type = DATA_INT;

                if (last) {
                    last->next = pass;
                    last = pass;
                } else {
                    chain = pass;
                    last = pass;
                }
            }

            free(obj_temp);
            return chain;
        }

    case AST_MEMBER_ACCESS:
        {
            AST_MemberAccess* access = (AST_MemberAccess*)ast_node;
            if (access->is_method) {
                return NULL;
            }
            if (access->object->type == AST_LITERAL) {
                AST_Literal* obj_lit = (AST_Literal*)access->object;
                if (obj_lit->base.value.type == TOKEN_IDENTIFIER
                    && !obj_lit->is_call_function
                    && obj_lit->internal_name == NULL) {
                    IR* lit = create_IR(IR_LITERAL);
                    lit->result = create_temp();
                    assign_def_id(lit);
                    char val_str[16];
                    snprintf(val_str, sizeof(val_str), "%d",
                             access->member_offset);
                    lit->args1 = strdup(val_str);
                    lit->args1_type = DATA_INT;
                    lit->data_type = access->member_type;
                    return lit;
                    }
            }
            IR* obj_chain = generate_IR(access->object);
            IR* obj_last = obj_chain;
            while (obj_last->next) obj_last = obj_last->next;
            IR* load = create_IR(IR_LOAD_MEMBER);
            load->result = create_temp();
            assign_def_id(load);
            load->args1 = strdup(obj_last->result);
            char off_str[16];
            sprintf(off_str, "%d", access->member_offset);
            load->args2 = strdup(off_str);
            load->data_type = access->member_type;
            return connect_IR(obj_chain, load);
        }

    case AST_THIS:
        {
            IR* ir = create_IR(IR_LOAD_THIS);
            ir->result = create_temp();
            assign_def_id(ir);
            ir->data_type = DATA_INT;
            ir->args1_type = DATA_INT;
            return ir;
        }

    case AST_MEMBER_ASS: {
            AST_MemberAss* ass = (AST_MemberAss*)ast_node;
            IR* obj_ir = generate_IR(ass->object);
            IR* obj_last = obj_ir;
            while (obj_last->next) obj_last = obj_last->next;
            char* obj_temp = obj_last->result;

            int offset = ass->member_offset;
            char offset_str[16];
            sprintf(offset_str, "%d", offset);

            IR* add_ir = create_IR(IR_BINARY);
            add_ir->op = IR_ADD;
            add_ir->args1 = strdup(obj_temp);
            add_ir->args1_type = DATA_INT;
            add_ir->args2 = strdup(offset_str);
            add_ir->args2_type = DATA_INT;
            add_ir->result = create_temp();
            assign_def_id(add_ir);
            add_ir->data_type = DATA_INT;

            IR* val_ir = generate_IR(ass->value);
            IR* val_last = val_ir;
            while (val_last->next) val_last = val_last->next;
            char* val_temp = val_last->result;

            IR* store = create_IR(IR_ARRAY_STORE);
            store->args1 = strdup(add_ir->result);
            store->args2 = strdup(val_temp);
            store->data_type = ass->member_type;
            store->elem_size = get_type_size(ass->member_type);
            IR* chain = connect_IR(obj_ir, add_ir);
            chain = connect_IR(chain, val_ir);
            chain = connect_IR(chain, store);
            return chain;
    }

    case AST_RETURN:
        AST_Return* ast_ret = (AST_Return*)ast_node;
        IR* ret_ir = create_IR(IR_RET);
        if (ast_ret->value != NULL) {
            IR* val_ir = generate_IR(ast_ret->value);
            IR* last = val_ir;
            while (last->next) last = last->next;
            ret_ir->args1 = strdup(last->result);
            ret_ir->has_return = true;
            ret_ir->data_type = ast_ret->returnType;
            return connect_IR(val_ir, ret_ir);
        } else {
            ret_ir->has_return = false;
            ret_ir->data_type = ast_ret->returnType;
            return ret_ir;
        }

    case AST_ENUM_DEF:
    case AST_ENUM_MEMBER:
    case AST_ERROR:
        return NULL;

    default:
        INTERNAL_ERROR("IR: unhandled AST type %d", ast_node->type);
        return NULL;
    }
}

//注：这里为SSA扩展
IR* ir_insert_after(IR* after, IR* new_ir) {
    if (!new_ir) return NULL;
    if (!after) return new_ir;
    new_ir->next = after->next;
    after->next  = new_ir;
    return new_ir;
}

IR* ir_tail(IR* head) {
    if (!head) return NULL;
    while (head->next) head = head->next;
    return head;
}

IR* ir_create_phi(const char* result_name, int arg_count,
                  char** pred_labels, char** arg_names) {
    IR* ir = calloc(1, sizeof(IR));
    if (!ir) OUT_OF_MEMORY();
    ir->type           = IR_PHI;
    ir->result         = result_name ? strdup(result_name) : NULL;
    ir->data_type      = DATA_INT;
    ir->phi_arg_count  = arg_count;

    ir->label_count = arg_count;
    if (arg_count > 0) {
        ir->label = calloc((size_t)arg_count, sizeof(char*));
        for (int i = 0; i < arg_count; i++) {
            if (pred_labels)
                ir->label[i] = strdup(pred_labels[i]);
            else
                ir->label[i] = NULL;
        }
    }

    if (arg_count > 0) {
        ir->phi_arg_names = calloc((size_t)arg_count, sizeof(char*));
        if (arg_names) {
            for (int i = 0; i < arg_count; i++) {
                if (arg_names[i])
                    ir->phi_arg_names[i] = strdup(arg_names[i]);
                else
                    ir->phi_arg_names[i] = NULL;
            }
        }
    }
    return ir;
}

void ir_phi_set_arg(IR* phi, int pred_index, const char* ssa_name) {
    if (!phi || phi->type != IR_PHI) return;
    if (pred_index < 0 || pred_index >= phi->phi_arg_count) return;
    if (phi->phi_arg_names[pred_index])
        free(phi->phi_arg_names[pred_index]);
    phi->phi_arg_names[pred_index] = ssa_name ? strdup(ssa_name) : NULL;
}

char* IR_OP_to_string(IR_OP_Type type)
{
    switch (type)
    {
    case IR_ADD:  return "+";
    case IR_SUB:  return "-";
    case IR_MUL:  return "*";
    case IR_DIV:  return "/";
    case IR_MOD: return "%";
    case IR_JE:   return "==";
    case IR_JNE:   return "!=";
    case IR_JL:   return "<";
    case IR_JG:   return ">";
    case IR_JLE:   return "<=";
    case IR_JGE:   return ">=";
    case IR_UDIV: return "u/";
    case IR_UMOD: return "u%";
    case IR_ULT:  return "u<";
    case IR_ULE:  return "u<=";
    case IR_UGT:  return "u>";
    case IR_UGE:  return "u>=";
    case IR_SHL: return "<<";
    case IR_SHR: return ">>";
    case IR_SAR: return ">>>";
    case IR_BITWISE_XOR: return "^";
    default:      return "unknown";
    }
}

void print_IR(IR* ir) {
    if (ir == NULL) {
        INTERNAL_ERROR("IR: print_IR called with NULL");
    }

    IR* temp = ir;
    int ir_idx = 0;
    while (temp != NULL) {
        printf("IR[%d]: ", ir_idx++);
        switch (temp->type) {
        case IR_LITERAL:
            printf("Literal(type:%d,def:%d): %s = %s\n",temp->data_type,temp->def_id, temp->result, temp->args1);
            break;
        case IR_BINARY:
            printf("Binary(type:%d,def:%d): %s = %s(type:%d) %s %s(type:%d)\n",
                temp->data_type,temp->def_id, temp->result,
                   temp->args1, temp->args1_type,
                   IR_OP_to_string(temp->op), temp->args2,temp->args2_type);
            break;
        case IR_UNARY:
            printf("Unary: op=%d, %s = %s %s\n",
                   temp->op, temp->result, IR_OP_to_string(temp->op), temp->args1);
            break;
        case IR_ARRAY_DECL:
            printf("ArrayDecl(type:%d, size:%d, elem_bytes:%d): %s\n",
                   temp->data_type, temp->array_size, temp->elem_size, temp->result);
            break;
        case IR_ARRAY_LOAD:
            printf("ArrayLoad(type:%d, elem_bytes:%d, def:%d): %s = [%s]\n",
                   temp->data_type, temp->elem_size, temp->def_id, temp->result, temp->args1);
            break;
        case IR_ARRAY_STORE:
            printf("ArrayStore(type:%d, elem_bytes:%d): [%s] = %s\n",
                   temp->data_type, temp->elem_size, temp->args1, temp->args2);
            break;
        case IR_ALLOC_ARRAY:
            printf("AllocArray(type:%d, elem_size:%d, def:%d): %s = new [%s]\n",
                   temp->data_type, temp->elem_size, temp->def_id, temp->result, temp->args1);
            break;
        case IR_ARRAY_LENGTH:
            printf("ArrayLength(def:%d): %s = length(%s)\n", temp->def_id, temp->result, temp->args1);
            break;
        case IR_ARRAY_TOSTRING:
            printf("ArrayToString(def:%d): %s = array_to_string(%s, %s)\n", temp->def_id, temp->result, temp->args1, temp->args2);
            break;
        case IR_PRINT:
            printf("Print parameter: %s\n",temp->args1);
            break;
        case IR_FORMAT:
            printf("Format(def:%d): %s\n", temp->def_id, temp->result);
            break;
        case IR_INPUT:
            printf("Input: %s\n",temp->result);
            break;
        case IR_VAR:
            printf("Var(type:%d): %s = %s(type:%d)\n",temp->data_type, temp->result, temp->args1,temp->args1_type);
            break;
        case IR_VAR_ASS:
            printf("Var_ass(type:%d): %s = %s(type:%d)\n",temp->data_type,temp->result , temp->args1,temp->args1_type);
            break;
        case IR_VAR_UNINIT:
            printf("Var_uninit(type:%d): %s\n",temp->data_type,temp->result);
            break;
        case IR_IF:
            printf("If: label_count=%d, labels=[", temp->label_count);
            for (int i = 0; i < temp->label_count; i++) {
                if (i > 0) printf(", ");
                printf("%s", temp->label[i]);
            }
            printf("]\n");
            break;
        case IR_PUSH:
            printf("push: %s\n",temp->args1);
            break;
        case IR_PARAM_DEF:
            printf("param_def: %s\n",temp->result);
            break;
        case IR_LABEL:
            printf("Label: %s\n", temp->args1);
            break;
        case IR_JMP:
            printf("JMP: %s\n", temp->args1);
            break;
        case IR_JMPZ:
            printf("JMPZ: if %s == 0 goto %s\n", temp->args1, temp->args2);
            break;
        case IR_JMPNZ:
            printf("JMPNZ: if %s != 0 goto %s\n", temp->args1, temp->args2);
            break;
        case IR_CALL:
            printf("Call(def:%d): %s\n", temp->def_id, temp->args1);
            break;
        case IR_RET:
            if (temp->has_return)
            {
                printf("Return: %s\n", temp->args1);
            } else
            {
                printf("Return:\n");
            }
            break;
        case IR_BOUNDS_CHECK:
            printf("BoundsCheck: %s < %s\n", temp->args1, temp->args2);
            break;
        case IR_BITWISE_NOT:
            printf("BitwiseNOT(def:%d): %s = ~%s (type:%d)\n", temp->def_id, temp->result, temp->args1, temp->data_type);
            break;
        case IR_ALLOC_CLASS:
            printf("AllocClass(size:%s, def:%d): %s\n", temp->args1, temp->def_id, temp->result);
            break;
        case IR_LOAD_MEMBER:
            printf("LoadMember(type:%d, offset:%s, def:%d): %s = [%s + %s]\n",
                   temp->data_type, temp->args2, temp->def_id, temp->result, temp->args1, temp->args2);
            break;
        case IR_LOAD_THIS:
            printf("LoadThis(def:%d): %s\n", temp->def_id, temp->result);
            break;

        case IR_STORE_MEMBER:
            printf("StoreMember(offset:%s): [%s + %s] = %s\n",
                   temp->args2, temp->result, temp->args1, temp->args2);
            break;
        case IR_PHI:
            printf("PHI(def:%d): %s = φ(", temp->def_id, temp->result);
            for (int i = 0; i < temp->phi_arg_count; i++) {
                if (i > 0) printf(", ");
                printf("%s:%s",
                       temp->label[i] ? temp->label[i] : "?",
                       temp->phi_arg_names[i] ? temp->phi_arg_names[i] : "?");
            }
            printf(")\n");
            break;
        default:
            fprintf(stderr,"Unknown IR type %d\n",temp->type);
            break;
        }
        temp = temp->next;
    }
}

void free_IR_chain(IR* ir)
{
    IR* current = ir;
    int a = 0;
    while (current != NULL) {
        IR* next = current->next;

        if (current->result)  { free(current->result);  current->result  = NULL; }
        if (current->args1)   { free(current->args1);   current->args1   = NULL; }
        if (current->args2)   { free(current->args2);   current->args2   = NULL; }

        if (current->label) {
            for (int i = 0; i < current->label_count; i++) {
                if (current->label[i])
                    free(current->label[i]);
            }
            free(current->label);
            current->label = NULL;
        }

        if (current->phi_arg_names) {
            for (int i = 0; i < current->phi_arg_count; i++) {
                if (current->phi_arg_names[i])
                    free(current->phi_arg_names[i]);
            }
            free(current->phi_arg_names);
            current->phi_arg_names = NULL;
        }

        if (current->call_func_args_type) {
            free(current->call_func_args_type);
            current->call_func_args_type = NULL;
        }

        if (current->dim_sizes) {
            free(current->dim_sizes);
            current->dim_sizes = NULL;
        }

        free(current);
        current = next;
    }
}