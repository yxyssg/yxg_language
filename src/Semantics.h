#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "AST.h"
#include <stdbool.h>

typedef struct SYMBOL
{
    struct SYMBOL* next;
    Token value;
    DataType type;
    char* key;
    char* internal_name;
    char* class_name;
    int dim_count;
    int* dim_sizes;
    bool is_field;
    int  field_offset;
    bool is_function;
    int  field_modifier;
} Symbol;

typedef struct SYMBOL_TABLE
{
    Symbol** buckets;
    int count;
    int size;
} Symbol_Table;

typedef struct SCOPE
{
    Symbol_Table* table;
    struct SCOPE* parent;
    int depth;
} Scope;

typedef struct FuncEntry {
    char* name;
    AST_Function* func;
    int param_count;
    DataType* param_types;
    struct FuncEntry* next;
} FuncEntry;

typedef struct MemberInfo {
    char* name;
    DataType type;
    bool is_method;
    AST_Function* func;
    int offset;
    struct ClassEntry* inherited_from;
    int field_modifier;
    ASTNode* init_expr;
    char* init_value_str;
    char* field_class_name;
} MemberInfo;

typedef struct ClassEntry {
    char* name;
    char** parent_names;
    int    parent_count;
    int modifier;
    MemberInfo** members;
    int member_count;
    int member_capacity;
    int total_size;
    struct ClassEntry* next;
    AST_Function* constructor;
    bool members_resolved;
} ClassEntry;

typedef struct TypeAlias {
    char* name;
    DataType actual_type;
    struct TypeAlias* next;
} TypeAlias;

extern Scope* current_scope;
extern bool main_has_args;

void Semantics(ASTNode* ast_node);
void set_current_semantic_file(const char* filename);
void main_parse(ASTNode* ast_node);
ClassEntry* find_class(const char* name);
void add_class(ClassEntry* entry);
char* data_type_to_string(DataType type);
DataType Get_Datatype(TokenType type);
static int S_get_type_size(DataType type);
void enter_scope(void);
void exit_scope(void);
Symbol* lookup_symbol(const char* name);
void insert_symbol(Symbol* sym);
static DataType get_expression_type(ASTNode* node);
static bool is_subclass_of(ClassEntry* class_entry, ClassEntry* parent_entry);
char* generate_internal_name(void);
ASTNode* parser_create_literal(Token tok);
AST_Function* find_function_by_signature(const char* name, int arg_count, DataType* arg_types);
bool function_signature_exists(const char* name, int param_count, DataType* param_types);
static void predeclare_all_types(ASTNode* node);
void semantics_cleanup(void);

#endif