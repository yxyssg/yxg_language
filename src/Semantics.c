#include "Semantics.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Error.h"

static const char* semantic_filename = NULL;
static const Token ZERO_TOKEN = {.line = 0, .column = 0};
#define SEMLOC(tok) LOC(semantic_filename, (tok).line, (tok).column)

const char* current_semantic_file = NULL;

static DataType get_expression_type(ASTNode* node);
static void analyze_node(ASTNode* node);
static void resolve_class_members(AST_ClassDef* cd, ClassEntry* cls);
static void analyze_class_bodies(AST_ClassDef* cd, ClassEntry* cls);
static void flatten_init_list_sem(ASTNode* node, ASTNode*** out, int* count, int* capacity);

static FuncEntry* func_table = NULL;
bool main_has_args = false;
static bool in_param_list = false;
static ClassEntry* class_table = NULL;
static ClassEntry* current_class = NULL;
static int lab_count = 0;
static char* current_class_name = NULL;

static TypeAlias* type_aliases = NULL;

static AST_Function* current_analyzing_function = NULL;

static void add_type_alias(const char* name, DataType actual_type) {
    TypeAlias* alias = malloc(sizeof(TypeAlias));
    alias->name = strdup(name);
    alias->actual_type = actual_type;
    alias->next = type_aliases;
    type_aliases = alias;
}

static DataType resolve_type_alias(const char* name) {
    for (TypeAlias* a = type_aliases; a; a = a->next) {
        if (strcmp(a->name, name) == 0)
            return a->actual_type;
    }
    return DATA_ERROR;
}

static char* make_getter_name(const char* field_name) {
    int len = strlen(field_name) + 4;
    char* name = malloc(len + 1);
    if (name) {
        name[0] = 'g'; name[1] = 'e'; name[2] = 't';
        name[3] = toupper((unsigned char)field_name[0]);
        strcpy(name + 4, field_name + 1);
    }
    return name;
}

static char* make_setter_name(const char* field_name) {
    int len = strlen(field_name) + 4;
    char* name = malloc(len + 1);
    if (name) {
        name[0] = 's'; name[1] = 'e'; name[2] = 't';
        name[3] = toupper((unsigned char)field_name[0]);
        strcpy(name + 4, field_name + 1);
    }
    return name;
}

static bool is_subclass_of(ClassEntry* class_entry, ClassEntry* parent_entry) {
    if (!class_entry || !parent_entry) return false;
    if (class_entry == parent_entry) return true;

    // —≠ª∑ºÃ≥–ºÏ≤‚
    static int recursion_depth = 0;
    if (recursion_depth > 100) return false; // ∑¿÷πŒﬁœﬁµ›πÈ
    recursion_depth++;

    for (int i = 0; i < class_entry->parent_count; i++) {
        ClassEntry* p = find_class(class_entry->parent_names[i]);
        if (p && is_subclass_of(p, parent_entry)) {
            recursion_depth--;
            return true;
        }
    }

    recursion_depth--;
    return false;
}

static AST_Function* create_getter_ast(const char* class_name,
                                        const char* field_name,
                                        DataType field_type,
                                        int field_offset) {
    AST_Function* func = calloc(1, sizeof(AST_Function));
    func->base.type = AST_FUNCTION;
    func->base.next = NULL;
    func->name.value = strdup(make_getter_name(field_name));
    func->name.type = TOKEN_IDENTIFIER;
    func->Modifier = 0;
    func->max_parameters = 8;
    func->parameterType = malloc(sizeof(DataType) * 8);
    func->parameterCount = 0;
    func->Parameter = NULL;
    func->return_Count = 1;

    int len = strlen(class_name) + 1 + strlen(func->name.value) + 1;
    char* internal = malloc(len);
    snprintf(internal, len, "%s_%s", class_name, func->name.value);
    func->internal_name = internal;

    AST_Return* ret = calloc(1, sizeof(AST_Return));
    ret->base.type = AST_RETURN;
    ret->returnType = field_type;

    AST_MemberAccess* ma = calloc(1, sizeof(AST_MemberAccess));
    ma->base.type = AST_MEMBER_ACCESS;
    ma->member_name = strdup(field_name);
    ma->member_type = field_type;
    ma->member_offset = field_offset;
    ma->is_method = false;

    AST_This* this_node = calloc(1, sizeof(AST_This));
    this_node->base.type = AST_THIS;
    ma->object = (ASTNode*)this_node;

    ret->value = (ASTNode*)ma;
    func->Return = ret;
    func->Code = NULL;

    return func;
}

static AST_Function* create_setter_ast(const char* class_name,
                                        const char* field_name,
                                        DataType field_type,
                                        int field_offset) {
    AST_Function* func = calloc(1, sizeof(AST_Function));
    func->base.type = AST_FUNCTION;
    func->base.next = NULL;
    func->name.value = strdup(make_setter_name(field_name));
    func->name.type = TOKEN_IDENTIFIER;
    func->Modifier = 0;
    func->max_parameters = 8;
    func->parameterType = malloc(sizeof(DataType) * 8);
    func->parameterCount = 0;
    func->Parameter = NULL;
    func->return_Count = 1;

    int len = strlen(class_name) + 1 + strlen(func->name.value) + 1;
    char* internal = malloc(len);
    snprintf(internal, len, "%s_%s", class_name, func->name.value);
    func->internal_name = internal;

    AST_Return* ret = calloc(1, sizeof(AST_Return));
    ret->base.type = AST_RETURN;
    ret->returnType = DATA_VOID;
    ret->value = NULL;
    func->Return = ret;

    AST_Var* param = calloc(1, sizeof(AST_Var));
    param->base.type = AST_VAR;
    param->base.next = NULL;
    param->name.value = strdup("value");
    param->name.type = TOKEN_IDENTIFIER;
    param->type = (field_type == DATA_INT) ? TOKEN_INT :
                  (field_type == DATA_FLOAT) ? TOKEN_FLOAT :
                  (field_type == DATA_STRING) ? TOKEN_STRING :
                  (field_type == DATA_BOOL) ? TOKEN_BOOL :
                  (field_type == DATA_CHAR) ? TOKEN_CHAR : TOKEN_INT;
    param->dataType = field_type;
    param->internal_name = NULL;
    param->modifier = -1;
    func->Parameter = (ASTNode*)param;
    func->parameterCount = 1;
    func->parameterType[0] = field_type;

    AST_MemberAss* mass = calloc(1, sizeof(AST_MemberAss));
    mass->base.type = AST_MEMBER_ASS;
    mass->member_name = strdup(field_name);
    mass->member_type = field_type;
    mass->member_offset = field_offset;

    AST_This* this_node = calloc(1, sizeof(AST_This));
    this_node->base.type = AST_THIS;
    mass->object = (ASTNode*)this_node;

    AST_Literal* val_lit = calloc(1, sizeof(AST_Literal));
    val_lit->base.type = AST_LITERAL;
    val_lit->base.value.type = TOKEN_IDENTIFIER;
    val_lit->base.value.value = strdup("value");
    val_lit->dataType = field_type;
    mass->value = (ASTNode*)val_lit;

    func->Code = (ASTNode*)mass;

    return func;
}

static bool method_signature_match(AST_Function* a, AST_Function* b) {
    if (!a || !b) return false;
    if (strcmp(a->name.value, b->name.value) != 0) return false;
    int a_params = a->parameterCount;
    int b_params = b->parameterCount;
    if (a_params != b_params) return false;
    for (int i = 0; i < a_params; i++) {
        if (a->parameterType[i] != b->parameterType[i]) return false;
    }
    return true;
}

static int inherit_from_parent(ClassEntry* child, ClassEntry* parent, int offset)
{
    if (!parent) return offset;
    int current_offset = offset;
    for (int i = 0; i < parent->member_count; i++)
    {
        MemberInfo* pmi = parent->members[i];
        if (pmi->is_method) continue;
        bool conflict = false;
        for (int j = 0; j < child->member_count; j++) {
            if (!child->members[j]->is_method &&
                strcmp(child->members[j]->name, pmi->name) == 0) {
                ERROR(ERR_SEM_DUPLICATE_VAR,
                      SEMLOC(ZERO_TOKEN),
                      "field '%s' conflicts with inherited field",
                      pmi->name);
                conflict = true;
                break;
                }
        }
        if (conflict) continue;
        if (child->member_count >= child->member_capacity) {
            child->member_capacity = child->member_capacity ? child->member_capacity * 2 : 8;
            child->members = realloc(child->members,
                                     sizeof(MemberInfo*) * child->member_capacity);
        }
        MemberInfo* mi = calloc(1, sizeof(MemberInfo));
        mi->name = strdup(pmi->name);
        mi->type = pmi->type;
        mi->is_method = false;
        mi->offset = current_offset;
        mi->inherited_from = parent;
        mi->field_modifier = pmi->field_modifier;
        child->members[child->member_count++] = mi;
        current_offset += S_get_type_size(pmi->type);
    }
    return current_offset;
}

static bool types_compatible(DataType declared, DataType actual) {
    if (declared == actual) return true;

    if (declared == DATA_IDENTIFIER && (actual == DATA_INT || actual == DATA_UINT))
        return true;
    if (actual == DATA_IDENTIFIER && (declared == DATA_INT || declared == DATA_UINT))
        return true;

    if (declared == DATA_IDENTIFIER || actual == DATA_IDENTIFIER) return true;

    bool decl_is_int_family = (declared == DATA_INT  || declared == DATA_UINT ||
                               declared == DATA_CHAR || declared == DATA_BOOL);
    bool act_is_int_family  = (actual  == DATA_INT  || actual  == DATA_UINT ||
                               actual  == DATA_CHAR || actual  == DATA_BOOL);
    if (decl_is_int_family && act_is_int_family) return true;

    bool decl_is_float_family = (declared == DATA_FLOAT || declared == DATA_UFLOAT);
    bool act_is_float_family  = (actual  == DATA_FLOAT || actual  == DATA_UFLOAT);
    if (decl_is_float_family && act_is_float_family) return true;

    if (act_is_int_family && decl_is_float_family) return true;
    if (decl_is_int_family && act_is_float_family) return true;

    if ((declared == DATA_STRING && actual == DATA_CHAR) ||
        (declared == DATA_CHAR && actual == DATA_STRING)) return true;
    return false;
}

static bool all_types_compatible(DataType* declared, DataType* actual, int count) {
    for (int i = 0; i < count; i++) {
        if (!types_compatible(declared[i], actual[i])) return false;
    }
    return true;
}

static bool function_signature_identical(AST_Function* a, AST_Function* b) {
    if (strcmp(a->name.value, b->name.value) != 0) return false;
    if (a->parameterCount != b->parameterCount) return false;
    for (int i = 0; i < a->parameterCount; i++) {
        if (a->parameterType[i] != b->parameterType[i]) return false;
    }
    DataType retA = a->Return ? a->Return->returnType : DATA_VOID;
    DataType retB = b->Return ? b->Return->returnType : DATA_VOID;
    if (retA != retB) return false;
    return true;
}

static bool function_signature_conflict(AST_Function* a, AST_Function* b) {
    if (strcmp(a->name.value, b->name.value) != 0) return false;
    if (a->parameterCount != b->parameterCount) return false;
    for (int i = 0; i < a->parameterCount; i++) {
        if (a->parameterType[i] != b->parameterType[i]) return false;
    }
    DataType retA = a->Return ? a->Return->returnType : DATA_VOID;
    DataType retB = b->Return ? b->Return->returnType : DATA_VOID;
    return retA != retB;
}

static void add_function_with_params(const char* name, AST_Function* func) {
    FuncEntry* entry = malloc(sizeof(FuncEntry));
    entry->name = strdup(name);
    entry->func = func;
    if (func->parameterCount > 0 && func->parameterType != NULL) {
        entry->param_count = func->parameterCount;
        entry->param_types = malloc(sizeof(DataType) * func->parameterCount);
        memcpy(entry->param_types, func->parameterType,
               sizeof(DataType) * func->parameterCount);
    } else {
        entry->param_count = 0;
        entry->param_types = NULL;
    }
    entry->next = func_table;
    func_table = entry;
}

AST_Function* find_function_by_signature(const char* name, int arg_count, DataType* arg_types) {
    AST_Function* best_match = NULL;
    int best_score = -1;
    for (FuncEntry* e = func_table; e; e = e->next) {
        if (strcmp(e->name, name) != 0) continue;

        if (e->param_types == NULL) {
            if (e->param_count == 0 && arg_count == 0) {
                best_match = e->func;
                break;
            }
            continue;
        }

        bool func_has_this = (e->param_count > 0 && e->param_types[0] == DATA_IDENTIFIER);
        bool call_has_this = (arg_count > 0 && arg_types != NULL && arg_types[0] == DATA_IDENTIFIER);
        int compare_offset = 0;
        if (func_has_this && call_has_this) {
            if (e->param_count != arg_count) continue;
            compare_offset = 0;
        } else if (func_has_this && !call_has_this) {
            if (e->param_count - 1 != arg_count) continue;
            compare_offset = 1;
        } else if (!func_has_this && call_has_this) {
            continue;
        } else {
            if (e->param_count != arg_count) continue;
            compare_offset = 0;
        }
        const DataType* func_params = e->param_types + compare_offset;
        bool match = true;
        int score = 0;
        for (int i = 0; i < arg_count; i++) {
            if (func_params[i] == arg_types[i]) {
                score += 2;
            } else if (types_compatible(func_params[i], arg_types[i])) {
                score += 1;
            } else {
                match = false;
                break;
            }
        }
        if (match && score > best_score) {
            best_score = score;
            best_match = e->func;
        }
    }
    return best_match;
}

