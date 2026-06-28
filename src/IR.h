#ifndef IR_H
#define IR_H

#include "AST.h"

extern int temp_i;

typedef enum IRTYPE
{
    IR_LITERAL,
    IR_BINARY,
    IR_UNARY,
    IR_PRINT,
    IR_VAR,
    IR_VAR_ASS,
    IR_VAR_UNINIT,
    IR_IF,
    IR_LABEL,
    IR_JMP,
    IR_JMPZ,
    IR_JMPNZ,
    IR_PARAM_DEF,
    IR_PUSH,
    IR_CALL,
    IR_RET,
    IR_ARRAY_DECL,
    IR_ARRAY_LOAD,
    IR_ARRAY_STORE,
    IR_ALLOC_ARRAY,
    IR_ARRAY_LENGTH,
    IR_ARRAY_TOSTRING,
    IR_BITWISE_NOT,
    IR_FORMAT,
    IR_INPUT,
    IR_ALLOC_CLASS,
    IR_LOAD_MEMBER,
    IR_STORE_MEMBER,
    IR_LOAD_THIS,

    IR_PHI,

    IR_BOUNDS_CHECK,

    IR_ERROR,
}IRType;

typedef enum IROPTYPE
{
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_MOD,
    IR_JE,
    IR_JNE,
    IR_JG,
    IR_JL,
    IR_JGE,
    IR_JLE,
    IR_UDIV,
    IR_UMOD,
    IR_ULT,
    IR_ULE,
    IR_UGT,
    IR_UGE,
    IR_LOGICAL_AND,
    IR_LOGICAL_OR,
    IR_BITWISE_AND,
    IR_BITWISE_OR,
    IR_BITWISE_XOR,
    IR_SHL,
    IR_SHR,
    IR_SAR,

    IR_OP_ERROR,
}IR_OP_Type;

typedef struct IR
{
    IRType type;
    char* result;
    int   def_id;
    IR_OP_Type op;
    char* args1;
    int   args1_def;
    DataType args1_type;
    char* args2;
    int   args2_def;
    DataType args2_type;

    int label_count;
    char** label;

    DataType data_type;

    DataType* call_func_args_type;
    int call_func_args_num;
    struct IR* call_function_Parameter;
    struct IR* next;
    bool has_return;

    int func_temp_count;
    int func_local_count;

    int array_size;
    int elem_size;
    int dim_count;
    int* dim_sizes;

    int func_stack_size;
    int func_local_area_base;

    int     phi_arg_count;
    char**  phi_arg_names;
}IR;


typedef struct {
    char* break_label;
    char* continue_label;
} LoopContext;

IR* generate_IR(ASTNode* ast_node);
IR* start_generate_IR(ASTNode* ast_node);
IR* parse_IR(ASTNode* ast_node);
void print_IR(IR* ir);
IR* create_IR(IRType ir_type);
int get_type_size(DataType type);
void free_IR_chain(IR* ir);
int  assign_def_id(IR* ir);
int  get_def_id_by_name(const char* name, IR* head, IR* stop_before);
#endif