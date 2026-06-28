#ifndef PARSER_H
#define PARSER_H

#include "AST.h"
#include "Lexer.h"

extern char* read_source(char* file_name);

typedef struct Parser
{
    Token* tokens;
    int index;
    Token current;
    char* filename;
    Token previous;
}Parser;

typedef struct ModulePath {
    char* path;
    struct ModulePath* next;
} ModulePath;

Parser* parser_init(Token* tokens,const char* filename);
ASTNode* parser_Sentence(Parser* parser,bool is_block);
ASTNode* parser_parse(Parser* parser);
static ASTNode* parser_member_assign(Parser* parser, ASTNode* left);
ASTNode* parser_class_def(Parser* parser,int modifier);
ASTNode* parser_constructor_def(Parser* parser, int modifier);
ASTNode* parser_enum_def(Parser* parser);
ASTNode* parser_return(Parser* parser);
ASTNode* parser_class_def_with_modifier(Parser* parser, int modifier);
ASTNode* parser_def_function_with_modifier(Parser* parser, int modifier);
ASTNode* parser_var_with_typename(Parser* parser);
ASTNode* parser_def_function(Parser* parser);
ASTNode* parser_while(Parser* parser);
ASTNode* parser_do_while(Parser* parser);
ASTNode* parser_for(Parser* parser);
ASTNode* parser_switch(Parser* parser);
ASTNode* parser_break(Parser* parser);
ASTNode* parser_continue(Parser* parser);
ASTNode* parser_if(Parser* parser);
ASTNode* parser_def_var(Parser* parser);
ASTNode* parser_var(Parser* parser);
ASTNode* parser_input(Parser* parser);
ASTNode* parser_Binary(Parser* parser);
ASTNode* parser_Unary(Parser* parser);
ASTNode* parser_init_list(Parser* parser);
ASTNode* parser_import(Parser* parser);
static ASTNode* parser_compound_assign_or_incdec(Parser* parser);
static ASTNode* parser_ArrayAccessSuffix(Parser* parser, ASTNode* left);
int* parse_dim_sizes(Parser* parser, int* count, DataType elemType);
ASTNode* parser_Print(Parser* parser);
void Print_AST(ASTNode* node, int indent,bool after_sem);
void AST_Free(ASTNode* node);
bool is_Modifier(Parser* parser);
char* AST_Type_to_String(ASTType type);
void module_path_add(const char* path);
void module_path_add_defaults(const char* exe_dir);
bool is_already_imported(const char* abs_path);
char* realpath_wrapper(const char* path);
void mark_imported(const char* abs_path);
void module_system_cleanup(void);
void AST_Free(ASTNode* node);

#endif //PARSER_H