bool function_signature_exists(const char* name, int param_count, DataType* param_types) {
    for (FuncEntry* e = func_table; e; e = e->next) {
        if (strcmp(e->name, name) != 0) continue;
        if (e->param_count != param_count) continue;
        bool match = true;
        for (int i = 0; i < param_count; i++) {
            if (e->param_types[i] != param_types[i]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

static char* generate_method_mangled_name(const char* class_name, AST_Function* func) {
    int len = strlen(class_name) + 1 + strlen(func->name.value);
    for (int i = 0; i < func->parameterCount; i++) {
        if (i == 0 && func->parameterType[0] == DATA_IDENTIFIER) continue;
        len += 1;
        const char* tname = data_type_to_string(func->parameterType[i]);
        len += strlen(tname);
    }
    char* mangled = malloc(len + 1);
    if (!mangled) return NULL;
    sprintf(mangled, "%s_%s", class_name, func->name.value);
    char* ptr = mangled + strlen(mangled);
    for (int i = 0; i < func->parameterCount; i++) {
        if (i == 0 && func->parameterType[0] == DATA_IDENTIFIER) continue;
        *ptr++ = '_';
        const char* tname = data_type_to_string(func->parameterType[i]);
        strcpy(ptr, tname);
        ptr += strlen(tname);
    }
    *ptr = '\0';
    return mangled;
}

ClassEntry* find_class(const char* name) {
    for (ClassEntry* e = class_table; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

void add_class(ClassEntry* entry) {
    entry->next = class_table;
    class_table = entry;
}

static void add_function(const char* name, AST_Function* func) {
    add_function_with_params(name, func);
}

static AST_Function* find_function(const char* name) {
    for (FuncEntry* e = func_table; e; e = e->next) {
        if (strcmp(e->name, name) == 0)
            return e->func;
    }
    return NULL;
}

static void free_function_table(void) {
    FuncEntry* e = func_table;
    while (e) {
        FuncEntry* next = e->next;
        if (e->param_types) free(e->param_types);
        free(e);
        e = next;
    }
    func_table = NULL;
}

Scope* current_scope = NULL;
static int internal_name_counter = 0;

char* generate_internal_name(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "_var%d", internal_name_counter++);
    return strdup(buf);
}

unsigned int hash_function(const char* str, int table_size) {
    if (str == NULL) {
        return 0;
    }
    const unsigned int seed = 131;
    unsigned int hash_value = 0;
    while (*str != '\0') {
        hash_value = hash_value * seed + (unsigned char)*str;
        str++;
    }
    return hash_value % table_size;
}

Symbol* create_symbol(Token value, DataType type, char* key, char* internal_name) {
    Symbol* symbol = calloc(1, sizeof(Symbol));
    if (symbol == NULL) {
        OUT_OF_MEMORY();
    }
    symbol->value = value;
    symbol->key = key ? strdup(key) : NULL;
    symbol->type = type;
    symbol->internal_name = internal_name ? strdup(internal_name) : NULL;
    symbol->next = NULL;
    symbol->dim_count = 0;
    symbol->dim_sizes = NULL;
    return symbol;
}

void free_symbol(Symbol* sym) {
    if (sym) {
        if (sym->key) free(sym->key);
        if (sym->internal_name) free(sym->internal_name);
        if (sym->dim_sizes) free(sym->dim_sizes);
        free(sym);
    }
}

void resize_hash_table(Symbol_Table* table) {
    if (table->count < table->size) return;

    int new_size = table->size * 2;
    Symbol** new_buckets = calloc(new_size, sizeof(Symbol*));
    if (new_buckets == NULL) {
        OUT_OF_MEMORY();
    }

    for (int i = 0; i < table->size; i++) {
        Symbol* current = table->buckets[i];
        while (current != NULL) {
            Symbol* next = current->next;
            int new_index = hash_function(current->key, new_size);
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;
            current = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->size = new_size;
}

void enter_scope(void) {
    Scope* new_scope = malloc(sizeof(Scope));
    if (new_scope == NULL) {
        OUT_OF_MEMORY();
    }

    new_scope->table = malloc(sizeof(Symbol_Table));
    if (new_scope->table == NULL) {
        OUT_OF_MEMORY();
    }

    new_scope->table->size = 16;
    new_scope->table->count = 0;
    new_scope->table->buckets = calloc(new_scope->table->size, sizeof(Symbol*));
    if (new_scope->table->buckets == NULL) {
        OUT_OF_MEMORY();
    }

    new_scope->parent = current_scope;
    new_scope->depth = current_scope ? current_scope->depth + 1 : 0;
    current_scope = new_scope;
}

void exit_scope(void) {
    if (!current_scope) return;

    for (int i = 0; i < current_scope->table->size; i++) {
        Symbol* sym = current_scope->table->buckets[i];
        while (sym) {
            Symbol* next = sym->next;
            free_symbol(sym);
            sym = next;
        }
    }

    free(current_scope->table->buckets);
    free(current_scope->table);

    Scope* parent = current_scope->parent;
    free(current_scope);
    current_scope = parent;
}

Symbol* lookup_symbol(const char* name) {
    if (!name) return NULL;

    for (Scope* scope = current_scope; scope != NULL; scope = scope->parent) {
        int index = hash_function(name, scope->table->size);
        Symbol* sym = scope->table->buckets[index];

        while (sym) {
            if (sym->key && strcmp(sym->key, name) == 0) {
                return sym;
            }
            sym = sym->next;
        }
    }
    return NULL;
}

void insert_symbol(Symbol* sym) {
    if (!current_scope || !sym || !sym->key) return;

    int index = hash_function(sym->key, current_scope->table->size);

    Symbol* cur = current_scope->table->buckets[index];
    while (cur) {
        if (cur->key && strcmp(cur->key, sym->key) == 0) {
            if (cur->is_function && sym->is_function) {
                break;
            }
            ERROR(ERR_SEM_DUPLICATE_VAR,
                  SEMLOC(sym->value),
                  "duplicate variable '%s'",
                  sym->key);
            free_symbol(sym);
            return;
        }
        cur = cur->next;
    }

    sym->next = current_scope->table->buckets[index];
    current_scope->table->buckets[index] = sym;
    current_scope->table->count++;

    resize_hash_table(current_scope->table);
}

bool has_main_function(void) {
    if (!current_scope) return false;

    Scope* global_scope = current_scope;
    while (global_scope->parent != NULL) {
        global_scope = global_scope->parent;
    }

    int index = hash_function("main", global_scope->table->size);
    Symbol* sym = global_scope->table->buckets[index];

    while (sym) {
        if (sym->key && strcmp(sym->key, "main") == 0) {
            return true;
        }
        sym = sym->next;
    }

    return false;
}

static void check_unsigned_assignment(DataType target_type, ASTNode* value,
                                      int line, int column) {
    if (target_type != DATA_UINT && target_type != DATA_UFLOAT) {
        return;
    }
    if (value == NULL) return;
    DataType expr_type = get_expression_type(value);
    if (value->type == AST_UNARY) {
        AST_Unary* unary = (AST_Unary*)value;
        if (unary->op.type == TOKEN_SUB) {
            ERROR(ERR_SEM_CANNOT_ASSIGN_NEG,
                  LOC(semantic_filename, line, column),
                  "cannot assign negative value to unsigned type '%s'",
                  data_type_to_string(target_type));
            return;
        }
    }

    if (value->type == AST_LITERAL) {
        AST_Literal* lit = (AST_Literal*)value;
        if (lit->base.value.type == TOKEN_NUMBER) {
            int val = atoi(lit->base.value.value);
            if (val < 0) {
                ERROR(ERR_SEM_CANNOT_ASSIGN_NEG,
                      LOC(semantic_filename, line, column),
                      "cannot assign negative constant %d to unsigned type '%s'",
                      val, data_type_to_string(target_type));
                return;
            }
        }
        if (lit->base.value.type == TOKEN_FLOAT_LITERAL) {
            double val = atof(lit->base.value.value);
            if (val < 0.0) {
                ERROR(ERR_SEM_CANNOT_ASSIGN_NEG,
                      LOC(semantic_filename, line, column),
                      "cannot assign negative constant %g to unsigned type '%s'",
                      val, data_type_to_string(target_type));
                return;
            }
        }
    }

    if (expr_type == DATA_INT || expr_type == DATA_FLOAT) {
        WARNING(ERR_SEM_CANNOT_ASSIGN_NEG,
                LOC(semantic_filename, line, column),
                "assigning signed '%s' to unsigned '%s' may lose sign",
                data_type_to_string(expr_type),
                data_type_to_string(target_type));
    }
}

void count_flat(ASTNode* node, int* cnt) {
    if (node->type == AST_INIT_LIST) {
        AST_InitList* l = (AST_InitList*)node;
        for (int i = 0; i < l->count; i++) count_flat(l->elements[i], cnt);
    } else (*cnt)++;
}

static void flatten_init_list_sem(ASTNode* node, ASTNode*** out, int* count, int* capacity) {
    if (node->type == AST_INIT_LIST) {
        AST_InitList* list = (AST_InitList*)node;
        for (int i = 0; i < list->count; i++) {
            flatten_init_list_sem(list->elements[i], out, count, capacity);
        }
    } else {
        if (*count >= *capacity) {
            if (*capacity == 0) *capacity = 16;
            else *capacity *= 2;
            *out = realloc(*out, sizeof(ASTNode*) * (*capacity));
        }
        (*out)[(*count)++] = node;
    }
}

char* data_type_to_string(DataType type) {
    switch (type) {
        case DATA_INT: return "int";
        case DATA_FLOAT: return "float";
        case DATA_UINT: return "uint";
        case DATA_UFLOAT: return "ufloat";
        case DATA_STRING: return "string";
        case DATA_BOOL: return "bool";
        case DATA_CHAR: return "char";
        case DATA_IDENTIFIER: return "identifier";
        case DATA_VOID: return "void";
        default: return "unknown";
    }
}

DataType Get_Datatype(TokenType type) {
    switch (type) {
        case TOKEN_NUMBER:
        case TOKEN_INT:
            return DATA_INT;
        case TOKEN_UINT:
            return DATA_UINT;
        case TOKEN_UFLOAT:
            return DATA_UFLOAT;
        case TOKEN_STRING_LITERAL:
        case TOKEN_STRING:
            return DATA_STRING;
        case TOKEN_CHAR_LITERAL:
        case TOKEN_CHAR:
            return DATA_CHAR;
        case TOKEN_BOOL_LITERAL:
        case TOKEN_BOOL:
            return DATA_BOOL;
        case TOKEN_FLOAT_LITERAL:
        case TOKEN_FLOAT:
            return DATA_FLOAT;
        case TOKEN_VOID:
            return DATA_VOID;
        case TOKEN_IDENTIFIER:
            return DATA_IDENTIFIER;
        default:
            return DATA_VOID;
    }
}

static DataType get_expression_type(ASTNode* node) {
    if (!node) return DATA_ERROR;

    switch (node->type) {
    case AST_LITERAL: {
        AST_Literal* lit = (AST_Literal*)node;
        return lit->dataType;
    }
    case AST_BINARY: {
        AST_Binary* bin = (AST_Binary*)node;
        return bin->resultType;
    }
    case AST_UNARY: {
        AST_Unary* unary = (AST_Unary*)node;
        DataType operand_type = get_expression_type(unary->right);
        if (unary->op.type == TOKEN_OPPOSE) {
            return DATA_BOOL;
        }
        return operand_type;
    }
    case AST_CONDITIONAL: {
            AST_Conditional* tern = (AST_Conditional*)node;
            return tern->resultType;
    }
    case AST_VAR:
    case AST_VAR_ASS: {
        AST_Var* var = (AST_Var*)node;
        return var->dataType;
    }
    case AST_ARRAY_ACCESS: {
        AST_ArrayAccess* acc = (AST_ArrayAccess*)node;
        return acc->elementType;
    }
    case AST_ARRAY_LENGTH: {
        return DATA_INT;
    }
    case AST_FUNCTION: {
        AST_Function* func = (AST_Function*)node;
        if (func->Return != NULL) {
            return func->Return->returnType;
        }
        return DATA_VOID;
    }
    case AST_MEMBER_ACCESS: {
        AST_MemberAccess* ma = (AST_MemberAccess*)node;
        return ma->member_type;
    }
    case AST_THIS:
    case AST_NEW_INSTANCE:
        return DATA_INT;
    case AST_MEMBER_ASS: {
        AST_MemberAss* mass = (AST_MemberAss*)node;
        return mass->member_type;
    }
    case AST_RETURN: {
        AST_Return* ret = (AST_Return*)node;
        return ret->returnType;
    }
    default:
        return DATA_ERROR;
    }
}

DataType L_A_R_get_Datatype(DataType left_type, DataType right_type, int line, int column) {
    if (left_type == right_type) {
        return left_type;
    }

    if (left_type == DATA_UINT || right_type == DATA_UINT) {
        if (left_type == DATA_INT || right_type == DATA_INT ||
            left_type == DATA_UINT || right_type == DATA_UINT) {
            return DATA_UINT;
        }
        if (left_type == DATA_CHAR || right_type == DATA_CHAR) {
            return DATA_UINT;
        }
        if (left_type == DATA_FLOAT || right_type == DATA_FLOAT) {
            return DATA_UFLOAT;
        }
        if (left_type == DATA_UFLOAT || right_type == DATA_UFLOAT) {
            return DATA_UFLOAT;
        }
    }

    if (left_type == DATA_UFLOAT || right_type == DATA_UFLOAT) {
        if (left_type == DATA_FLOAT || right_type == DATA_FLOAT ||
            left_type == DATA_INT || right_type == DATA_INT ||
            left_type == DATA_UINT || right_type == DATA_UINT ||
            left_type == DATA_CHAR || right_type == DATA_CHAR) {
            return DATA_UFLOAT;
        }
    }

    if (left_type == DATA_INT) {
        if (right_type == DATA_FLOAT) return DATA_FLOAT;
        if (right_type == DATA_CHAR) return DATA_INT;
    }
    if (left_type == DATA_FLOAT) {
        if (right_type == DATA_INT) return DATA_FLOAT;
    }
    if (left_type == DATA_STRING) {
        if (right_type == DATA_CHAR) return DATA_STRING;
    }
    if (left_type == DATA_CHAR) {
        if (right_type == DATA_INT) return DATA_INT;
        if (right_type == DATA_STRING) return DATA_STRING;
        if (right_type == DATA_CHAR) return DATA_STRING;
    }

    ERROR(ERR_SEM_INCOMPATIBLE_TYPES,
          LOC(semantic_filename, line, column),
          "incompatible types: '%s' and '%s'",
          data_type_to_string(left_type), data_type_to_string(right_type));
    return DATA_ERROR;
}

bool Operation_Check(AST_Binary* binary) {
    DataType left_type = get_expression_type(binary->left);
    DataType right_type = get_expression_type(binary->right);
    binary->is_unsigned = (left_type == DATA_UINT || right_type == DATA_UINT ||
                           left_type == DATA_UFLOAT || right_type == DATA_UFLOAT);

    if (!types_compatible(left_type, right_type) &&
        !types_compatible(right_type, left_type)) {
        ERROR(ERR_SEM_INCOMPATIBLE_TYPES,
              LOC(semantic_filename, binary->base.value.line, binary->base.value.column),
              "incompatible types: '%s' and '%s'",
              data_type_to_string(left_type), data_type_to_string(right_type));
        return false;
        }
    if (binary->op.type == TOKEN_SHIFT_LEFT ||
        binary->op.type == TOKEN_SHIFT_RIGHT) {
        DataType lt = get_expression_type(binary->left);
        DataType rt = get_expression_type(binary->right);
        if (lt != DATA_INT && lt != DATA_UINT && lt != DATA_CHAR && lt != DATA_BOOL) {
            ERROR(ERR_SEM_TYPE_MISMATCH,
                  LOC(semantic_filename, binary->base.value.line, binary->base.value.column),
                  "shift operator requires integer left operand, got '%s'",
                  data_type_to_string(lt));
            return false;
        }

        if (binary->op.type == TOKEN_BITWISE_AND ||
            binary->op.type == TOKEN_BITWISE_OR  ||
            binary->op.type == TOKEN_BITWISE_XOR)
        {
            DataType lt = get_expression_type(binary->left);
            DataType rt = get_expression_type(binary->right);
            bool left_ok  = (lt == DATA_INT  || lt == DATA_UINT ||
                             lt == DATA_CHAR || lt == DATA_BOOL);
            bool right_ok = (rt == DATA_INT  || rt == DATA_UINT ||
                             rt == DATA_CHAR || rt == DATA_BOOL);
            if (!left_ok || !right_ok) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      LOC(semantic_filename, binary->base.value.line,
                          binary->base.value.column),
                      "bitwise operator requires integer operands, got '%s' and '%s'",
                      data_type_to_string(lt), data_type_to_string(rt));
                return false;
            }
            binary->is_unsigned = (lt == DATA_UINT || rt == DATA_UINT);
            return true;
        }

        if (rt != DATA_INT && rt != DATA_UINT && rt != DATA_CHAR) {
            ERROR(ERR_SEM_TYPE_MISMATCH,
                  LOC(semantic_filename, binary->base.value.line, binary->base.value.column),
                  "shift amount must be integer, got '%s'",
                  data_type_to_string(rt));
            return false;
        }
        binary->is_unsigned = (lt == DATA_UINT);
        binary->resultType  = lt;
        return true;
    }
    return true;
}

DataType Binary_get_Datatype(AST_Binary* binary) {
    if (binary->op.type == TOKEN_EQ || binary->op.type == TOKEN_NE ||
        binary->op.type == TOKEN_LT || binary->op.type == TOKEN_GT ||
        binary->op.type == TOKEN_LE || binary->op.type == TOKEN_GE) {
        return DATA_BOOL;
    }

    if (binary->op.type == TOKEN_LOGICAL_AND || binary->op.type == TOKEN_LOGICAL_OR)
        return DATA_BOOL;

    if (binary->op.type == TOKEN_BITWISE_AND||
        binary->op.type == TOKEN_BITWISE_OR ||
        binary->op.type == TOKEN_BITWISE_XOR) {
        DataType left_type = get_expression_type(binary->left);
        DataType right_type = get_expression_type(binary->right);
        return L_A_R_get_Datatype(left_type, right_type,
                                  binary->base.value.line, binary->base.value.column);
    }

    if (binary->op.type == TOKEN_SHIFT_LEFT ||
        binary->op.type == TOKEN_SHIFT_RIGHT) {
        return binary->resultType;
    }

    DataType left_type = get_expression_type(binary->left);
    DataType right_type = get_expression_type(binary->right);

    return L_A_R_get_Datatype(left_type, right_type,
        binary->base.value.line, binary->base.value.column);
}

bool check_division_by_zero(AST_Binary* binary) {
    if (binary->op.type != TOKEN_DIV) return true;

    if (binary->right->type == AST_LITERAL) {
        AST_Literal* lit = (AST_Literal*)binary->right;
        if (lit->base.value.type == TOKEN_NUMBER &&
            strcmp(lit->base.value.value, "0") == 0) {
            ERROR(ERR_SEM_DIV_BY_ZERO,
                  LOC(semantic_filename, binary->base.value.line, binary->base.value.column),
                  "division by zero");
            return false;
        }
        else if (lit->base.value.type == TOKEN_FLOAT_LITERAL &&
                 atof(lit->base.value.value) == 0.0) {
            ERROR(ERR_SEM_DIV_BY_ZERO,
                  LOC(semantic_filename, binary->base.value.line, binary->base.value.column),
                  "division by zero");
            return false;
        }
    }
    return true;
}

int S_get_type_size(DataType type) {
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

static void predeclare_all_types(ASTNode* node) {
    while (node) {
        switch (node->type) {
        case AST_FUNCTION: {
            AST_Function* func = (AST_Function*)node;
            add_function_with_params(func->name.value, func);
            break;
        }
        case AST_CLASS_DEF: {
            AST_ClassDef* cd = (AST_ClassDef*)node;
            if (find_class(cd->name) != NULL) break;
            ClassEntry* cls = calloc(1, sizeof(ClassEntry));
            cls->name = strdup(cd->name);
            cls->parent_count = cd->parent_count;
            cls->parent_names = malloc(sizeof(char*) * cd->parent_count);
            for (int i = 0; i < cd->parent_count; i++)
                cls->parent_names[i] = strdup(cd->parent_names[i]);
            cls->modifier = cd->modifier;
            add_class(cls);
            ASTNode* m = cd->members;
            while (m) {
                if (m->type == AST_FUNCTION) {
                    add_function_with_params(((AST_Function*)m)->name.value,
                                            (AST_Function*)m);
                }
                m = m->next;
            }
            break;
        }
        default:
            break;
        }
        node = node->next;
    }

    {
        AST_Function* fmt_func = calloc(1, sizeof(AST_Function));
        fmt_func->base.type = AST_FUNCTION;
        fmt_func->name.value = strdup("format");
        fmt_func->name.type = TOKEN_IDENTIFIER;
        fmt_func->internal_name = strdup("_format_string");
        fmt_func->parameterCount = 1;
        fmt_func->parameterType = malloc(sizeof(DataType));
        fmt_func->parameterType[0] = DATA_STRING;
        fmt_func->return_Count = 1;
        fmt_func->Return = calloc(1, sizeof(AST_Return));
        fmt_func->Return->returnType = DATA_STRING;
        add_function_with_params("format", fmt_func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_str_length", .type = TOKEN_IDENTIFIER},
            DATA_INT, "_str_length", "_str_length");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_str_length");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_str_length");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_STRING;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_INT;
        add_function_with_params("_str_length", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_str_char_at", .type = TOKEN_IDENTIFIER},
            DATA_INT, "_str_char_at", "_str_char_at");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_str_char_at");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_str_char_at");
        func->parameterCount = 2;
        func->parameterType = malloc(sizeof(DataType) * 2);
        func->parameterType[0] = DATA_STRING;
        func->parameterType[1] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_INT;
        add_function_with_params("_str_char_at", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_str_substring", .type = TOKEN_IDENTIFIER},
            DATA_STRING, "_str_substring", "_str_substring");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_str_substring");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_str_substring");
        func->parameterCount = 3;
        func->parameterType = malloc(sizeof(DataType) * 3);
        func->parameterType[0] = DATA_STRING;
        func->parameterType[1] = DATA_INT;
        func->parameterType[2] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_str_substring", func);
    }

    {
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_char_to_str");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_char_to_str");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_char_to_str", func);
    }

    {
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_str_plus_char");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_str_plus_char_2");
        func->parameterCount = 2;
        func->parameterType = malloc(sizeof(DataType) * 2);
        func->parameterType[0] = DATA_STRING;
        func->parameterType[1] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_str_plus_char", func);
    }

    {
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_char_plus_str");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_char_plus_str_2");
        func->parameterCount = 2;
        func->parameterType = malloc(sizeof(DataType) * 2);
        func->parameterType[0] = DATA_STRING;
        func->parameterType[1] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_char_plus_str", func);
    }

    {
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_str_concat");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_str_concat");
        func->parameterCount = 2;
        func->parameterType = malloc(sizeof(DataType) * 2);
        func->parameterType[0] = DATA_STRING;
        func->parameterType[1] = DATA_STRING;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_str_concat", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_GetCommandLineArgc", .type = TOKEN_IDENTIFIER},
            DATA_INT, "_GetCommandLineArgc", "_GetCommandLineArgc");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_GetCommandLineArgc");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_GetCommandLineArgc");
        func->parameterCount = 0;
        func->parameterType = NULL;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_INT;
        add_function_with_params("_GetCommandLineArgc", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_sys_argv_at", .type = TOKEN_IDENTIFIER},
            DATA_STRING, "_sys_argv_at", "_sys_argv_at");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_sys_argv_at");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_sys_argv_at");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_sys_argv_at", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_system_exit", .type = TOKEN_IDENTIFIER},
            DATA_VOID, "_system_exit", "_system_exit");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_system_exit");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_system_exit");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_VOID;
        add_function_with_params("_system_exit", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_system_sleep", .type = TOKEN_IDENTIFIER},
            DATA_VOID, "_system_sleep", "_system_sleep");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_system_sleep");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_system_sleep");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_VOID;
        add_function_with_params("_system_sleep", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_system_time", .type = TOKEN_IDENTIFIER},
            DATA_INT, "_system_time", "_system_time");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_system_time");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_system_time");
        func->parameterCount = 0;
        func->parameterType = NULL;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_INT;
        add_function_with_params("_system_time", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_system_getenv", .type = TOKEN_IDENTIFIER},
            DATA_STRING, "_system_getenv", "_system_getenv");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_system_getenv");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_system_getenv");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_STRING;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_system_getenv", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_GetCommandLineArgv", .type = TOKEN_IDENTIFIER},
            DATA_STRING, "_GetCommandLineArgv", "_GetCommandLineArgv");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_GetCommandLineArgv");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_GetCommandLineArgv");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_INT;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_STRING;
        add_function_with_params("_GetCommandLineArgv", func);
    }

    {
        Symbol* sym = create_symbol(
            (Token){.value = "_system_call", .type = TOKEN_IDENTIFIER},
            DATA_INT, "_system_call", "_system_call");
        sym->is_function = true;
        insert_symbol(sym);
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_system_call");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_system_call");
        func->parameterCount = 1;
        func->parameterType = malloc(sizeof(DataType));
        func->parameterType[0] = DATA_STRING;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_INT;
        add_function_with_params("_system_call", func);
    }

    {
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("gc");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_gc_force");
        func->parameterCount = 0;
        func->parameterType = NULL;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_VOID;
        add_function_with_params("gc", func);
    }

    {
        AST_Function* func = calloc(1, sizeof(AST_Function));
        func->base.type = AST_FUNCTION;
        func->name.value = strdup("_gc_status");
        func->name.type = TOKEN_IDENTIFIER;
        func->internal_name = strdup("_gc_status");
        func->parameterCount = 0;
        func->parameterType = NULL;
        func->return_Count = 1;
        func->Return = calloc(1, sizeof(AST_Return));
        func->Return->returnType = DATA_VOID;
        add_function_with_params("_gc_status", func);
    }
}

