#ifndef AST_H
#define AST_H

#include "Token.h"

typedef enum ASTTYPE
{
    AST_LITERAL,
    AST_BINARY,
    AST_UNARY,
    AST_CONDITIONAL,
    AST_VAR,
    AST_UNINIT_VAR,
    AST_VAR_ASS,
    AST_PRINT,
    AST_INPUT,
    AST_IF,
    AST_SWITCH,
    AST_CASE,
    AST_BREAK,
    AST_CONTINUE,
    AST_WHILE,
    AST_FOR,
    AST_FOREACH,
    AST_FUNCTION,
    AST_RETURN,
    AST_PARAM,
    AST_ARRAY_DECL,
    AST_ARRAY_ACCESS,
    AST_ARRAY_ASS,
    AST_NEW_ARRAY,
    AST_INIT_LIST,
    AST_ARRAY_LENGTH,
    AST_SUBARRAY,
    AST_CLASS_DEF,
    AST_NEW_INSTANCE,
    AST_MEMBER_ACCESS,
    AST_THIS,
    AST_CLASS_MEMBER,
    AST_MEMBER_ASS,
    AST_ENUM_DEF,
    AST_ENUM_MEMBER,

    AST_ERROR,
}ASTType;

typedef enum DATATYPE
{
    DATA_INT = 0,
    DATA_FLOAT,
    DATA_CHAR,
    DATA_STRING,
    DATA_BOOL,
    DATA_VOID,
    DATA_UINT,
    DATA_UFLOAT,
    DATA_IDENTIFIER,

    DATA_ERROR,
}DataType;

typedef struct ASTNODE
{
    ASTType type;
    Token value;
    struct ASTNODE* next;
}ASTNode;

typedef struct AST_LITERAL
{
    ASTNode base;
    DataType dataType;
    char* internal_name;

    bool is_call_function;
    ASTNode* call_function_Parameter;
    int call_function_ParameterCount;

    ASTNode* method_arguments;
    int method_arg_count;

    bool is_field_ref;
    int  field_offset;

    bool returns_array;
    int  return_total_bytes;
}AST_Literal;

typedef struct AST_BINARY
{
    ASTNode base;
    ASTNode* left;
    Token op;
    ASTNode* right;
    DataType resultType;
    bool is_unsigned;
}AST_Binary;

typedef struct AST_ARRAY_DECL {
    ASTNode base;
    DataType elementType;
    char* name;
    int* dim_sizes;
    int dim_count;
    char* internal_name;
    ASTNode* init_list;
    int modifier;
} AST_ArrayDecl;

typedef struct AST_ARRAY_ACCESS {
    ASTNode base;
    ASTNode* array;
    ASTNode** indices;
    int index_count;
    DataType elementType;
    int dim_count;
    int* dim_sizes;
    int* full_dim_sizes;
    int full_dim_count;
} AST_ArrayAccess;

typedef struct AST_ARRAY_ASS {
    ASTNode base;
    ASTNode* target;
    ASTNode* value;
    DataType elementType;
} AST_ArrayAss;

typedef struct AST_NEW_ARRAY {
    ASTNode base;
    DataType elementType;
    int dim_count;
    int* dim_sizes;
    ASTNode** dim_exprs;
} AST_NewArray;

typedef struct AST_INIT_LIST {
    ASTNode base;
    ASTNode** elements;
    int count;
    DataType elemType;
    int dim_count;
    int* dim_sizes;
} AST_InitList;

typedef struct AST_ARRAY_LENGTH {
    ASTNode base;
    ASTNode* array;
} AST_ArrayLength;

typedef struct AST_SUBARRAY {
    ASTNode base;
    ASTNode* array;
    int index_count;
    DataType elementType;
    int dim_count;
    int* dim_sizes;
    char* base_temp;
} AST_SubArray;

typedef struct AST_CONDITIONAL {
    ASTNode base;
    ASTNode* condition;
    ASTNode* then_expr;
    ASTNode* else_expr;
    DataType resultType;
} AST_Conditional;

typedef struct AST_UNARY
{
    ASTNode base;
    Token op;
    ASTNode* right;
    DataType dataType;
}AST_Unary;