static int get_type_size(DataType type) {
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

static void resolve_class_members(AST_ClassDef* cd, ClassEntry* cls) {
    if (cls->members_resolved) return;

    char* saved_class_name = current_class_name;
    ClassEntry* saved_class = current_class;
    current_class_name = cls->name;
    current_class = cls;

    Symbol* cls_sym = create_symbol(
        (Token){.value = cd->name, .type = TOKEN_IDENTIFIER},
        DATA_IDENTIFIER, cd->name, generate_internal_name());
    insert_symbol(cls_sym);

    {
        ASTNode* m = cd->members;
        while (m) {
            if (m->type == AST_ENUM_DEF) {
                ASTNode* saved_next = m->next;
                m->next = NULL;
                analyze_node(m);
                m->next = saved_next;
            }
            m = m->next;
        }
    }

    enter_scope();

    Symbol* this_sym = create_symbol(
        (Token){.value = "this", .type = TOKEN_IDENTIFIER},
        DATA_IDENTIFIER, "this", strdup("_this"));
    this_sym->class_name = strdup(cd->name);
    insert_symbol(this_sym);

    for (int i = 0; i < cls->member_count; i++) {
        MemberInfo* mi = cls->members[i];
        if (mi->is_method) continue;

        Symbol* field_sym = create_symbol(
            (Token){.value = mi->name, .type = TOKEN_IDENTIFIER},
            mi->type, mi->name, strdup(mi->name));
        field_sym->is_field = true;
        field_sym->field_offset = mi->offset;
        field_sym->field_modifier = mi->field_modifier;
        insert_symbol(field_sym);
    }

    ClassEntry** parent_entries = malloc(sizeof(ClassEntry*) * (size_t)cd->parent_count);
    bool all_parents_valid = true;
    for (int i = 0; i < cd->parent_count; i++) {
        parent_entries[i] = find_class(cd->parent_names[i]);
        if (!parent_entries[i]) {
            Token err_tok = {.line = cd->base.value.line, .column = cd->base.value.column};
            ERROR(ERR_SEM_CLASS_NOT_FOUND,
                  SEMLOC(err_tok),
                  "parent class '%s' not found",
                  cd->parent_names[i]);
            all_parents_valid = false;
        }
    }

    int off = 0;
    if (all_parents_valid) {
        for (int i = 0; i < cd->parent_count; i++) {
            off = inherit_from_parent(cls, parent_entries[i], off);
        }
    }

    int mcount = 0;
    for (ASTNode* n = cd->members; n; n = n->next) mcount++;

    ASTNode** marray = malloc(sizeof(ASTNode*) * (size_t)(mcount + 1));
    if (!marray) OUT_OF_MEMORY();
    {
        int i = 0;
        ASTNode* n = cd->members;
        while (n) {
            ASTNode* saved_next = n->next;
            n->next = NULL;
            marray[i++] = n;
            n = saved_next;
        }
        marray[mcount] = NULL;
    }

    ASTNode* extra_head = NULL;
    ASTNode* extra_tail = NULL;

    #define APPEND_EXTRA(node) do {            \
        (node)->next = NULL;                   \
        if (!extra_head) {                      \
            extra_head = extra_tail = (node);   \
        } else {                                \
            extra_tail->next = (node);          \
            extra_tail = (node);                \
        }                                       \
    } while(0)

    for (int mi = 0; mi < mcount; mi++) {
        ASTNode* m = marray[mi];
        if (!m) continue;

        if (m->type == AST_VAR || m->type == AST_UNINIT_VAR) {
            AST_Var* var = (AST_Var*)m;

            char* saved_class_name_field = NULL;
            if (var->internal_name != NULL) {
                saved_class_name_field = strdup(var->internal_name);
            }

            char* internal = generate_internal_name();
            var->dataType = Get_Datatype(var->type);
            Symbol* sym = create_symbol(var->name, var->dataType,
                                        var->name.value, internal);

            if (saved_class_name_field != NULL) {
                ClassEntry* cls_entry = find_class(saved_class_name_field);
                if (cls_entry) {
                    sym->class_name = strdup(saved_class_name_field);
                }
            }
            insert_symbol(sym);
            var->internal_name = strdup(internal);
            free(internal);

            bool field_conflict = false;
            for (int j = 0; j < cls->member_count; j++) {
                if (!cls->members[j]->is_method &&
                    strcmp(cls->members[j]->name, var->name.value) == 0) {
                    ERROR(ERR_SEM_DUPLICATE_VAR,
                          SEMLOC(var->name),
                          "field '%s' conflicts with inherited field",
                          var->name.value);
                    field_conflict = true;
                    break;
                }
            }
            if (field_conflict) {
                free(saved_class_name_field);
                continue;
            }

            DataType dt = var->dataType;
            int sz = S_get_type_size(dt);

            if (cls->member_count >= cls->member_capacity) {
                cls->member_capacity = cls->member_capacity ? cls->member_capacity * 2 : 8;
                cls->members = realloc(cls->members,
                                       sizeof(MemberInfo*) * (size_t)cls->member_capacity);
            }

            MemberInfo* mem_info = calloc(1, sizeof(MemberInfo));
            mem_info->name = strdup(var->name.value);
            mem_info->type = dt;
            mem_info->is_method = false;
            mem_info->offset = off;
            mem_info->inherited_from = NULL;
            mem_info->field_modifier = var->modifier;
            mem_info->field_class_name = saved_class_name_field
                                   ? strdup(saved_class_name_field) : NULL;

            if (dt == DATA_IDENTIFIER && var->internal_name != NULL) {
                ClassEntry* field_cls = find_class(var->internal_name);
                if (field_cls) {
                    free(mem_info->field_class_name);
                    mem_info->field_class_name = strdup(var->internal_name);
                }
            }

            if (var->Value != NULL) {
                if (var->Value->type == AST_LITERAL) {
                    AST_Literal* lit = (AST_Literal*)var->Value;
                    if (lit->base.type == AST_LITERAL &&
                        lit->base.value.value != NULL) {
                        mem_info->init_value_str = strdup(lit->base.value.value);
                    }
                } else if (var->Value->type == AST_UNARY) {
                    AST_Unary* un = (AST_Unary*)var->Value;
                    if (un->op.type == TOKEN_SUB &&
                        un->right != NULL &&
                        un->right->type == AST_LITERAL) {
                        AST_Literal* lit = (AST_Literal*)un->right;
                        if (lit->base.type == AST_LITERAL &&
                            lit->base.value.value != NULL) {
                            int len = (int)strlen(lit->base.value.value) + 3;
                            mem_info->init_value_str = malloc((size_t)len);
                            if (mem_info->init_value_str)
                                snprintf(mem_info->init_value_str, (size_t)len,
                                         "-%s", lit->base.value.value);
                        }
                    }
                }
            }

            cls->members[cls->member_count++] = mem_info;

            Symbol* field_sym = lookup_symbol(var->name.value);
            if (field_sym) {
                field_sym->is_field = true;
                field_sym->field_offset = off;
                field_sym->field_modifier = var->modifier;
            }
            off += sz;

            if (var->modifier == 2) {
                const char* fname = var->name.value;
                int off_val = mem_info->offset;

                char* gname = make_getter_name(fname);
                char* sname = make_setter_name(fname);

                bool has_getter = false;
                bool has_setter = false;
                for (int j = 0; j < cls->member_count; j++) {
                    if (cls->members[j]->is_method) {
                        if (strcmp(cls->members[j]->name, gname) == 0)
                            has_getter = true;
                        if (strcmp(cls->members[j]->name, sname) == 0)
                            has_setter = true;
                    }
                }

                for (int j = mi + 1; j < mcount; j++) {
                    ASTNode* mn = marray[j];
                    if (mn && mn->type == AST_FUNCTION) {
                        AST_Function* mf = (AST_Function*)mn;
                        if (strcmp(mf->name.value, gname) == 0)
                            has_getter = true;
                        if (strcmp(mf->name.value, sname) == 0)
                            has_setter = true;
                    }
                }

                if (!has_getter) {
                    AST_Function* getter = create_getter_ast(cd->name, fname, dt, off_val);
                    if (getter) APPEND_EXTRA((ASTNode*)getter);
                }
                if (!has_setter) {
                    AST_Function* setter = create_setter_ast(cd->name, fname, dt, off_val);
                    if (setter) APPEND_EXTRA((ASTNode*)setter);
                }

                free(gname);
                free(sname);
            }

            free(saved_class_name_field);
        }

        else if (m->type == AST_FUNCTION) {
            AST_Function* func = (AST_Function*)m;

            AST_Var* this_param = calloc(1, sizeof(AST_Var));
            this_param->base.type = AST_VAR;
            this_param->base.value = (Token){.value = "this", .type = TOKEN_IDENTIFIER};
            this_param->type = TOKEN_IDENTIFIER;
            this_param->name = (Token){.value = "this", .type = TOKEN_IDENTIFIER};
            this_param->dataType = DATA_IDENTIFIER;
            this_param->internal_name = strdup("_this");
            this_param->base.next = func->Parameter;
            func->Parameter = (ASTNode*)this_param;
            func->parameterCount++;

            DataType* new_types = malloc(sizeof(DataType) * (size_t)func->parameterCount);
            new_types[0] = DATA_IDENTIFIER;
            if (func->parameterCount > 1)
                memcpy(new_types + 1, func->parameterType,
                       sizeof(DataType) * (size_t)(func->parameterCount - 1));
            free(func->parameterType);
            func->parameterType = new_types;

            if (!func->internal_name) {
                func->internal_name = generate_method_mangled_name(cd->name, func);
            }

            for (int j = 0; j < cls->member_count; j++) {
                if (cls->members[j]->is_method &&
                    strcmp(cls->members[j]->name, func->name.value) == 0) {
                    if (function_signature_identical(cls->members[j]->func, func)) {
                        ERROR(ERR_SEM_DUPLICATE_VAR,
                              SEMLOC(func->name),
                              "duplicate method '%s'", func->name.value);
                    } else if (function_signature_conflict(cls->members[j]->func, func)) {
                        ERROR(ERR_SEM_DUPLICATE_VAR,
                              SEMLOC(func->name),
                              "duplicate method '%s'", func->name.value);
                    }
                }
            }

            if (cls->member_count >= cls->member_capacity) {
                cls->member_capacity = cls->member_capacity ? cls->member_capacity * 2 : 8;
                cls->members = realloc(cls->members,
                                       sizeof(MemberInfo*) * (size_t)cls->member_capacity);
            }
            MemberInfo* mi = calloc(1, sizeof(MemberInfo));
            mi->name = strdup(func->name.value);
            mi->type = func->Return ? func->Return->returnType : DATA_VOID;
            mi->is_method = true;
            mi->func = func;
            mi->offset = -1;
            mi->inherited_from = NULL;
            cls->members[cls->member_count++] = mi;

            if (strcmp(func->name.value, cd->name) == 0) {
                cls->constructor = func;
            }

            Symbol* func_sym = lookup_symbol(func->name.value);
            if (!func_sym) {
                func_sym = calloc(1, sizeof(Symbol));
                func_sym->key = strdup(func->name.value);
                func_sym->value.type = TOKEN_IDENTIFIER;
                func_sym->value.value = strdup(func->name.value);
                func_sym->type = DATA_INT;
                func_sym->internal_name = strdup(func->internal_name);
                func_sym->is_function = false;
                insert_symbol(func_sym);
            }
        }
    }

    ASTNode* new_head = NULL;
    ASTNode* new_tail = NULL;

    for (int i = 0; i < mcount; i++) {
        if (!new_head) {
            new_head = new_tail = marray[i];
        } else {
            new_tail->next = marray[i];
            new_tail = marray[i];
        }
    }

    if (extra_head) {
        if (!new_head) {
            new_head = extra_head;
        } else {
            new_tail->next = extra_head;
        }
    }

    cd->members = new_head;
    free(marray);
    #undef APPEND_EXTRA

    if (all_parents_valid) {
        for (int p = 0; p < cd->parent_count; p++) {
            ClassEntry* parent = parent_entries[p];
            if (!parent) continue;

            for (int i = 0; i < parent->member_count; i++) {
                MemberInfo* pmi = parent->members[i];
                if (!pmi->is_method) continue;

                bool overridden = false;
                for (int j = 0; j < cls->member_count; j++) {
                    if (cls->members[j]->is_method &&
                        strcmp(cls->members[j]->name, pmi->name) == 0 &&
                        method_signature_match(cls->members[j]->func, pmi->func)) {
                        overridden = true;
                        break;
                    }
                }
                if (overridden) continue;

                bool diamond_conflict = false;
                for (int j = 0; j < cls->member_count; j++) {
                    if (cls->members[j]->is_method &&
                        cls->members[j]->inherited_from != NULL &&
                        strcmp(cls->members[j]->name, pmi->name) == 0 &&
                        method_signature_match(cls->members[j]->func, pmi->func)) {
                        ERROR(ERR_SEM_INCOMPATIBLE_TYPES,
                              SEMLOC(ZERO_TOKEN),
                              "diamond inheritance conflict for method '%s'",
                              pmi->name);
                        diamond_conflict = true;
                        break;
                    }
                }
                if (diamond_conflict) continue;

                if (cls->member_count >= cls->member_capacity) {
                    cls->member_capacity = cls->member_capacity ? cls->member_capacity * 2 : 8;
                    cls->members = realloc(cls->members,
                                           sizeof(MemberInfo*) * (size_t)cls->member_capacity);
                }
                MemberInfo* mi = calloc(1, sizeof(MemberInfo));
                mi->name = strdup(pmi->name);
                mi->type = pmi->type;
                mi->is_method = true;
                mi->func = pmi->func;
                mi->offset = -1;
                mi->inherited_from = parent;
                cls->members[cls->member_count++] = mi;

                Symbol* func_sym = lookup_symbol(pmi->name);
                if (!func_sym) {
                    func_sym = calloc(1, sizeof(Symbol));
                    func_sym->key = strdup(pmi->name);
                    func_sym->value.type = TOKEN_IDENTIFIER;
                    func_sym->value.value = strdup(pmi->name);
                    func_sym->type = DATA_INT;
                    func_sym->internal_name = strdup(pmi->func->internal_name);
                    insert_symbol(func_sym);
                }
            }
        }
    }

    if (!cls->constructor && cls->parent_count > 0) {
        for (int i = 0; i < cls->parent_count; i++) {
            ClassEntry* parent = find_class(cls->parent_names[i]);
            if (parent && parent->constructor) {
                cls->constructor = parent->constructor;
                break;
            }
        }
    }

    cls->total_size = off;
    if (cls->total_size == 0) cls->total_size = 4;
    cls->members_resolved = true;

    exit_scope();
    current_class = saved_class;
    current_class_name = saved_class_name;
    free(parent_entries);
}

static void analyze_class_bodies(AST_ClassDef* cd, ClassEntry* cls) {
    char* saved_class_name = current_class_name;
    ClassEntry* saved_class = current_class;
    current_class_name = cls->name;
    current_class = cls;

    Symbol* cls_sym = lookup_symbol(cd->name);
    if (!cls_sym) {
        cls_sym = create_symbol(
            (Token){.value = cd->name, .type = TOKEN_IDENTIFIER},
            DATA_IDENTIFIER, cd->name, generate_internal_name());
        insert_symbol(cls_sym);
    }

    enter_scope();
    Symbol* this_sym = create_symbol(
        (Token){.value = "this", .type = TOKEN_IDENTIFIER},
        DATA_IDENTIFIER, "this", strdup("_this"));
    this_sym->class_name = strdup(cd->name);
    insert_symbol(this_sym);

    for (int i = 0; i < cls->member_count; i++) {
        MemberInfo* mi = cls->members[i];
        if (mi->is_method) continue;

        Symbol* field_sym = create_symbol(
            (Token){.value = mi->name, .type = TOKEN_IDENTIFIER},
            mi->type,
            mi->name,
            strdup(mi->name)
        );
        field_sym->is_field = true;
        field_sym->field_offset = mi->offset;
        field_sym->field_modifier = mi->field_modifier;
        if (mi->field_class_name) {
            field_sym->class_name = strdup(mi->field_class_name);
        }
        insert_symbol(field_sym);
    }

    ASTNode* m = cd->members;
    while (m) {
        ASTNode* saved_next = m->next;
        m->next = NULL;

        if (m->type == AST_FUNCTION) {
            analyze_node(m);
        }
        else if (m->type == AST_CLASS_DEF) {
            AST_ClassDef* inner = (AST_ClassDef*)m;
            int len = strlen(current_class_name) + 1
                    + strlen(inner->name) + 1;
            char* full_name = malloc(len);
            snprintf(full_name, len, "%s_%s", current_class_name, inner->name);
            free(inner->name);
            inner->name = full_name;

            ClassEntry* inner_cls = find_class(full_name);
            if (inner_cls) {
                analyze_class_bodies(inner, inner_cls);
            }
        }

        m->next = saved_next;
        m = m->next;
    }

    exit_scope();
    current_class = saved_class;
    current_class_name = saved_class_name;
}

static void analyze_node_list(ASTNode* node) {
    while (node) {
        analyze_node(node);
        node = node->next;
    }
}

static void collect_function_return(ASTNode* node, AST_Function* func) {
    if (!node) return;
    if (node->type == AST_RETURN) {
        if (func->Return == NULL) {
            func->Return = (AST_Return*)node;
            func->return_Count = 1;
        }
        return;
    }
    if (node->type == AST_IF) {
        AST_If* if_node = (AST_If*)node;
        collect_function_return(if_node->code, func);
        for (int i = 0; i < if_node->elif_Count; i++)
            collect_function_return(if_node->Elif[i], func);
        if (if_node->has_else)
            collect_function_return(if_node->Else, func);
    }
    if (node->type == AST_WHILE) {
        collect_function_return(((AST_While*)node)->code, func);
    }
    if (node->type == AST_FOR) {
        collect_function_return(((AST_For*)node)->code, func);
    }
    if (node->type == AST_FOREACH) {
        collect_function_return(((AST_ForEach*)node)->code, func);
    }
    if (node->type == AST_SWITCH) {
        AST_Switch* sw = (AST_Switch*)node;
        for (int i = 0; i < sw->case_count; i++)
            collect_function_return(sw->cases[i]->body, func);
        if (sw->has_default)
            collect_function_return(sw->default_body, func);
    }
    if (node->type == AST_CONDITIONAL) {
        AST_Conditional* tern = (AST_Conditional*)node;
        collect_function_return(tern->then_expr, func);
        collect_function_return(tern->else_expr, func);
    }
    collect_function_return(node->next, func);
}