typedef struct AST_VAR
{
    ASTNode base;
    Token name;
    ASTNode* Value;

    TokenType type;
    DataType dataType;
    char* internal_name;

    int dim_count;
    int* dim_sizes;

    int modifier;
    bool is_field_ref;
    int  field_offset;
}AST_Var;

typedef struct AST_PRINT
{
    ASTNode base;
    ASTNode* Parameter;
}AST_Print;

typedef struct AST_INPUT {
    ASTNode base;
    ASTNode* format_string;
    ASTNode** args;
    int arg_count;
} AST_Input;

typedef struct AST_IF
{
    ASTNode base;
    ASTNode* Condition;
    ASTNode* code;
    int elif_Count;
    ASTNode** Elif;
    ASTNode** elif_condition;
    int has_else;
    ASTNode* Else;
}AST_If;

typedef struct AST_SWITCH_NODE {
    ASTNode base;
    ASTNode* condition;
    int case_count;
    int case_capacity;
    struct AST_CASE_NODE** cases;
    bool has_default;
    ASTNode* default_body;
} AST_Switch;

typedef struct AST_CASE_NODE {
    ASTNode base;
    ASTNode* value;
    ASTNode* body;
    bool is_default;
} AST_Case;

typedef struct AST_BREAK_NODE {
    ASTNode base;
} AST_Break;

typedef struct AST_CONTINUE_NODE {
    ASTNode base;
} AST_Continue;

typedef struct AST_WHILE
{
    ASTNode base;
    ASTNode* Condition;
    ASTNode* code;
    bool has_do;
}AST_While;

typedef struct AST_FOR {
    ASTNode base;
    ASTNode* init;
    ASTNode* condition;
    ASTNode* increment;
    ASTNode* code;
} AST_For;

typedef struct AST_FOREACH {
    ASTNode base;
    Token iterator;
    ASTNode* array;
    ASTNode* code;
    char* internal_iterator;
    DataType elemType;
    bool has_explicit_type;
} AST_ForEach;

typedef struct AST_RETURN
{
    ASTNode base;
    DataType returnType;
    ASTNode* value;
}AST_Return;

typedef struct AST_FUNCTION
{
    ASTNode base;
    Token name;
    char* internal_name;
    ASTNode* Parameter;
    DataType* parameterType;
    int parameterCount;
    int max_parameters;

    ASTNode* Code;

    AST_Return* Return;
    int return_Count;
    bool    returns_array;
    DataType return_element_type;
    int     return_dim_count;
    int*    return_dim_sizes;
    int Modifier; //0:pub,1:pro,2:pri
}AST_Function;

typedef struct AST_CLASS_DEF {
    ASTNode base;
    char* name;
    char* parent_name;
    char** parent_names;
    int    parent_count;
    int modifier;
    ASTNode* members;
} AST_ClassDef;


typedef struct FieldInit {
    int offset;
    DataType type;
    char* value_str;
} FieldInit;

typedef struct AST_NEW_INSTANCE {
    ASTNode base;
    char* class_name;
    ASTNode* arguments;
    int instance_size;
    bool has_constructor;
    char* constructor_name;
    int  constructor_arg_count;
    int field_init_count;
    FieldInit* field_inits;
} AST_NewInstance;

typedef struct AST_MEMBER_ACCESS {
    ASTNode base;
    ASTNode* object;
    char* member_name;
    int member_offset;
    DataType member_type;
    bool is_method;
} AST_MemberAccess;

typedef struct AST_THIS {
    ASTNode base;
    int instance_size;
} AST_This;

typedef struct AST_MEMBER_ASS {
    ASTNode base;
    ASTNode* object;
    char* member_name;
    Token op;
    ASTNode* value;
    DataType member_type;
    int member_offset;
} AST_MemberAss;

typedef struct AST_ENUM_MEMBER {
    ASTNode base;
    char* name;
    int value;
    bool has_explicit_value;
    Token* explicit_token;
} AST_EnumMember;

typedef struct AST_ENUM_DEF {
    ASTNode base;
    char* name;
    int member_count;
    int member_capacity;
    AST_EnumMember** members;
    int modifier;
} AST_EnumDef;

#endif