static void analyze_node(ASTNode* temp) {
    if (!temp) return;

    switch (temp->type) {

    case AST_LITERAL: {
        AST_Literal* literal = (AST_Literal*)temp;

        if (literal->is_call_function)
        {
            if (literal->call_function_Parameter &&
                literal->call_function_Parameter->type == AST_MEMBER_ACCESS) {
                AST_MemberAccess* ma = (AST_MemberAccess*)literal->call_function_Parameter;
                ClassEntry* target_class = NULL;
                Symbol* obj_sym = NULL;

                analyze_node(ma->object);

                if (ma->object->type == AST_THIS) {
                    obj_sym = lookup_symbol("this");
                    if (!obj_sym || !obj_sym->class_name) {
                        ERROR(ERR_SEM_CLASS_NOT_FOUND,
                              SEMLOC(ma->base.value),
                              "'this' used outside of class context");
                        literal->dataType = DATA_ERROR;
                        break;
                    }
                    target_class = find_class(obj_sym->class_name);
                }
                else if (ma->object->type == AST_LITERAL) {
                    AST_Literal* obj_lit = (AST_Literal*)ma->object;
                    obj_sym = lookup_symbol(obj_lit->base.value.value);
                    if (!obj_sym || !obj_sym->class_name) {
                        ERROR(ERR_SEM_CLASS_NOT_FOUND,
                              SEMLOC(obj_lit->base.value),
                              "cannot determine class of object '%s'",
                              obj_lit->base.value.value);
                        literal->dataType = DATA_ERROR;
                        break;
                    }
                    target_class = find_class(obj_sym->class_name);
                }
                else {
                    ERROR(ERR_SEM_TYPE_MISMATCH,
                          SEMLOC(ma->base.value),
                          "member access on complex expression not supported");
                    literal->dataType = DATA_ERROR;
                    break;
                }

                if (!target_class) {
                    ERROR(ERR_SEM_CLASS_NOT_FOUND,
                          SEMLOC(ma->base.value),
                          "unknown class for method '%s'",
                          ma->member_name);
                    literal->dataType = DATA_ERROR;
                    break;
                }

                ASTNode* arg_node = literal->method_arguments;
                while (arg_node) {
                    analyze_node(arg_node);
                    arg_node = arg_node->next;
                }
                int total_args = 1 + literal->method_arg_count;
                DataType* call_arg_types = malloc(sizeof(DataType) * total_args);
                call_arg_types[0] = DATA_IDENTIFIER;
                arg_node = literal->method_arguments;
                int idx = 1;
                while (arg_node && idx < total_args) {
                    call_arg_types[idx] = get_expression_type(arg_node);
                    idx++;
                    arg_node = arg_node->next;
                }

                AST_Function* matched_method = NULL;
                for (int i = 0; i < target_class->member_count; i++) {
                    MemberInfo* mi = target_class->members[i];
                    if (mi->is_method && strcmp(mi->name, ma->member_name) == 0) {
                        if (mi->func->parameterCount == total_args) {
                            bool sig_match = true;
                            for (int j = 0; j < total_args; j++) {
                                if (!types_compatible(mi->func->parameterType[j], call_arg_types[j])) {
                                    sig_match = false;
                                    break;
                                }
                            }
                            if (sig_match) {
                                matched_method = mi->func;
                                break;
                            }
                        }
                    }
                }

                if (matched_method) {
                    literal->internal_name = strdup(matched_method->internal_name);
                    literal->dataType = matched_method->Return ?
                        matched_method->Return->returnType : DATA_VOID;
                } else {
                    ERROR(ERR_SEM_METHOD_NOT_FOUND,
                          SEMLOC(ma->base.value),
                          "no matching method '%s'",
                          ma->member_name);
                    literal->dataType = DATA_ERROR;
                }
                free(call_arg_types);

                break;
                } else
                {
                    ASTNode* arg = literal->call_function_Parameter;
                    while (arg) {
                        analyze_node(arg);
                        arg = arg->next;
                    }
                    int arg_count = literal->call_function_ParameterCount;
                    DataType* arg_types = NULL;
                    if (arg_count > 0) {
                        arg_types = malloc(sizeof(DataType) * arg_count);
                        ASTNode* a = literal->call_function_Parameter;
                        int idx = 0;
                        while (a && idx < arg_count) {
                            arg_types[idx] = get_expression_type(a);
                            idx++;
                            a = a->next;
                        }
                    }

                    if (strcmp(literal->base.value.value, "format") == 0 ||
                        strcmp(literal->base.value.value, "_format") == 0) {
                        literal->internal_name = strdup("_format_string");
                        literal->dataType = DATA_STRING;
                        free(arg_types);
                        break;
                    }

                    AST_Function* func_def = find_function_by_signature(
                        literal->base.value.value, arg_count, arg_types);
                    if (func_def) {
                        if (literal->internal_name) free(literal->internal_name);
                        literal->internal_name = strdup(func_def->internal_name);
                        literal->dataType = func_def->Return ?
                            func_def->Return->returnType : DATA_VOID;
                        literal->returns_array = func_def->returns_array;
                        if (func_def->returns_array) {
                            int total = get_type_size(func_def->return_element_type);
                            for (int d = 0; d < func_def->return_dim_count; d++) {
                                if (func_def->return_dim_sizes[d] > 0)
                                    total *= func_def->return_dim_sizes[d];
                            }
                            literal->return_total_bytes = total;
                        } else {
                            literal->return_total_bytes = 0;
                        }
                        if (func_def->parameterCount > 0 &&
                            func_def->parameterType[0] == DATA_IDENTIFIER &&
                            current_class != NULL &&
                            (literal->call_function_Parameter == NULL ||
                             literal->call_function_Parameter->type != AST_THIS)) {
                            AST_This* this_node = calloc(1, sizeof(AST_This));
                            this_node->base.type = AST_THIS;
                            this_node->base.next = literal->call_function_Parameter;
                            literal->call_function_Parameter = (ASTNode*)this_node;
                            literal->call_function_ParameterCount++;
                             }
                    } else {
                        Symbol* sym = lookup_symbol(literal->base.value.value);
                        if (sym && sym->is_function) {
                            ERROR(ERR_SEM_ARG_COUNT_MISMATCH,
                                  SEMLOC(literal->base.value),
                                  "no matching function '%s' for %d argument(s)",
                                  literal->base.value.value, arg_count);
                        } else {
                            ERROR(ERR_SEM_NOT_FUNCTION,
                                  SEMLOC(literal->base.value),
                                  "function '%s' is not declared",
                                  literal->base.value.value);
                        }
                        literal->dataType = DATA_ERROR;
                    }
                    free(arg_types);
                }
        } else {
            if (literal->base.value.type == TOKEN_IDENTIFIER) {
                Symbol* sym = lookup_symbol(literal->base.value.value);
                if (sym == NULL) {
                    DataType alias_type = resolve_type_alias(literal->base.value.value);
                    if (alias_type != DATA_ERROR) {
                        literal->dataType = alias_type;
                    } else
                    {
                        ERROR(ERR_SEM_UNDECLARED_VAR,
                              SEMLOC(literal->base.value),
                              "variable '%s' is not declared",
                              literal->base.value.value);
                        literal->dataType = DATA_ERROR;
                    }
                } else {
                    if (sym->dim_count > 0) {
                        literal->dataType = DATA_INT;
                    } else {
                        literal->dataType = sym->type;
                    }
                    if (sym->is_field) {
                        literal->is_field_ref = true;
                        literal->field_offset = sym->field_offset;
                        free(literal->internal_name);
                        literal->internal_name = NULL;
                    } else {
                        literal->internal_name = strdup(sym->internal_name);
                    }
                }
            } else {
                literal->dataType = Get_Datatype(literal->base.value.type);
                literal->internal_name = NULL;
            }
        }
        break;
    }

    case AST_BINARY: {
        AST_Binary* binary = (AST_Binary*)temp;
        analyze_node(binary->left);
        analyze_node(binary->right);

        if (Operation_Check(binary) && check_division_by_zero(binary)) {
            binary->resultType = Binary_get_Datatype(binary);
        } else {
            binary->resultType = DATA_ERROR;
        }

        if (binary->op.type == TOKEN_BITWISE_AND || binary->op.type == TOKEN_BITWISE_OR) {
            DataType lt = get_expression_type(binary->left);
            DataType rt = get_expression_type(binary->right);
            if ((lt != DATA_INT && lt != DATA_UINT && lt != DATA_CHAR) ||
                (rt != DATA_INT && rt != DATA_UINT && rt != DATA_CHAR)) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(binary->op),
                      "bitwise operators require integer operands");
                binary->resultType = DATA_ERROR;
            }
        }
        break;
    }

    case AST_UNARY: {
        AST_Unary* unary = (AST_Unary*)temp;
        if (unary->right != NULL) {
            analyze_node(unary->right);
        }

        DataType operand_type = DATA_ERROR;
        if (unary->right != NULL) {
            operand_type = get_expression_type(unary->right);
        }

        if (unary->op.type == TOKEN_SUB) {
            if (operand_type == DATA_INT || operand_type == DATA_FLOAT ||
                operand_type == DATA_UINT || operand_type == DATA_UFLOAT) {
                if (operand_type == DATA_UINT) {
                    unary->dataType = DATA_INT;
                } else if (operand_type == DATA_UFLOAT) {
                    unary->dataType = DATA_FLOAT;
                } else {
                    unary->dataType = operand_type;
                }
            } else {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(unary->op),
                      "unary minus requires numeric operand, got '%s'",
                      data_type_to_string(operand_type));
                unary->dataType = DATA_ERROR;
            }
        } else if (unary->op.type == TOKEN_OPPOSE) {
            if (operand_type == DATA_BOOL || operand_type == DATA_INT || operand_type == DATA_UINT) {
                unary->dataType = DATA_BOOL;
            } else {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(unary->op),
                      "logical NOT requires boolean or integer, got '%s'",
                      data_type_to_string(operand_type));
                unary->dataType = DATA_ERROR;
            }
        } else if (unary->op.type == TOKEN_BITWISE_NOT) {
            if (operand_type == DATA_INT || operand_type == DATA_UINT || operand_type == DATA_CHAR) {
                unary->dataType = operand_type;
            } else {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(unary->op),
                      "bitwise NOT requires integer operand");
                unary->dataType = DATA_ERROR;
            }
        } else if (unary->op.type == TOKEN_INCREMENT || unary->op.type == TOKEN_DECREMENT) {
            ASTType op_type = unary->right->type;
            bool is_lvalue = (op_type == AST_LITERAL) ||
                             (op_type == AST_ARRAY_ACCESS) ||
                             (op_type == AST_UNARY);
            if (!is_lvalue) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(unary->op),
                      "prefix '%s' requires an lvalue",
                      unary->op.type == TOKEN_INCREMENT ? "++" : "--");
                unary->dataType = DATA_ERROR;
            } else {
                if (operand_type == DATA_INT   || operand_type == DATA_FLOAT ||
                    operand_type == DATA_UINT  || operand_type == DATA_UFLOAT ||
                    operand_type == DATA_CHAR) {
                    unary->dataType = operand_type;
                } else {
                    ERROR(ERR_SEM_TYPE_MISMATCH,
                          SEMLOC(unary->op),
                          "prefix '%s' requires numeric operand",
                          unary->op.type == TOKEN_INCREMENT ? "++" : "--");
                    unary->dataType = DATA_ERROR;
                }
            }
        }
        break;
    }

    case AST_CONDITIONAL: {
            AST_Conditional* tern = (AST_Conditional*)temp;
            analyze_node(tern->condition);
            analyze_node(tern->then_expr);
            analyze_node(tern->else_expr);
            DataType cond_type = get_expression_type(tern->condition);
            if (cond_type != DATA_BOOL && cond_type != DATA_ERROR) {
                ERROR(ERR_SEM_CONDITION_NOT_BOOL,
                      SEMLOC(tern->condition->value),
                      "ternary condition must be boolean, got '%s'",
                      data_type_to_string(cond_type));
            }
            DataType then_type = get_expression_type(tern->then_expr);
            DataType else_type = get_expression_type(tern->else_expr);
            tern->resultType = L_A_R_get_Datatype(then_type, else_type,
                                                   tern->base.value.line,
                                                   tern->base.value.column);
            break;
    }

    case AST_ARRAY_DECL: {
        AST_ArrayDecl* arr = (AST_ArrayDecl*)temp;

        bool has_unknown_dim = false;
        int total_elements = 1;
        for (int i = 0; i < arr->dim_count; i++) {
            if (arr->dim_sizes[i] == -1) {
                has_unknown_dim = true;
                continue;
            }
            if (arr->dim_sizes[i] <= 0) {
                Token arr_token = {.line = arr->base.value.line, .column = arr->base.value.column};
                ERROR(ERR_SEM_INVALID_INDEX,
                      SEMLOC(arr_token),
                      "invalid dimension size %d for array '%s'",
                      arr->dim_sizes[i], arr->name);
                total_elements = 0;
                break;
            }
            total_elements *= arr->dim_sizes[i];
        }

        if (arr->init_list != NULL) {
            analyze_node(arr->init_list);
            if (arr->init_list->type == AST_NEW_ARRAY) {
                AST_NewArray* narr = (AST_NewArray*)arr->init_list;
                for (int i = 0; i < arr->dim_count && i < narr->dim_count; i++) {
                    if (arr->dim_sizes[i] == -1) {
                        arr->dim_sizes[i] = narr->dim_sizes[i];
                    }
                }
                total_elements = 1;
                for (int i = 0; i < arr->dim_count; i++) {
                    if (arr->dim_sizes[i] > 0) {
                        total_elements *= arr->dim_sizes[i];
                    }
                }
                has_unknown_dim = false;
            }
            else if (arr->init_list->type == AST_INIT_LIST) {
                AST_InitList* list = (AST_InitList*)arr->init_list;
                if (has_unknown_dim && list->dim_count > 0) {
                    for (int i = 0; i < arr->dim_count; i++) {
                        if (arr->dim_sizes[i] == -1 && i < list->dim_count) {
                            arr->dim_sizes[i] = list->dim_sizes[i];
                        }
                    }
                    total_elements = 1;
                    for (int i = 0; i < arr->dim_count; i++) {
                        if (arr->dim_sizes[i] > 0) {
                            total_elements *= arr->dim_sizes[i];
                        }
                    }
                    has_unknown_dim = false;
                }
                if (list->count > total_elements && total_elements > 0) {
                    Token arr_token2 = {.line = arr->base.value.line, .column = arr->base.value.column};
                    ERROR(ERR_SEM_INVALID_INDEX,
                          SEMLOC(arr_token2),
                          "too many initializers for array '%s'",
                          arr->name);
                }
                if (list->elemType != DATA_ERROR && list->elemType != arr->elementType) {
                    bool compatible = false;
                    if (arr->elementType == DATA_UINT && list->elemType == DATA_INT) compatible = true;
                    else if (arr->elementType == DATA_FLOAT && list->elemType == DATA_INT) compatible = true;
                    else if (arr->elementType == DATA_UFLOAT && list->elemType == DATA_FLOAT) compatible = true;
                    else if (arr->elementType == DATA_UFLOAT && list->elemType == DATA_INT) compatible = true;
                    else if (arr->elementType == DATA_UFLOAT && list->elemType == DATA_UINT) compatible = true;
                    if (!compatible) {
                        Token arr_token3 = {.line = arr->base.value.line, .column = arr->base.value.column};
                        ERROR(ERR_SEM_TYPE_MISMATCH,
                              SEMLOC(arr_token3),
                              "array element type mismatch: expected '%s', got '%s'",
                              data_type_to_string(arr->elementType),
                              data_type_to_string(list->elemType));
                    }
                }
            }
        } else {
            if (!in_param_list && has_unknown_dim) {
                Token arr_token4 = {.line = arr->base.value.line, .column = arr->base.value.column};
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(arr_token4),
                      "array '%s' of unknown size must be initialized",
                      arr->name);
            }
        }

        char* internal = generate_internal_name();
        arr->internal_name = strdup(internal);

        Symbol* sym = create_symbol(
            (Token){.value = arr->name, .type = TOKEN_IDENTIFIER},
            arr->elementType,
            arr->name,
            internal
        );

        sym->dim_count = arr->dim_count;
        sym->dim_sizes = malloc(sizeof(int) * arr->dim_count);
        memcpy(sym->dim_sizes, arr->dim_sizes, sizeof(int) * arr->dim_count);

        insert_symbol(sym);
        free(internal);
        break;
    }

    case AST_ARRAY_ACCESS: {
        AST_ArrayAccess* acc = (AST_ArrayAccess*)temp;
        analyze_node(acc->array);
        for (int i = 0; i < acc->index_count; i++) {
            analyze_node(acc->indices[i]);
        }

        Symbol* sym = NULL;
        if (acc->array->type == AST_LITERAL) {
            AST_Literal* lit = (AST_Literal*)acc->array;
            sym = lookup_symbol(lit->base.value.value);
        }

        if (sym && sym->dim_count > 0) {
            int totalDims = sym->dim_count;
            acc->full_dim_count = totalDims;
            acc->full_dim_sizes = malloc(sizeof(int) * totalDims);
            memcpy(acc->full_dim_sizes, sym->dim_sizes, sizeof(int) * totalDims);

            if (acc->index_count > totalDims) {
                ERROR(ERR_SEM_INVALID_INDEX,
                      SEMLOC(acc->indices[totalDims]->value),
                      "too many indices for array");
                acc->elementType = DATA_ERROR;
            } else {
                int remaining = totalDims - acc->index_count;
                acc->dim_count = remaining;
                if (remaining > 0) {
                    acc->dim_sizes = malloc(sizeof(int) * remaining);
                    memcpy(acc->dim_sizes, &sym->dim_sizes[acc->index_count], sizeof(int) * remaining);
                    acc->elementType = sym->type;
                } else {
                    acc->elementType = sym->type;
                    acc->dim_sizes = NULL;
                }
            }

            for (int i = 0; i < acc->index_count && i < totalDims; i++) {
                ASTNode* idxNode = acc->indices[i];
                if (idxNode->type == AST_LITERAL) {
                    AST_Literal* idxLit = (AST_Literal*)idxNode;
                    if (idxLit->base.value.type == TOKEN_NUMBER) {
                        int idxVal = atoi(idxLit->base.value.value);
                        int dimSize = acc->full_dim_sizes[i];
                        if (idxVal < 0 || idxVal >= dimSize) {
                            ERROR(ERR_SEM_INVALID_INDEX,
                                  SEMLOC(idxLit->base.value),
                                  "array index %d out of bounds (size %d)",
                                  idxVal, dimSize);
                        }
                    }
                }
            }
        } else {
            acc->elementType = DATA_ERROR;
            acc->full_dim_count = 0;
            acc->full_dim_sizes = NULL;
        }
        break;
    }

    case AST_NEW_ARRAY: {
            AST_NewArray* newArr = (AST_NewArray*)temp;
            if (newArr->dim_exprs) {
                for (int i = 0; i < newArr->dim_count; i++) {
                    if (newArr->dim_exprs[i]) {
                        analyze_node(newArr->dim_exprs[i]);
                    }
                }
            }
            int total = 1;
            for (int i = 0; i < newArr->dim_count; i++) {
                if (newArr->dim_sizes[i] == -1) {
                    continue;
                }
                if (newArr->dim_sizes[i] <= 0) {
                    Token narr_token = {.line = newArr->base.value.line, .column = newArr->base.value.column};
                    ERROR(ERR_SEM_INVALID_INDEX,
                          SEMLOC(narr_token),
                          "array dimension must be positive");
                }
                total *= newArr->dim_sizes[i];
            }
            break;
    }

    case AST_ARRAY_ASS: {
        AST_ArrayAss* ass = (AST_ArrayAss*)temp;
        if (ass->value) analyze_node(ass->value);
        if (ass->target) analyze_node(ass->target);

        AST_ArrayAccess* acc = (AST_ArrayAccess*)ass->target;
        DataType elemType = (acc) ? acc->elementType : DATA_ERROR;
        ass->elementType = elemType;

        DataType valType = get_expression_type(ass->value);
        if (elemType != DATA_ERROR && valType != DATA_ERROR && elemType != valType) {
            bool allowed = false;
            bool t_int = (elemType == DATA_INT || elemType == DATA_UINT ||
                          elemType == DATA_CHAR || elemType == DATA_BOOL);
            bool s_int = (valType  == DATA_INT || valType  == DATA_UINT ||
                          valType  == DATA_CHAR || valType  == DATA_BOOL);
            if (t_int && s_int) allowed = true;

            bool t_float = (elemType == DATA_FLOAT || elemType == DATA_UFLOAT);
            bool s_float = (valType  == DATA_FLOAT || valType  == DATA_UFLOAT);
            if (t_float && s_float) allowed = true;

            if ((elemType == DATA_FLOAT || elemType == DATA_UFLOAT) && s_int) allowed = true;
            if (t_int && s_float) allowed = true;

            if ((elemType == DATA_STRING && valType == DATA_CHAR) ||
                (elemType == DATA_CHAR  && valType == DATA_STRING)) allowed = true;

            if (elemType == DATA_UINT || elemType == DATA_UFLOAT) {
                check_unsigned_assignment(elemType, ass->value,
                                          ass->base.value.line,
                                          ass->base.value.column);
            }

            if (!allowed) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(ass->base.value),
                      "array element type mismatch: expected '%s', got '%s'",
                      data_type_to_string(elemType), data_type_to_string(valType));
            }
        }
        break;
    }

    case AST_INIT_LIST: {
        AST_InitList* list = (AST_InitList*)temp;
        DataType commonType = DATA_ERROR;
        int nested_dim = 0;
        int inner_size = -1;

        for (int i = 0; i < list->count; i++) {
            analyze_node(list->elements[i]);
            ASTNode* elem = list->elements[i];
            DataType t;
            if (elem->type == AST_INIT_LIST) {
                AST_InitList* sub = (AST_InitList*)elem;
                t = sub->elemType;
                if (nested_dim == 0) {
                    nested_dim = 1;
                    inner_size = sub->count;
                    list->dim_count = sub->dim_count + 1;
                    list->dim_sizes = malloc(sizeof(int) * list->dim_count);
                    list->dim_sizes[0] = list->count;
                    memcpy(&list->dim_sizes[1], sub->dim_sizes, sizeof(int) * sub->dim_count);
                } else {
                    if (sub->count != inner_size)
                    {
                        Token list_token2 = {.line = list->base.value.line, .column = list->base.value.column};
                        ERROR(ERR_SEM_TYPE_MISMATCH,
                              SEMLOC(list_token2),
                              "type mismatch in initializer");
                    }
                }
            } else {
                t = get_expression_type(elem);
                if (nested_dim == 0) {
                    list->dim_count = 1;
                    list->dim_sizes = malloc(sizeof(int));
                    list->dim_sizes[0] = list->count;
                }
            }
            if (t == DATA_ERROR) continue;
            if (commonType == DATA_ERROR) commonType = t;
            else if (commonType != t) {
                if (commonType == DATA_INT && t == DATA_FLOAT) commonType = DATA_FLOAT;
                else if (commonType == DATA_FLOAT && t == DATA_INT) commonType = DATA_FLOAT;
                else if (commonType == DATA_UINT && t == DATA_INT) commonType = DATA_UINT;
                else if (commonType == DATA_INT && t == DATA_UINT) commonType = DATA_UINT;
                else if (commonType == DATA_FLOAT && t == DATA_UFLOAT) commonType = DATA_UFLOAT;
                else if (commonType == DATA_UFLOAT && t == DATA_FLOAT) commonType = DATA_UFLOAT;
                else if (commonType == DATA_UINT && t == DATA_UFLOAT) commonType = DATA_UFLOAT;
                else if (commonType == DATA_UFLOAT && t == DATA_UINT) commonType = DATA_UFLOAT;
                else if (commonType != t)
                {
                    Token type_token = {.line = list->base.value.line, .column = list->base.value.column};
                    ERROR(ERR_SEM_TYPE_MISMATCH,
                        SEMLOC(type_token),
                        "type mismatch in initializer");
                }
            }
        }
        list->elemType = commonType;
        break;
    }

    case AST_ARRAY_LENGTH: {
        AST_ArrayLength* len = (AST_ArrayLength*)temp;
        analyze_node(len->array);

        if (len->array->type == AST_LITERAL) {
            AST_Literal* lit = (AST_Literal*)len->array;
            Symbol* sym = lookup_symbol(lit->base.value.value);
            if (sym == NULL) {
                ERROR(ERR_SEM_UNDECLARED_VAR,
                      SEMLOC(lit->base.value),
                      "variable '%s' is not declared",
                      lit->base.value.value);
            } else if (sym->dim_count == 0) {
                ERROR(ERR_SEM_NOT_ARRAY,
                      SEMLOC(lit->base.value),
                      "'.length' requires an array");
            }
        }
        else if (len->array->type == AST_ARRAY_ACCESS) {
        }
        else {
            ERROR(ERR_SEM_NOT_ARRAY,
                  SEMLOC(len->base.value),
                  "'.length' requires an array");
        }
        break;
    }

    case AST_PRINT: {
        AST_Print* print = (AST_Print*)temp;

        ASTNode** prev_ptr = &print->Parameter;
        ASTNode* param = print->Parameter;
        while (param) {
            if (param->type == AST_LITERAL) {
                AST_Literal* lit = (AST_Literal*)param;
                if (lit->base.value.type == TOKEN_IDENTIFIER && !lit->is_call_function) {
                    Symbol* sym = lookup_symbol(lit->base.value.value);
                    if (sym && sym->dim_count > 0) {
                        AST_Literal* call = calloc(1, sizeof(AST_Literal));
                        call->base.type = AST_LITERAL;
                        call->base.next = param->next;
                        call->base.value.type = TOKEN_IDENTIFIER;
                        call->base.value.value = strdup("_arrayToString");
                        call->dataType = DATA_STRING;
                        call->internal_name = strdup("_arrayToString");
                        call->is_call_function = true;

                        AST_Literal* arr_node = calloc(1, sizeof(AST_Literal));
                        arr_node->base.type = AST_LITERAL;
                        arr_node->base.next = NULL;
                        arr_node->base.value = lit->base.value;
                        arr_node->dataType = sym->type;
                        arr_node->internal_name = strdup(sym->internal_name);
                        arr_node->is_call_function = false;

                        call->call_function_Parameter = (ASTNode*)arr_node;
                        call->call_function_ParameterCount = 1;

                        *prev_ptr = (ASTNode*)call;
                        prev_ptr = &call->base.next;
                        param = call->base.next;
                        continue;
                    }
                }
            }
            else if (param->type == AST_ARRAY_ACCESS) {
                AST_ArrayAccess* acc = (AST_ArrayAccess*)param;
                if (acc->dim_count > 0) {
                    AST_Literal* call = calloc(1, sizeof(AST_Literal));
                    call->base.type = AST_LITERAL;
                    call->base.next = param->next;
                    call->base.value.type = TOKEN_IDENTIFIER;
                    call->base.value.value = strdup("_arrayToString");
                    call->dataType = DATA_STRING;
                    call->internal_name = strdup("_arrayToString");
                    call->is_call_function = true;

                    param->next = NULL;
                    call->call_function_Parameter = param;
                    call->call_function_ParameterCount = 1;

                    *prev_ptr = (ASTNode*)call;
                    prev_ptr = &call->base.next;
                    param = call->base.next;
                    continue;
                }
            }
            prev_ptr = &param->next;
            param = param->next;
        }

        analyze_node_list(print->Parameter);
        break;
    }

    case AST_INPUT: {
        AST_Input* inp = (AST_Input*)temp;
        analyze_node(inp->format_string);
        for (int i = 0; i < inp->arg_count; i++) {
            analyze_node(inp->args[i]);
            AST_Literal* var = (AST_Literal*)inp->args[i];
            Symbol* sym = lookup_symbol(var->base.value.value);
            if (!sym) {
                ERROR(ERR_SEM_UNDECLARED_VAR,
                      SEMLOC(var->base.value),
                      "variable '%s' is not declared",
                      var->base.value.value);
            }
            var->internal_name = sym ? strdup(sym->internal_name) : NULL;
            var->dataType = sym ? sym->type : DATA_ERROR;
        }
        break;
    }

    case AST_VAR: {
        AST_Var* var = (AST_Var*)temp;
        if (var->Value) analyze_node(var->Value);

        char* internal = generate_internal_name();
        var->dataType = Get_Datatype(var->type);

        if (var->dataType == DATA_IDENTIFIER && var->internal_name != NULL) {
            DataType resolved = resolve_type_alias(var->internal_name);
            if (resolved != DATA_ERROR) {
                var->dataType = resolved;
            }
        }
        Symbol* sym = create_symbol(var->name, var->dataType, var->name.value, internal);
        if (strcmp(var->name.value, "this") == 0 && current_class_name != NULL) {
            sym->class_name = strdup(current_class_name);
        }
        if (var->dataType == DATA_IDENTIFIER && var->internal_name != NULL) {
            ClassEntry* cls = find_class(var->internal_name);
            if (cls) {
                sym->class_name = strdup(var->internal_name);
            }
        }
        insert_symbol(sym);
        var->internal_name = strdup(internal);
        free(internal);

        if (var->Value && var->Value->type == AST_NEW_INSTANCE) {
            AST_NewInstance* ni = (AST_NewInstance*)var->Value;
            sym->class_name = strdup(ni->class_name);
        }

        if (var->Value != NULL) {
            check_unsigned_assignment(var->dataType, var->Value,
                                      var->name.line, var->name.column);

            DataType expr_type = get_expression_type(var->Value);
            if (expr_type != var->dataType && expr_type != DATA_ERROR) {
                bool ok = false;
                bool target_int_family = (var->dataType == DATA_INT  ||
                          var->dataType == DATA_UINT ||
                          var->dataType == DATA_CHAR ||
                          var->dataType == DATA_BOOL);
                bool source_int_family = (expr_type     == DATA_INT  ||
                                          expr_type     == DATA_UINT ||
                                          expr_type     == DATA_CHAR ||
                                          expr_type     == DATA_BOOL);
                if (target_int_family && source_int_family) ok = true;
                bool target_float_family = (var->dataType == DATA_FLOAT ||
                                            var->dataType == DATA_UFLOAT);
                bool source_float_family = (expr_type     == DATA_FLOAT ||
                                            expr_type     == DATA_UFLOAT);
                if (target_float_family && source_float_family) ok = true;

                if ((var->dataType == DATA_FLOAT || var->dataType == DATA_UFLOAT)
                    && source_int_family) ok = true;

                if (target_int_family && source_float_family) ok = true;

                if ((var->dataType == DATA_STRING && expr_type == DATA_CHAR) ||
                    (var->dataType == DATA_CHAR  && expr_type == DATA_STRING))
                    ok = true;
                if (var->dataType == DATA_IDENTIFIER || expr_type == DATA_IDENTIFIER)
                    ok = true;
                if (var->dataType == DATA_IDENTIFIER && (expr_type == DATA_INT || expr_type == DATA_UINT))
                    ok = true;
                if (!ok) {
                    ERROR(ERR_SEM_TYPE_MISMATCH,
                          SEMLOC(var->name),
                          "type mismatch: expected '%s', got '%s'",
                          data_type_to_string(var->dataType),
                          data_type_to_string(expr_type));
                }
            }
        }

        if (var->Value && var->Value->type == AST_INIT_LIST) {
            AST_InitList* list = (AST_InitList*)var->Value;
            sym->dim_count = list->dim_count;
            sym->dim_sizes = malloc(sizeof(int) * list->dim_count);
            if (sym->dim_sizes && list->dim_sizes)
                memcpy(sym->dim_sizes, list->dim_sizes, sizeof(int) * list->dim_count);

            var->dim_count = list->dim_count;
            var->dim_sizes = malloc(sizeof(int) * list->dim_count);
            if (var->dim_sizes && list->dim_sizes)
                memcpy(var->dim_sizes, list->dim_sizes, sizeof(int) * list->dim_count);
        }
        else if (var->Value && var->Value->type == AST_NEW_ARRAY) {
            AST_NewArray* narr = (AST_NewArray*)var->Value;
            sym->dim_count = narr->dim_count;
            sym->dim_sizes = malloc(sizeof(int) * narr->dim_count);
            memcpy(sym->dim_sizes, narr->dim_sizes, sizeof(int) * narr->dim_count);
            var->dim_count = narr->dim_count;
            var->dim_sizes = malloc(sizeof(int) * narr->dim_count);
            memcpy(var->dim_sizes, narr->dim_sizes, sizeof(int) * narr->dim_count);
        }
        if (sym->is_field) {
            var->is_field_ref = true;
            var->field_offset = sym->field_offset;
        } else {
            var->is_field_ref = false;
            var->field_offset = 0;
        }
        break;
    }

    case AST_UNINIT_VAR: {
        AST_Var* var = (AST_Var*)temp;
        char* internal = generate_internal_name();
        var->dataType = Get_Datatype(var->type);
        if (var->dataType == DATA_IDENTIFIER && var->internal_name != NULL) {
            DataType resolved = resolve_type_alias(var->internal_name);
            if (resolved != DATA_ERROR) {
                var->dataType = resolved;
            }
        }

        Symbol* sym = create_symbol(var->name, var->dataType, var->name.value, internal);
        if (strcmp(var->name.value, "this") == 0 && current_class_name != NULL) {
            sym->class_name = strdup(current_class_name);
        }
        if (var->dataType == DATA_IDENTIFIER && var->internal_name != NULL) {
            ClassEntry* cls = find_class(var->internal_name);
            if (cls) {
                sym->class_name = strdup(var->internal_name);
            }
        }
        insert_symbol(sym);

        var->internal_name = strdup(internal);
        free(internal);

        if (var->dim_count > 0) {
            ERROR(ERR_SEM_TYPE_MISMATCH,
                  SEMLOC(var->name),
                  "dynamic array must be initialized at declaration");
        }
        break;
    }

    case AST_VAR_ASS: {
        AST_Var* ast_var_ass = (AST_Var*)temp;

        analyze_node(ast_var_ass->Value);

        Symbol* sym = lookup_symbol(ast_var_ass->name.value);
        if (sym == NULL) {
            ERROR(ERR_SEM_UNDECLARED_VAR,
                  SEMLOC(ast_var_ass->name),
                  "variable '%s' is not declared",
                  ast_var_ass->name.value);
            ast_var_ass->dataType = DATA_ERROR;
        } else {
            if (ast_var_ass->Value && ast_var_ass->Value->type == AST_NEW_INSTANCE) {
                if (sym->class_name) free(sym->class_name);
                AST_NewInstance* ni = (AST_NewInstance*)ast_var_ass->Value;
                sym->class_name = strdup(ni->class_name);
            }
            ast_var_ass->dataType = sym->type;
            ast_var_ass->internal_name = strdup(sym->internal_name);

            if (ast_var_ass->Value != NULL) {
                check_unsigned_assignment(sym->type, ast_var_ass->Value,
                                          ast_var_ass->name.line,
                                          ast_var_ass->name.column);
            }

            DataType expr_type = get_expression_type(ast_var_ass->Value);
            if (expr_type != sym->type && expr_type != DATA_ERROR) {
                bool ok = false;
                bool target_int_family = (sym->type == DATA_INT  ||
                                          sym->type == DATA_UINT ||
                                          sym->type == DATA_CHAR ||
                                          sym->type == DATA_BOOL);
                bool source_int_family = (expr_type     == DATA_INT  ||
                                          expr_type     == DATA_UINT ||
                                          expr_type     == DATA_CHAR ||
                                          expr_type     == DATA_BOOL);
                if (target_int_family && source_int_family) ok = true;

                bool target_float_family = (sym->type == DATA_FLOAT ||
                                            sym->type == DATA_UFLOAT);
                bool source_float_family = (expr_type     == DATA_FLOAT ||
                                            expr_type     == DATA_UFLOAT);
                if (target_float_family && source_float_family) ok = true;

                if ((sym->type == DATA_FLOAT || sym->type == DATA_UFLOAT)
                    && source_int_family) ok = true;

                if (target_int_family && source_float_family) ok = true;

                if ((sym->type == DATA_STRING && expr_type == DATA_CHAR) ||
                    (sym->type == DATA_CHAR  && expr_type == DATA_STRING)) ok = true;
                if (sym->type == DATA_IDENTIFIER && (expr_type == DATA_INT || expr_type == DATA_UINT))
                    ok = true;

                if (!ok) {
                    ERROR(ERR_SEM_TYPE_MISMATCH,
                          SEMLOC(ast_var_ass->name),
                          "type mismatch: expected '%s', got '%s'",
                          data_type_to_string(sym->type),
                          data_type_to_string(expr_type));
                }
            }
        }
        if (sym && sym->is_field) {
            ast_var_ass->is_field_ref = true;
            ast_var_ass->field_offset = sym->field_offset;
        } else {
            ast_var_ass->is_field_ref = false;
            ast_var_ass->field_offset = 0;
        }
        break;
    }

    case AST_IF: {
        AST_If* ast_if = (AST_If*)temp;

        if (ast_if->Condition != NULL) {
            analyze_node(ast_if->Condition);

            DataType cond_type = get_expression_type(ast_if->Condition);
            if (cond_type != DATA_BOOL && cond_type != DATA_ERROR) {
                ERROR(ERR_SEM_CONDITION_NOT_BOOL,
                      SEMLOC(ast_if->Condition->value),
                      "condition must be boolean, got '%s'",
                      data_type_to_string(cond_type));
            }
        }

        enter_scope();
        if (ast_if->code != NULL) {
            analyze_node_list(ast_if->code);
        }
        exit_scope();

        if (ast_if->elif_Count > 0) {
            for (int i = 0; i < ast_if->elif_Count; i++) {
                if (ast_if->elif_condition[i] != NULL) {
                    analyze_node(ast_if->elif_condition[i]);

                    DataType cond_type = get_expression_type(ast_if->elif_condition[i]);
                    if (cond_type != DATA_BOOL && cond_type != DATA_ERROR) {
                        ERROR(ERR_SEM_CONDITION_NOT_BOOL,
                              SEMLOC(ast_if->elif_condition[i]->value),
                              "condition must be boolean, got '%s'",
                              data_type_to_string(cond_type));
                    }
                }

                enter_scope();
                if (ast_if->Elif[i] != NULL) {
                    analyze_node_list(ast_if->Elif[i]);
                }
                exit_scope();
            }
        }

        if (ast_if->Else != NULL) {
            enter_scope();
            analyze_node_list(ast_if->Else);
            exit_scope();
        }
        break;
    }

    case AST_WHILE: {
        AST_While* while_node = (AST_While*)temp;

        if (while_node->Condition != NULL) {
            analyze_node(while_node->Condition);

            DataType cond_type = get_expression_type(while_node->Condition);
            if (cond_type != DATA_BOOL && cond_type != DATA_ERROR) {
                ERROR(ERR_SEM_CONDITION_NOT_BOOL,
                      SEMLOC(while_node->Condition->value),
                      "condition must be boolean, got '%s'",
                      data_type_to_string(cond_type));
            }
        }

        enter_scope();
        if (while_node->code != NULL) {
            analyze_node_list(while_node->code);
        }
        exit_scope();
        break;
    }

    case AST_FOR: {
        AST_For* for_node = (AST_For*)temp;
        enter_scope();

        if (for_node->init) analyze_node_list(for_node->init);

        if (for_node->condition) {
            analyze_node(for_node->condition);
            DataType cond_type = get_expression_type(for_node->condition);
            if (cond_type != DATA_BOOL && cond_type != DATA_ERROR) {
                ERROR(ERR_SEM_CONDITION_NOT_BOOL,
                      SEMLOC(for_node->condition->value),
                      "condition must be boolean");
            }
        }

        if (for_node->increment) analyze_node_list(for_node->increment);

        if (for_node->code) analyze_node_list(for_node->code);

        exit_scope();
        break;
    }

    case AST_FOREACH: {
        AST_ForEach* foreach_node = (AST_ForEach*)temp;

        analyze_node(foreach_node->array);

        DataType elemType = DATA_ERROR;
        if (foreach_node->array->type == AST_LITERAL) {
            AST_Literal* lit = (AST_Literal*)foreach_node->array;
            Symbol* sym = lookup_symbol(lit->base.value.value);
            if (sym && sym->dim_count > 0) {
                elemType = sym->type;
            } else {
                ERROR(ERR_SEM_NOT_ARRAY,
                      SEMLOC(lit->base.value),
                      "cannot iterate over non-array type");
            }
        } else if (foreach_node->array->type == AST_ARRAY_ACCESS) {
            AST_ArrayAccess* acc = (AST_ArrayAccess*)foreach_node->array;
            elemType = acc->elementType;
        } else {
            ERROR(ERR_SEM_NOT_ARRAY,
                  SEMLOC(foreach_node->iterator),
                  "cannot iterate over non-array type");
        }

        if (foreach_node->has_explicit_type) {
            DataType declared = foreach_node->elemType;
            if (elemType != DATA_ERROR &&
                !types_compatible(declared, elemType)) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(foreach_node->iterator),
                      "foreach iterator type '%s' is incompatible with "
                      "array element type '%s'",
                      data_type_to_string(declared),
                      data_type_to_string(elemType));
                }
            foreach_node->elemType = declared;
        } else {
            foreach_node->elemType = elemType;
        }

        enter_scope();
        char* internal = generate_internal_name();
        Symbol* iter_sym = create_symbol(foreach_node->iterator, elemType,
                                         foreach_node->iterator.value, internal);
        insert_symbol(iter_sym);
        foreach_node->internal_iterator = strdup(internal);
        free(internal);

        if (foreach_node->code) analyze_node_list(foreach_node->code);
        exit_scope();
        break;
    }

    case AST_SWITCH: {
        AST_Switch* sw = (AST_Switch*)temp;
        analyze_node(sw->condition);
        DataType cond_type = get_expression_type(sw->condition);
        if (cond_type != DATA_INT && cond_type != DATA_CHAR &&
            cond_type != DATA_BOOL && cond_type != DATA_UINT && cond_type != DATA_ERROR) {
            ERROR(ERR_SEM_TYPE_MISMATCH,
                  SEMLOC(sw->condition->value),
                  "switch condition must be integer type");
        }

        for (int i = 0; i < sw->case_count; i++) {
            AST_Case* cs = sw->cases[i];
            if (!cs->is_default) {
                analyze_node(cs->value);
            }
            if (cs->body) analyze_node_list(cs->body);
        }
        if (sw->has_default && sw->default_body) {
            analyze_node_list(sw->default_body);
        }

        for (int i = 0; i < sw->case_count; i++) {
            AST_Case* cs1 = sw->cases[i];
            if (cs1->is_default) continue;
            if (cs1->value->type != AST_LITERAL) continue;
            AST_Literal* lit1 = (AST_Literal*)cs1->value;
            if (lit1->base.value.type != TOKEN_NUMBER &&
                lit1->base.value.type != TOKEN_CHAR_LITERAL &&
                lit1->base.value.type != TOKEN_BOOL_LITERAL) continue;

            for (int j = i + 1; j < sw->case_count; j++) {
                AST_Case* cs2 = sw->cases[j];
                if (cs2->is_default) continue;
                if (cs2->value->type != AST_LITERAL) continue;
                AST_Literal* lit2 = (AST_Literal*)cs2->value;
                if (lit2->base.value.type != TOKEN_NUMBER &&
                    lit2->base.value.type != TOKEN_CHAR_LITERAL &&
                    lit2->base.value.type != TOKEN_BOOL_LITERAL) continue;

                bool is_duplicate = false;
                if (lit1->base.value.type == TOKEN_NUMBER &&
                    lit2->base.value.type == TOKEN_NUMBER) {
                    int val1 = atoi(lit1->base.value.value);
                    int val2 = atoi(lit2->base.value.value);
                    if (val1 == val2) is_duplicate = true;
                }
                else if (lit1->base.value.type == TOKEN_CHAR_LITERAL &&
                         lit2->base.value.type == TOKEN_CHAR_LITERAL) {
                    if (strcmp(lit1->base.value.value, lit2->base.value.value) == 0)
                        is_duplicate = true;
                }
                else if (lit1->base.value.type == TOKEN_BOOL_LITERAL &&
                         lit2->base.value.type == TOKEN_BOOL_LITERAL) {
                    if (strcmp(lit1->base.value.value, lit2->base.value.value) == 0)
                        is_duplicate = true;
                }
                else if (lit1->base.value.type == TOKEN_NUMBER &&
                         lit2->base.value.type == TOKEN_CHAR_LITERAL) {
                    int val1 = atoi(lit1->base.value.value);
                    int val2 = (unsigned char)lit2->base.value.value[0];
                    if (val1 == val2) is_duplicate = true;
                }
                else if (lit1->base.value.type == TOKEN_CHAR_LITERAL &&
                         lit2->base.value.type == TOKEN_NUMBER) {
                    int val1 = (unsigned char)lit1->base.value.value[0];
                    int val2 = atoi(lit2->base.value.value);
                    if (val1 == val2) is_duplicate = true;
                }

                if (is_duplicate) {
                    ERROR(ERR_SEM_DUPLICATE_CASE,
                          SEMLOC(lit2->base.value),
                          "duplicate case value '%s'",
                          lit2->base.value.value);
                }
            }
        }
        break;
    }

    case AST_BREAK:
    case AST_CONTINUE:
        break;

    case AST_FUNCTION: {
        AST_Function* function_node = (AST_Function*)temp;

        AST_Function* saved_func = current_analyzing_function;
        current_analyzing_function = function_node;

        bool had_preset_return = (function_node->Return != NULL);
        AST_Return* saved_return = function_node->Return;
        int saved_return_count = function_node->return_Count;

        function_node->Return = NULL;
        function_node->return_Count = 0;
        collect_function_return(function_node->Code, function_node);

        if (function_node->Code == NULL && had_preset_return) {
            function_node->Return = saved_return;
            function_node->return_Count = saved_return_count;
        } else if (function_node->Code == NULL && function_node->Return == NULL && had_preset_return) {
            function_node->Return = saved_return;
            function_node->return_Count = saved_return_count;
        }
        if (function_node->internal_name == NULL) {
            int len = strlen(function_node->name.value) + 1;
            for (int i = 0; i < function_node->parameterCount; i++) {
                len += 1 + strlen(data_type_to_string(function_node->parameterType[i]));
            }
            char* sig_name = malloc(len + 1);
            if (sig_name) {
                sprintf(sig_name, "%s", function_node->name.value);
                char* p = sig_name + strlen(sig_name);
                for (int i = 0; i < function_node->parameterCount; i++) {
                    *p++ = '_';
                    const char* tn = data_type_to_string(function_node->parameterType[i]);
                    strcpy(p, tn);
                    p += strlen(tn);
                }
                *p = '\0';
                function_node->internal_name = sig_name;
            }
        }

        Symbol* func_sym = lookup_symbol(function_node->name.value);
        if (!func_sym) {
            func_sym = calloc(1, sizeof(Symbol));
            func_sym->key = strdup(function_node->name.value);
            func_sym->value.type = TOKEN_IDENTIFIER;
            func_sym->value.value = strdup(function_node->name.value);
            func_sym->type = DATA_INT;
            func_sym->internal_name = strdup(function_node->internal_name);
            func_sym->is_function = true;
            insert_symbol(func_sym);
        } else {
            if (func_sym->internal_name) free(func_sym->internal_name);
            func_sym->internal_name = strdup(function_node->internal_name);
        }

        enter_scope();

        in_param_list = true;
        if (function_node->Parameter != NULL) {
            analyze_node_list(function_node->Parameter);
        }
        in_param_list = false;

        if (function_node->Code != NULL) {
            analyze_node_list(function_node->Code);
        }

        if (function_node->Code == NULL && function_node->Return != NULL) {
            analyze_node((ASTNode*)function_node->Return);
        }

        exit_scope();

        if (strcmp(function_node->name.value, "main") == 0) {
            if (function_node->parameterCount > 0) {
                main_has_args = true;
            }
        }
        current_analyzing_function = saved_func;
        break;
    }

    case AST_MEMBER_ACCESS: {
        AST_MemberAccess* ma = (AST_MemberAccess*)temp;
        analyze_node(ma->object);

        ClassEntry* target_class = NULL;
        Symbol* obj_sym = NULL;

        if (ma->object->type == AST_THIS) {
            obj_sym = lookup_symbol("this");
            if (!obj_sym || !obj_sym->class_name) {
                ERROR(ERR_SEM_CLASS_NOT_FOUND,
                      SEMLOC(ma->base.value),
                      "'this' used outside of class context");
                break;
            }
            target_class = find_class(obj_sym->class_name);
        } else if (ma->object->type == AST_MEMBER_ACCESS) {
            AST_MemberAccess* inner = (AST_MemberAccess*)ma->object;
            ClassEntry* inner_class = NULL;
            if (inner->object->type == AST_LITERAL) {
                AST_Literal* obj_lit = (AST_Literal*)inner->object;
                Symbol* sym = lookup_symbol(obj_lit->base.value.value);
                if (sym && sym->class_name) {
                    inner_class = find_class(sym->class_name);
                }
            } else if (inner->object->type == AST_THIS) {
                Symbol* this_sym = lookup_symbol("this");
                if (this_sym && this_sym->class_name) {
                    inner_class = find_class(this_sym->class_name);
                }
            }
            if (inner_class) {
                for (int i = 0; i < inner_class->member_count; i++) {
                    MemberInfo* mi = inner_class->members[i];
                    if (!mi->is_method && strcmp(mi->name, inner->member_name) == 0) {
                        if (mi->field_class_name) {
                            target_class = find_class(mi->field_class_name);
                        }
                        break;
                    }
                }
            }
            if (!target_class) {
                target_class = find_class(inner->member_name);
            }
            if (!target_class) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(ma->base.value),
                      "cannot resolve chain member access");
                break;
            }
        } else if (ma->object->type == AST_MEMBER_ACCESS) {
            AST_MemberAccess* inner = (AST_MemberAccess*)ma->object;
            ClassEntry* inner_class = NULL;
            if (inner->object->type == AST_LITERAL) {
                AST_Literal* obj_lit = (AST_Literal*)inner->object;
                Symbol* sym = lookup_symbol(obj_lit->base.value.value);
                if (sym && sym->class_name) {
                    inner_class = find_class(sym->class_name);
                }
            }
            if (inner_class) {
                for (int i = 0; i < inner_class->member_count; i++) {
                    MemberInfo* mi = inner_class->members[i];
                    if (!mi->is_method && strcmp(mi->name, inner->member_name) == 0) {
                        if (mi->field_class_name) {
                            target_class = find_class(mi->field_class_name);
                        }
                        break;
                    }
                }
            }
            if (!target_class) {
                ERROR(ERR_SEM_TYPE_MISMATCH,
                      SEMLOC(ma->base.value),
                      "cannot resolve chain member access");
                break;
            }
        } else if (ma->object->type == AST_LITERAL) {
            AST_Literal* obj_lit = (AST_Literal*)ma->object;
            if (strcmp(obj_lit->base.value.value, "this") == 0) {
                obj_sym = lookup_symbol("this");
                if (!obj_sym || !obj_sym->class_name) {
                    ERROR(ERR_SEM_CLASS_NOT_FOUND,
                          SEMLOC(ma->base.value),
                          "'this' used outside of class context");
                    break;
                }
                target_class = find_class(obj_sym->class_name);
            }
            else {
                DataType alias_type = resolve_type_alias(obj_lit->base.value.value);
                if (alias_type != DATA_ERROR) {
                    Symbol* member_sym = lookup_symbol(ma->member_name);
                    if (member_sym) {
                        ma->member_type = member_sym->type;
                        if (member_sym->internal_name) {
                            ma->member_offset = atoi(member_sym->internal_name);
                        } else {
                            ma->member_offset = 0;
                        }
                        ma->is_method = false;
                        break;
                    } else {
                        ERROR(ERR_SEM_FIELD_NOT_FOUND,
                              SEMLOC(ma->base.value),
                              "enum '%s' has no member '%s'",
                              obj_lit->base.value.value, ma->member_name);
                        break;
                    }
                }

                obj_sym = lookup_symbol(obj_lit->base.value.value);
                if (!obj_sym || !obj_sym->class_name) {
                    DataType alias_type = resolve_type_alias(obj_lit->base.value.value);
                    if (alias_type != DATA_ERROR) {
                        Symbol* member_sym = lookup_symbol(ma->member_name);
                        if (member_sym) {
                            ma->member_type = member_sym->type;
                            if (member_sym->internal_name) {
                                ma->member_offset = atoi(member_sym->internal_name);
                            } else {
                                ma->member_offset = 0;
                            }
                            ma->is_method = false;
                            break;
                        } else {
                            ERROR(ERR_SEM_FIELD_NOT_FOUND,
                                  SEMLOC(ma->base.value),
                                  "enum '%s' has no member '%s'",
                                  obj_lit->base.value.value, ma->member_name);
                            break;
                        }
                    }
                }
                if (!obj_sym || !obj_sym->class_name) {
                    ERROR(ERR_SEM_CLASS_NOT_FOUND,
                          SEMLOC(obj_lit->base.value),
                          "cannot determine class of object '%s'",
                          obj_lit->base.value.value);
                    break;
                }
                target_class = find_class(obj_sym->class_name);
            }
        }
        else {
            ERROR(ERR_SEM_TYPE_MISMATCH,
                  SEMLOC(ma->base.value),
                  "member access on complex expression not supported");
            break;
        }

        if (!target_class) {
            ERROR(ERR_SEM_CLASS_NOT_FOUND,
                  SEMLOC(ma->base.value),
                  "unknown class for member '%s'",
                  ma->member_name);
            break;
        }

        int found = 0;
        for (int i = 0; i < target_class->member_count; i++) {
            MemberInfo* mi = target_class->members[i];
            if (!mi->is_method && strcmp(mi->name, ma->member_name) == 0) {
                ma->member_type = mi->type;
                ma->member_offset = mi->offset;
                found = 1;

                if (mi->field_modifier == 2) {
                    ClassEntry* field_owner = target_class;
                    if (mi->inherited_from != NULL) {
                        field_owner = mi->inherited_from;
                    }
                    if (current_class != field_owner) {
                        char* gname = make_getter_name(ma->member_name);
                        char* sname = make_setter_name(ma->member_name);
                        ERROR(ERR_SEM_FIELD_NOT_FOUND,
                              SEMLOC(ma->base.value),
                              "cannot access private field '%s'",
                              ma->member_name);
                        free(gname);
                        free(sname);
                    }
                }
                else if (mi->field_modifier == 1) {
                    ClassEntry* field_owner = target_class;
                    if (mi->inherited_from != NULL) {
                        field_owner = mi->inherited_from;
                    }
                    if (!current_class || !is_subclass_of(current_class, field_owner)) {
                        ERROR(ERR_SEM_FIELD_NOT_FOUND,
                              SEMLOC(ma->base.value),
                              "cannot access protected field '%s'",
                              ma->member_name);
                    }
                }
                break;
            }
        }

        if (!found) {
            int method_count = 0;
            for (int i = 0; i < target_class->member_count; i++) {
                MemberInfo* mi = target_class->members[i];
                if (mi->is_method && strcmp(mi->name, ma->member_name) == 0) {
                    method_count++;
                    if (method_count == 1) {
                        ma->is_method = true;
                        if (mi->func && mi->func->Return) {
                            ma->member_type = mi->func->Return->returnType;
                        } else {
                            ma->member_type = DATA_VOID;
                        }
                        found = 1;
                    }
                }
            }

            if (found && ma->is_method) {
                ERROR(ERR_SEM_METHOD_NOT_FOUND,
                      SEMLOC(ma->base.value),
                      "method '%s' cannot be used as a value",
                      ma->member_name);
            }

            if (!found) {
                WARNING(ERR_SEM_FIELD_NOT_FOUND,
                        SEMLOC(ma->base.value),
                        "member '%s' not found in class",
                        ma->member_name);
                ma->member_type = DATA_INT;
                ma->member_offset = -1;
            }
        }
        break;
    }

    case AST_NEW_INSTANCE: {
        AST_NewInstance* ni = (AST_NewInstance*)temp;

        if (ni->arguments) {
            analyze_node(ni->arguments);
        }

        ClassEntry* cls = find_class(ni->class_name);
        if (!cls && current_class_name) {
            int len = strlen(current_class_name) + 1
                    + strlen(ni->class_name) + 1;
            char* full_name = malloc(len);
            snprintf(full_name, len, "%s_%s",
                     current_class_name, ni->class_name);
            cls = find_class(full_name);
            if (cls) {
                free(ni->class_name);
                ni->class_name = full_name;
            } else {
                free(full_name);
            }
        }

        if (!cls) {
            ERROR(ERR_SEM_CLASS_NOT_FOUND,
                  SEMLOC(ni->base.value),
                  "unknown class '%s'",
                  ni->class_name);
            ni->instance_size = 0;
            break;
        }

        ni->instance_size = cls->total_size;
        ni->field_init_count = 0;
        ni->field_inits = NULL;

        {
            int field_cnt = 0;
            for (int i = 0; i < cls->member_count; i++) {
                if (!cls->members[i]->is_method) field_cnt++;
            }
            if (field_cnt > 0) {
                ni->field_inits = calloc(field_cnt, sizeof(FieldInit));
                int idx = 0;
                for (int i = 0; i < cls->member_count; i++) {
                    MemberInfo* mi = cls->members[i];
                    if (mi->is_method) continue;
                    ni->field_inits[idx].offset = mi->offset;
                    ni->field_inits[idx].type = mi->type;
                    ni->field_inits[idx].value_str =
                        mi->init_value_str ? strdup(mi->init_value_str) : NULL;
                    idx++;
                }
                ni->field_init_count = field_cnt;
            }
        }

        if (cls->constructor) {
            ni->has_constructor = true;
            ni->constructor_name = strdup(cls->constructor->internal_name);
            int expected_params = cls->constructor->parameterCount;
            int expected_user_args = expected_params - 1;
            if (ni->constructor_arg_count != expected_user_args) {
                ERROR(ERR_SEM_ARG_COUNT_MISMATCH,
                      SEMLOC(ni->base.value),
                      "constructor expects %d argument(s), got %d",
                      expected_user_args, ni->constructor_arg_count);
            }
        } else {
            if (ni->constructor_arg_count > 0) {
                ERROR(ERR_SEM_METHOD_NOT_FOUND,
                      SEMLOC(ni->base.value),
                      "class '%s' has no constructor",
                      ni->class_name);
            }
            ni->has_constructor = false;
        }
        break;
    }

    case AST_MEMBER_ASS: {
        AST_MemberAss* mass = (AST_MemberAss*)temp;

        analyze_node(mass->object);

        ClassEntry* target_class = NULL;
        Symbol* obj_sym = NULL;

        if (mass->object->type == AST_THIS) {
            obj_sym = lookup_symbol("this");
            if (!obj_sym || !obj_sym->class_name) {
                ERROR(ERR_SEM_CLASS_NOT_FOUND,
                      SEMLOC(mass->base.value),
                      "'this' used outside of class context");
                break;
            }
            target_class = find_class(obj_sym->class_name);
        }
        else if (mass->object->type == AST_LITERAL) {
            AST_Literal* obj_lit = (AST_Literal*)mass->object;
            if (strcmp(obj_lit->base.value.value, "this") == 0) {
                obj_sym = lookup_symbol("this");
                if (obj_sym && obj_sym->class_name)
                    target_class = find_class(obj_sym->class_name);
            } else {
                obj_sym = lookup_symbol(obj_lit->base.value.value);
                if (!obj_sym || !obj_sym->class_name) {
                    DataType alias_type = resolve_type_alias(obj_lit->base.value.value);
                    if (alias_type != DATA_ERROR) {
                        ERROR(ERR_SEM_TYPE_MISMATCH,
                              SEMLOC(mass->base.value),
                              "cannot assign to enum member through member access");
                        break;
                    }
                }
                if (obj_sym && obj_sym->class_name)
                    target_class = find_class(obj_sym->class_name);
            }
        } else {
            ERROR(ERR_SEM_TYPE_MISMATCH,
                  SEMLOC(mass->base.value),
                  "left side of member assignment must be a simple object");
            break;
        }

        if (!target_class) {
            ERROR(ERR_SEM_CLASS_NOT_FOUND,
                  SEMLOC(mass->base.value),
                  "cannot determine class for member assign");
            break;
        }

        int found = 0;
        for (int i = 0; i < target_class->member_count; i++) {
            MemberInfo* mi = target_class->members[i];
            if (!mi->is_method && strcmp(mi->name, mass->member_name) == 0) {
                mass->member_type = mi->type;
                mass->member_offset = mi->offset;
                found = 1;

                if (mi->field_modifier == 2) {
                    ClassEntry* field_owner = target_class;
                    if (mi->inherited_from != NULL) {
                        field_owner = mi->inherited_from;
                    }
                    if (current_class != field_owner) {
                        ERROR(ERR_SEM_FIELD_NOT_FOUND,
                              SEMLOC(mass->base.value),
                              "cannot access private field '%s'",
                              mass->member_name);
                    }
                }
                else if (mi->field_modifier == 1) {
                    ClassEntry* field_owner = target_class;
                    if (mi->inherited_from != NULL) {
                        field_owner = mi->inherited_from;
                    }
                    if (!current_class || !is_subclass_of(current_class, field_owner)) {
                        ERROR(ERR_SEM_FIELD_NOT_FOUND,
                              SEMLOC(mass->base.value),
                              "cannot access protected field '%s'",
                              mass->member_name);
                    }
                }
                break;
            }
        }

        if (!found) {
            ERROR(ERR_SEM_FIELD_NOT_FOUND,
                  SEMLOC(mass->base.value),
                  "field '%s' not found in class '%s'",
                  mass->member_name, target_class->name);
            break;
        }

        analyze_node(mass->value);
        break;
    }

    case AST_ENUM_DEF: {
        AST_EnumDef* enum_def = (AST_EnumDef*)temp;

        if (enum_def->name != NULL) {
            Symbol* existing = lookup_symbol(enum_def->name);
            if (existing != NULL) {
                ERROR(ERR_SEM_DUPLICATE_VAR,
                      SEMLOC(enum_def->base.value),
                      "enum name '%s' conflicts with existing symbol",
                      enum_def->name);
            }
            add_type_alias(enum_def->name, DATA_INT);
        }

        int current_value = 0;
        for (int i = 0; i < enum_def->member_count; i++) {
            AST_EnumMember* member = enum_def->members[i];

            if (member->has_explicit_value) {
                current_value = member->value;
            } else {
                member->value = current_value;
            }

            Symbol* existing = lookup_symbol(member->name);
            if (existing != NULL) {
                ERROR(ERR_SEM_DUPLICATE_VAR,
                      SEMLOC(member->base.value),
                      "duplicate enum member '%s'",
                      member->name);
                current_value++;
                continue;
            }

            char value_str[32];
            snprintf(value_str, sizeof(value_str), "%d", member->value);

            Symbol* sym = create_symbol(
                (Token){.value = member->name, .type = TOKEN_IDENTIFIER},
                DATA_INT,
                member->name,
                value_str
            );
            insert_symbol(sym);

            current_value = member->value + 1;
        }
        break;
    }

    case AST_ENUM_MEMBER:
        break;

    case AST_THIS: {
        AST_This* tn = (AST_This*)temp;
        if (!current_class) {
            ERROR(ERR_SEM_CLASS_NOT_FOUND,
                  SEMLOC(tn->base.value),
                  "'this' used outside of class context");
            tn->instance_size = 0;
        } else {
            tn->instance_size = current_class->total_size;
        }
        break;
    }

    case AST_RETURN: {
        AST_Return* return_node = (AST_Return*)temp;

        analyze_node(return_node->value);

            if (current_analyzing_function &&
                current_analyzing_function->returns_array)
        {
            if (return_node->value == NULL)
            {
                ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                      SEMLOC(return_node->base.value),
                      "array-returning function must return an array value");
            }
            else
            {
                ASTNode* val = return_node->value;
                DataType expr_type = get_expression_type(val);
                if (val->type == AST_LITERAL)
                {
                    AST_Literal* lit = (AST_Literal*)val;
                    Symbol* sym = lookup_symbol(lit->base.value.value);
                    if (sym && sym->dim_count > 0)
                    {
                        if (sym->dim_count != current_analyzing_function->return_dim_count)
                        {
                            ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                                  SEMLOC(return_node->base.value),
                                  "return array dimension mismatch: "
                                  "expected %d, got %d",
                                  current_analyzing_function->return_dim_count,
                                  sym->dim_count);
                        }

                        if (!types_compatible(
                                current_analyzing_function->return_element_type,
                                sym->type))
                        {
                            ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                                  SEMLOC(return_node->base.value),
                                  "return array element type mismatch: "
                                  "expected '%s', got '%s'",
                                  data_type_to_string(
                                      current_analyzing_function->return_element_type),
                                  data_type_to_string(sym->type));
                        }
                    }
                }
                else if (val->type == AST_ARRAY_ACCESS)
                {
                    AST_ArrayAccess* acc = (AST_ArrayAccess*)val;
                    if (!types_compatible(
                            current_analyzing_function->return_element_type,
                            acc->elementType))
                    {
                        ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                              SEMLOC(return_node->base.value),
                              "return array element type mismatch: "
                              "expected '%s', got '%s'",
                              data_type_to_string(
                                  current_analyzing_function->return_element_type),
                              data_type_to_string(acc->elementType));
                    }
                }
                else
                {
                    ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                          SEMLOC(return_node->base.value),
                          "array-returning function requires an array expression");
                }
            }
            break;
        }

        if (return_node->returnType == DATA_VOID) {
            if (return_node->value != NULL) {
                ERROR(ERR_SEM_VOID_VALUE,
                      SEMLOC(return_node->base.value),
                      "void function should not return a value");
            }
        } else {
            if (return_node->value == NULL) {
                ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                      SEMLOC(return_node->base.value),
                      "non-void function must return a value");
            } else {
                DataType ret_expr_type = get_expression_type(return_node->value);

                if (!types_compatible(return_node->returnType, ret_expr_type)
                    && ret_expr_type != DATA_ERROR) {
                    ERROR(ERR_SEM_RETURN_TYPE_MISMATCH,
                          SEMLOC(return_node->base.value),
                          "return type mismatch: expected '%s', got '%s'",
                          data_type_to_string(return_node->returnType),
                          data_type_to_string(ret_expr_type));
                }
            }
        }
        break;
    }

    case AST_ERROR:
        break;

    default:
        break;
    }
}

void main_parse(ASTNode* ast_node) {
    for (ASTNode* node = ast_node; node != NULL; node = node->next) {
        if (node->type == AST_CLASS_DEF) {
            AST_ClassDef* cd = (AST_ClassDef*)node;

            ClassEntry* cls = find_class(cd->name);
            if (cls == NULL) {
                cls = calloc(1, sizeof(ClassEntry));
                cls->name = strdup(cd->name);
                cls->parent_count = cd->parent_count;
                cls->parent_names = malloc(sizeof(char*) * cd->parent_count);
                for (int i = 0; i < cd->parent_count; i++)
                    cls->parent_names[i] = strdup(cd->parent_names[i]);
                cls->modifier = cd->modifier;
                add_class(cls);
            }

            for (int i = 0; i < cd->parent_count; i++) {
                ClassEntry* parent = find_class(cd->parent_names[i]);
                if (parent && !parent->members_resolved) {
                    ERROR(ERR_SEM_CLASS_NOT_FOUND,
                          SEMLOC(cd->base.value),
                          "parent class '%s' must be defined before '%s'",
                          cd->parent_names[i], cd->name);
                }
            }

            resolve_class_members(cd, cls);
        }
        else if (node->type == AST_ENUM_DEF) {
            analyze_node(node);
        }
    }

    ASTNode* temp = ast_node;
    while (temp != NULL) {
        switch (temp->type) {
        case AST_CLASS_DEF: {
            AST_ClassDef* cd = (AST_ClassDef*)temp;
            ClassEntry* cls = find_class(cd->name);
            if (cls && cls->members_resolved) {
                analyze_class_bodies(cd, cls);
            }
            break;
        }

        case AST_ENUM_DEF:
            break;

        default:
            analyze_node(temp);
            break;
        }
        temp = temp->next;
    }
}

void set_current_semantic_file(const char* filename) {
    current_semantic_file = filename;
    semantic_filename = filename;
}

void Semantics(ASTNode* ast_node) {
    if (ast_node == NULL) {
        INTERNAL_ERROR("semantics: NULL AST node");
    }

    semantic_filename = current_semantic_file;

    bool created_global = false;
    if (current_scope == NULL) {
        enter_scope();
        created_global = true;
    }

    predeclare_all_types(ast_node);

    main_parse(ast_node);

    if (!has_main_function()) {
        ERROR(ERR_SEM_NO_MAIN,
              LOC_NONE,
              "no main function found");
    }

    if (created_global) {
        exit_scope();
    }

    free_function_table();
}

static void cleanup_all_scopes(void)
{
    while (current_scope != NULL) {
        if (current_scope->table) {
            for (int i = 0; i < current_scope->table->size; i++) {
                Symbol* sym = current_scope->table->buckets[i];
                while (sym) {
                    Symbol* next = sym->next;
                    free_symbol(sym);
                    sym = next;
                }
            }
            free(current_scope->table->buckets);
            free(current_scope->table);
            current_scope->table = NULL;
        }
        Scope* parent = current_scope->parent;
        free(current_scope);
        current_scope = parent;
    }
}

static void cleanup_class_table(void)
{
    ClassEntry* cls = class_table;
    while (cls) {
        ClassEntry* next = cls->next;
        if (cls->name) free(cls->name);
        if (cls->parent_names) {
            for (int i = 0; i < cls->parent_count; i++) {
                free(cls->parent_names[i]);
            }
            free(cls->parent_names);
        }
        if (cls->members) {
            for (int i = 0; i < cls->member_count; i++) {
                if (cls->members[i]) {
                    if (cls->members[i]->name) free(cls->members[i]->name);
                    if (cls->members[i]->init_value_str) free(cls->members[i]->init_value_str);
                    if (cls->members[i]->field_class_name) free(cls->members[i]->field_class_name);
                    free(cls->members[i]);
                }
            }
            free(cls->members);
        }
        free(cls);
        cls = next;
    }
    class_table = NULL;
}

static void cleanup_type_aliases(void)
{
    TypeAlias* a = type_aliases;
    while (a) {
        TypeAlias* next = a->next;
        if (a->name) free(a->name);
        free(a);
        a = next;
    }
    type_aliases = NULL;
}

void semantics_cleanup(void)
{
    cleanup_all_scopes();
    cleanup_class_table();
    cleanup_type_aliases();
    free_function_table();
}