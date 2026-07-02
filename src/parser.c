#include "parser.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "Semantics.h"
#include "Error.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <stdlib.h>
    #define PATH_SEPARATOR '\\'
    #define MAX_PATH 260
#else
    #include <unistd.h>
    #include <limits.h>
    #define PATH_SEPARATOR '/'
    #define MAX_PATH PATH_MAX
#endif

ModulePath* module_paths = NULL;
#define IMPORTED_CAPACITY 128
char** imported_modules = NULL;
int imported_count = 0;
int imported_capacity = 0;
static const char* parser_filename = NULL;

void module_path_add(const char* path) {
    ModulePath* mp = malloc(sizeof(ModulePath));
    mp->path = strdup(path);
    mp->next = module_paths;
    module_paths = mp;
}

void module_path_add_defaults(const char* exe_dir) {
    module_path_add(".");
    if (exe_dir) {
        int len = strlen(exe_dir) + 5 + 1;
        char* lib_path = malloc(len);
        snprintf(lib_path, len, "%s%clib", exe_dir, PATH_SEPARATOR);
        module_path_add(lib_path);
        free(lib_path);
    }
    const char* env = getenv("YXG_PATH");
    if (env) {
        char* env_copy = strdup(env);
        char* token = strtok(env_copy,
            #ifdef _WIN32
                ";"
            #else
                ":"
            #endif
        );
        while (token) {
            module_path_add(token);
            token = strtok(NULL,
                #ifdef _WIN32
                    ";"
                #else
                    ":"
                #endif
            );
        }
        free(env_copy);
    }
}

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

char* resolve_module(const char* module_name) {
    if (strchr(module_name, '/') || strchr(module_name, '\\') ||
        strstr(module_name, ".yxg")) {
        if (file_exists(module_name))
            return realpath_wrapper(module_name);
        for (ModulePath* mp = module_paths; mp; mp = mp->next) {
            int len = strlen(mp->path) + 1 + strlen(module_name) + 1;
            char* full = malloc(len);
            snprintf(full, len, "%s%c%s", mp->path, PATH_SEPARATOR, module_name);
            if (file_exists(full)) {
                char* result = realpath_wrapper(full);
                free(full);
                return result;
            }
            free(full);
        }
        return NULL;
    }
    char* path_form = strdup(module_name);
    for (char* p = path_form; *p; p++) {
        if (*p == '.') *p = PATH_SEPARATOR;
    }
    int len = strlen(path_form) + 5;
    char* with_ext = malloc(len);
    snprintf(with_ext, len, "%s.yxg", path_form);
    free(path_form);
    for (ModulePath* mp = module_paths; mp; mp = mp->next) {
        len = strlen(mp->path) + 1 + strlen(with_ext) + 1;
        char* full = malloc(len);
        snprintf(full, len, "%s%c%s", mp->path, PATH_SEPARATOR, with_ext);
        if (file_exists(full)) {
            free(with_ext);
            char* result = realpath_wrapper(full);
            free(full);
            return result;
        }
        free(full);
    }
    free(with_ext);
    return NULL;
}

char* realpath_wrapper(const char* path) {
    char* resolved = malloc(MAX_PATH);
    if (!resolved) return NULL;
#ifdef _WIN32
    if (_fullpath(resolved, path, MAX_PATH) == NULL) {
        free(resolved);
        return NULL;
    }
#else
    if (realpath(path, resolved) == NULL) {
        free(resolved);
        return NULL;
    }
#endif
    return resolved;
}

bool is_already_imported(const char* abs_path) {
    for (int i = 0; i < imported_count; i++) {
        if (strcmp(imported_modules[i], abs_path) == 0)
            return true;
    }
    return false;
}

void mark_imported(const char* abs_path) {
    if (imported_count >= imported_capacity) {
        imported_capacity = (imported_capacity == 0) ? IMPORTED_CAPACITY : imported_capacity * 2;
        imported_modules = realloc(imported_modules, sizeof(char*) * imported_capacity);
    }
    imported_modules[imported_count++] = strdup(abs_path);
}

void module_system_cleanup(void) {
    ModulePath* mp = module_paths;
    while (mp) {
        ModulePath* next = mp->next;
        free(mp->path);
        free(mp);
        mp = next;
    }
    module_paths = NULL;
    for (int i = 0; i < imported_count; i++)
        free(imported_modules[i]);
    free(imported_modules);
    imported_modules = NULL;
    imported_count = 0;
    imported_capacity = 0;
}

Parser* parser_init(Token* tokens, const char* filename)
{
    if (tokens == NULL)
    {
        INTERNAL_ERROR("parser_init: tokens is NULL");
    }
    Parser* parser = malloc(sizeof(Parser));
    if (parser == NULL)
    {
        INTERNAL_ERROR("parser_init: parser allocation failed");
    }
    parser->tokens = tokens;
    parser->index = 0;
    parser->current = parser->tokens[parser->index];
    parser->previous = parser->tokens[0];
    parser->filename = filename;
    parser_filename = filename;
    return parser;
}

void parser_advance(Parser* parser)
{
    if (parser->current.type == TOKEN_EOF)
    {
        Token prev = parser->previous;
        int err_line   = prev.line;
        int err_column = prev.column + strlen(prev.value);
        ERROR(ERR_PARSE_UNEXPECTED_EOF,
              LOC(parser->filename, err_line, err_column),
              "unexpected end of file");
        return;
    }
    parser->previous = parser->current;
    parser->current = parser->tokens[++parser->index];
}

bool parser_current_is_type(Parser* parser, TokenType type)
{
    return parser->current.type == type;
}

void parser_eat(Parser* parser, TokenType expected)
{
    if (!parser_current_is_type(parser, expected))
    {
        int err_line   = parser->previous.line;
        int err_column = parser->previous.column + strlen(parser->previous.value);
        ERROR(ERR_PARSE_EXPECTED_TOKEN,
              LOC(parser->filename, err_line, err_column),
              "expected '%s' before '%s'",
              type_to_string(expected),
              type_to_string(parser->current.type));
    }
    if (!parser_current_is_type(parser, TOKEN_EOF))
    {
        parser_advance(parser);
    }
}

ASTNode* parser_parse(Parser* parser)
{
    if (parser == NULL)
    {
        INTERNAL_ERROR("parser_parse: parser is NULL");
    }
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    while (!parser_current_is_type(parser, TOKEN_EOF))
    {
        ASTNode* stmt = parser_Sentence(parser, false);
        if (stmt == NULL)
        {
            while (!parser_current_is_type(parser, TOKEN_SEMICOLON) &&
                   !parser_current_is_type(parser, TOKEN_EOF) &&
                   !is_Modifier(parser))
            {
                parser_advance(parser);
            }
            if (parser_current_is_type(parser, TOKEN_SEMICOLON))
                parser_advance(parser);
            continue;
        }

        if (head == NULL) {
            head = stmt;
            tail = stmt;
        } else {
            tail->next = stmt;
            tail = stmt;
        }

        while (tail->next != NULL) {
            tail = tail->next;
        }
    }
    return head;
}

ASTNode* parser_parse_code_block(Parser* parser)
{
    if (parser == NULL)
    {
        INTERNAL_ERROR("parser_parse_code_block: parser is NULL");
    }
    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    while (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
           !parser_current_is_type(parser, TOKEN_EOF))
    {
        ASTNode* stmt = parser_Sentence(parser, true);
        if (stmt == NULL)
        {
            while (!parser_current_is_type(parser, TOKEN_SEMICOLON) &&
                   !parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
                   !parser_current_is_type(parser, TOKEN_EOF))
            {
                parser_advance(parser);
            }
            if (parser_current_is_type(parser, TOKEN_SEMICOLON))
                parser_advance(parser);
            continue;
        }

        if (head == NULL) {
            head = stmt;
            tail = stmt;
        } else {
            tail->next = stmt;
            tail = stmt;
        }

        while (tail->next != NULL) {
            tail = tail->next;
        }
    }
    return head;
}

bool is_var_type(TokenType type)
{
    return type == TOKEN_INT || type == TOKEN_FLOAT || type == TOKEN_STRING ||
        type == TOKEN_CHAR || type == TOKEN_BOOL || type == TOKEN_VOID || type == TOKEN_UINT ||
            type == TOKEN_UFLOAT;
}

bool is_Modifier(Parser* parser)
{
    return parser->current.type == TOKEN_PUB || parser->current.type == TOKEN_PRO || parser->current.type == TOKEN_PRI;
}

ASTNode* parser_create_literal(Token tok) {
    AST_Literal* lit = malloc(sizeof(AST_Literal));
    lit->base.type = AST_LITERAL;
    lit->base.next = NULL;
    lit->base.value = tok;
    lit->dataType = DATA_IDENTIFIER;
    lit->internal_name = NULL;
    lit->is_call_function = false;
    lit->call_function_ParameterCount = 0;
    lit->method_arguments = NULL;
    lit->method_arg_count = 0;
    lit->is_field_ref = false;
    lit->field_offset = 0;
    return (ASTNode*)lit;
}

ASTNode* parser_Sentence(Parser* parser, bool is_block)
{
    ASTNode* temp = NULL;
    int modifier = 0;
    if (is_Modifier(parser)) {
        if (parser->current.type == TOKEN_PUB)      modifier = 0;
        else if (parser->current.type == TOKEN_PRO) modifier = 1;
        else                                        modifier = 2;
        parser_advance(parser);

        if (parser->current.type == TOKEN_CLASS) {
            return parser_class_def(parser, modifier);
        }
        if (parser->current.type == TOKEN_ENUM) {
            return parser_enum_def(parser);
        }

        if (is_var_type(parser->current.type)) {
            TokenType next = parser->tokens[parser->index + 1].type;
            TokenType next2 = parser->tokens[parser->index + 2].type;
            if (next == TOKEN_IDENTIFIER && next2 == TOKEN_PAREN_LEFT) {
                return parser_def_function_with_modifier(parser, modifier);
            } else {
                temp = parser_var(parser,true);
                if (temp) {
                    if (temp->type == AST_ARRAY_DECL) {
                        ((AST_ArrayDecl*)temp)->modifier = modifier;
                    } else if (temp->type == AST_VAR || temp->type == AST_UNINIT_VAR) {
                        ((AST_Var*)temp)->modifier = modifier;
                    }
                }
                parser_eat(parser, TOKEN_SEMICOLON);
                return temp;
            }
        }

        if (parser->current.type == TOKEN_IDENTIFIER) {
            TokenType next = parser->tokens[parser->index + 1].type;
            if (next == TOKEN_PAREN_LEFT) {
                return parser_constructor_def(parser, modifier);
            }

            if (next == TOKEN_IDENTIFIER) {
                temp = parser_var_with_typename(parser);
                if (temp) {
                    if (temp->type == AST_VAR || temp->type == AST_UNINIT_VAR) {
                        ((AST_Var*)temp)->modifier = modifier;
                    }
                }
                parser_eat(parser, TOKEN_SEMICOLON);
                return temp;
            }
        }

        ERROR(ERR_PARSE_UNEXPECTED_TOKEN,
              LOC(parser->filename, parser->current.line, parser->current.column),
              "unexpected token '%s' after modifier",
              type_to_string(parser->current.type));
        return NULL;
    }

    if (parser->current.type == TOKEN_PRINT)
    {
        temp = parser_Print(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    } else if (parser->current.type == TOKEN_INPUT) {
        temp = parser_input(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    } else if (is_var_type(parser->current.type))
    {
        int lookahead = parser->index + 1;
        while (parser->tokens[lookahead].type == TOKEN_SQUARE_BRACKET_LEFT) {
            lookahead++;
            if (parser->tokens[lookahead].type == TOKEN_NUMBER)
                lookahead++;
            lookahead++;
        }
        if (parser->tokens[lookahead].type == TOKEN_IDENTIFIER &&
            parser->tokens[lookahead + 1].type == TOKEN_PAREN_LEFT) {
            temp = parser_def_function(parser);
        } else if (parser->tokens[parser->index + 1].type == TOKEN_IDENTIFIER &&
                    parser->tokens[parser->index + 2].type == TOKEN_PAREN_LEFT) {
            temp = parser_def_function(parser);
        } else {
            temp = parser_var(parser,true);
            parser_eat(parser, TOKEN_SEMICOLON);
        }
    } else if (parser->current.type == TOKEN_IDENTIFIER) {
        TokenType next = parser->tokens[parser->index + 1].type;
        TokenType next2 = parser->tokens[parser->index + 2].type;

        if (next == TOKEN_IDENTIFIER &&
            (next2 == TOKEN_ASSIGN || next2 == TOKEN_SEMICOLON || next2 == TOKEN_SQUARE_BRACKET_LEFT)) {
            temp = parser_var_with_typename(parser);
            parser_eat(parser, TOKEN_SEMICOLON);
            }

        else if (next == TOKEN_ASSIGN || next == TOKEN_SQUARE_BRACKET_LEFT) {
            temp = parser_def_var(parser);
            parser_eat(parser, TOKEN_SEMICOLON);
        }

        else if (next == TOKEN_DOT) {
            ASTNode* left = parser_Unary(parser);
            if (parser_current_is_type(parser, TOKEN_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_ADD_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_SUB_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_MUL_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_DIV_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_MOD_ASSIGN)) {
                temp = parser_member_assign(parser, left);
                parser_eat(parser, TOKEN_SEMICOLON);
                } else {
                    temp = left;
                    parser_eat(parser, TOKEN_SEMICOLON);
                }
        } else if (next == TOKEN_INCREMENT || next == TOKEN_DECREMENT ||
                 next == TOKEN_ADD_ASSIGN || next == TOKEN_SUB_ASSIGN ||
                 next == TOKEN_MUL_ASSIGN || next == TOKEN_DIV_ASSIGN ||
                 next == TOKEN_MOD_ASSIGN || next == TOKEN_SHIFT_LEFT_ASSIGN ||
                 next == TOKEN_SHIFT_RIGHT_ASSIGN || next == TOKEN_BITWISE_XOR_ASSIGN) {
            temp = parser_compound_assign_or_incdec(parser);
            parser_eat(parser, TOKEN_SEMICOLON);
                 }

        else {
            temp = parser_Binary(parser);
            parser_eat(parser, TOKEN_SEMICOLON);
        }
    } else if (parser->current.type == TOKEN_THIS) {
        TokenType next = parser->tokens[parser->index + 1].type;
        if (next == TOKEN_DOT) {
            ASTNode* left = parser_Unary(parser);
            if (parser_current_is_type(parser, TOKEN_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_ADD_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_SUB_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_MUL_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_DIV_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_MOD_ASSIGN)) {
                temp = parser_member_assign(parser, left);
                parser_eat(parser, TOKEN_SEMICOLON);
                } else {
                    temp = left;
                    parser_eat(parser, TOKEN_SEMICOLON);
                }
        } else {
            temp = parser_Binary(parser);
            parser_eat(parser, TOKEN_SEMICOLON);
        }
    } else if (parser->current.type == TOKEN_IF)
    {
        temp = parser_if(parser);
    } else if (parser->current.type == TOKEN_SWITCH) {
        temp = parser_switch(parser);
    } else if (parser->current.type == TOKEN_BREAK) {
        temp = parser_break(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    } else if (parser->current.type == TOKEN_CONTINUE) {
        temp = parser_continue(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    } else if (parser->current.type == TOKEN_WHILE)
    {
        temp = parser_while(parser);
    } else if (parser->current.type == TOKEN_DO)
    {
        temp = parser_do_while(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    } else if (parser->current.type == TOKEN_FOR) {
        temp = parser_for(parser);
    } else if (is_Modifier(parser) && !is_block)
    {
        temp = parser_def_function(parser);
    } else if (parser->current.type == TOKEN_RETURN)
    {
        temp = parser_return(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    } else if (parser->current.type == TOKEN_CLASS) {
        temp = parser_class_def(parser, 0);
    } else if (parser->current.type == TOKEN_ENUM) {
        temp = parser_enum_def(parser);
    } else if (parser->current.type == TOKEN_IMPORT) {
        ASTNode* imported = parser_import(parser);
        if (parser_current_is_type(parser, TOKEN_SEMICOLON))
            parser_eat(parser, TOKEN_SEMICOLON);
        return imported;
    } else
    {
        temp = parser_Binary(parser);
        parser_eat(parser, TOKEN_SEMICOLON);
    }
    return temp;
}

static ASTNode* clone_ast(ASTNode* node) {
    if (!node) return NULL;
    switch (node->type) {
    case AST_LITERAL: {
        AST_Literal* src = (AST_Literal*)node;
        AST_Literal* dst = calloc(1, sizeof(AST_Literal));
        if (!dst) OUT_OF_MEMORY();
        dst->base.type = AST_LITERAL;
        dst->base.next = NULL;
        if (src->base.value.value) {
            dst->base.value.value = strdup(src->base.value.value);
            dst->base.value.type  = src->base.value.type;
            dst->base.value.line  = src->base.value.line;
            dst->base.value.column = src->base.value.column;
        }
        dst->dataType     = src->dataType;
        dst->internal_name = src->internal_name ? strdup(src->internal_name) : NULL;
        dst->is_call_function = src->is_call_function;
        dst->call_function_Parameter = NULL;      // 浅拷贝：不递归
        dst->call_function_ParameterCount = 0;
        dst->method_arguments  = NULL;
        dst->method_arg_count  = 0;
        dst->is_field_ref      = src->is_field_ref;
        dst->field_offset      = src->field_offset;
        return (ASTNode*)dst;
    }
    case AST_THIS: {
        AST_This* src = (AST_This*)node;
        AST_This* dst = calloc(1, sizeof(AST_This));
        if (!dst) OUT_OF_MEMORY();
        dst->base.type  = AST_THIS;
        dst->base.next  = NULL;
        dst->instance_size = src->instance_size;
        return (ASTNode*)dst;
    }
    case AST_MEMBER_ACCESS: {
        AST_MemberAccess* src = (AST_MemberAccess*)node;
        AST_MemberAccess* dst = calloc(1, sizeof(AST_MemberAccess));
        if (!dst) OUT_OF_MEMORY();
        dst->base.type  = AST_MEMBER_ACCESS;
        dst->base.next  = NULL;
        dst->object     = clone_ast(src->object);
        dst->member_name = src->member_name ? strdup(src->member_name) : NULL;
        dst->member_offset = src->member_offset;
        dst->member_type   = src->member_type;
        dst->is_method     = src->is_method;
        return (ASTNode*)dst;
    }

    default:
        return NULL;
    }
}

static ASTNode* parser_member_assign(Parser* parser, ASTNode* left) {
    Token op = parser->current;
    parser_advance(parser);
    ASTNode* right = parser_Binary(parser);

    if (left->type != AST_MEMBER_ACCESS) {
        ERROR(ERR_PARSE_ILLEGAL_OPERAND,
              LOC(parser->filename, parser->current.line, parser->current.column),
              "left side of member assignment must be a member access");
        free(right);
        return NULL;
    }

    AST_MemberAccess* access = (AST_MemberAccess*)left;
    char* member_name = access->member_name;
    access->member_name = NULL;

    if (op.type != TOKEN_ASSIGN) {
        ASTNode* obj_copy = clone_ast(access->object);
        AST_MemberAccess* ma_copy = calloc(1, sizeof(AST_MemberAccess));
        ma_copy->base.type = AST_MEMBER_ACCESS;
        ma_copy->object = obj_copy;
        ma_copy->member_name = strdup(member_name);

        TokenType bin_op_type;
        switch (op.type) {
            case TOKEN_ADD_ASSIGN: bin_op_type = TOKEN_ADD; break;
            case TOKEN_SUB_ASSIGN: bin_op_type = TOKEN_SUB; break;
            case TOKEN_MUL_ASSIGN: bin_op_type = TOKEN_MUL; break;
            case TOKEN_DIV_ASSIGN: bin_op_type = TOKEN_DIV; break;
            case TOKEN_MOD_ASSIGN: bin_op_type = TOKEN_MOD; break;
            default:
                INTERNAL_ERROR("parser_member_assign: unknown compound assignment token");
                free(member_name);
                free(ma_copy);
                return NULL;
        }

        AST_Binary* bin = calloc(1, sizeof(AST_Binary));
        bin->base.type = AST_BINARY;
        bin->left = (ASTNode*)ma_copy;
        bin->op.type = bin_op_type;
        bin->op.value = strdup(type_to_string(bin_op_type));
        bin->right = right;
        right = (ASTNode*)bin;
    }

    AST_MemberAss* ass = calloc(1, sizeof(AST_MemberAss));
    ass->base.type = AST_MEMBER_ASS;
    ass->object = access->object;
    ass->member_name = member_name;
    ass->value = right;
    ass->member_type = DATA_ERROR;
    ass->member_offset = -1;

    free(access);
    return (ASTNode*)ass;
}

ASTNode* parser_return(Parser* parser)
{
    AST_Return* ast_return = malloc(sizeof(AST_Return));
    ast_return->base.value = parser->current;
    parser_eat(parser, TOKEN_RETURN);
    if (parser_current_is_type(parser, TOKEN_SEMICOLON)) {
        ast_return->value = NULL;
    } else {
        ast_return->value = parser_Binary(parser);
    }
    ast_return->base.next = NULL;
    ast_return->base.type = AST_RETURN;
    return (ASTNode*)ast_return;
}

void function_parameter_resize(AST_Function* ast_function)
{
    ast_function->max_parameters *= 2;
    DataType* parameterType = realloc(ast_function->parameterType, ast_function->max_parameters * sizeof(DataType));
    if (parameterType == NULL)
    {
        OUT_OF_MEMORY();
    }
    ast_function->parameterType = parameterType;
}

void function_add_parameter(AST_Function* ast_function, ASTNode* ast_parameter, DataType data_type)
{
    if (ast_function->Parameter == NULL)
    {
        ast_function->Parameter = ast_parameter;
        ast_function->parameterType[0] = data_type;
        ast_function->parameterCount++;
        if (ast_function->parameterCount == ast_function->max_parameters)
        {
            function_parameter_resize(ast_function);
        }
    } else
    {
        ASTNode* temp = ast_function->Parameter;
        while (temp->next != NULL)
        {
            temp = temp->next;
        }
        temp->next = ast_parameter;
        ast_function->parameterType[ast_function->parameterCount++] = data_type;
        if (ast_function->parameterCount == ast_function->max_parameters)
        {
            function_parameter_resize(ast_function);
        }
    }
}


static void set_return_type_recursive(ASTNode* node, DataType return_type)
{
    if (node == NULL) return;

    switch (node->type) {
        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            ret->returnType = return_type;
            break;
        }
        case AST_IF: {
            AST_If* if_node = (AST_If*)node;
            set_return_type_recursive(if_node->Condition, return_type);
            set_return_type_recursive(if_node->code, return_type);

            for (int i = 0; i < if_node->elif_Count; i++) {
                set_return_type_recursive(if_node->elif_condition[i], return_type);
                set_return_type_recursive(if_node->Elif[i], return_type);
            }

            set_return_type_recursive(if_node->Else, return_type);
            break;
        }
        case AST_WHILE: {
            AST_While* while_node = (AST_While*)node;
            set_return_type_recursive(while_node->Condition, return_type);
            set_return_type_recursive(while_node->code, return_type);
            break;
        }
        case AST_FOR: {
                    AST_For* f = (AST_For*)node;
                    set_return_type_recursive(f->init, return_type);
                    set_return_type_recursive(f->condition, return_type);
                    set_return_type_recursive(f->increment, return_type);
                    set_return_type_recursive(f->code, return_type);
                    break;
        }
        case AST_FOREACH: {
                    AST_ForEach* fe = (AST_ForEach*)node;
                    set_return_type_recursive(fe->array, return_type);
                    set_return_type_recursive(fe->code, return_type);
                    break;
        }
        case AST_FUNCTION: {
            AST_Function* func_node = (AST_Function*)node;
            set_return_type_recursive(func_node->Code, return_type);
            set_return_type_recursive(func_node->Parameter, return_type);
            if (func_node->Return) {
                set_return_type_recursive((ASTNode*)func_node->Return, return_type);
            }
            break;
        }
        case AST_PRINT: {
            AST_Print* print_node = (AST_Print*)node;
            set_return_type_recursive(print_node->Parameter, return_type);
            break;
        }
        case AST_VAR:
        case AST_VAR_ASS:
        case AST_UNINIT_VAR: {
            AST_Var* var_node = (AST_Var*)node;
            set_return_type_recursive(var_node->Value, return_type);
            break;
        }
        case AST_BINARY: {
            AST_Binary* binary_node = (AST_Binary*)node;
            set_return_type_recursive(binary_node->left, return_type);
            set_return_type_recursive(binary_node->right, return_type);
            break;
        }
        case AST_UNARY: {
            AST_Unary* unary_node = (AST_Unary*)node;
            set_return_type_recursive(unary_node->right, return_type);
            break;
        }
        case AST_LITERAL: {
            AST_Literal* literal_node = (AST_Literal*)node;
            if (literal_node->is_call_function) {
                set_return_type_recursive(literal_node->call_function_Parameter, return_type);
            }
            break;
        }
        case AST_CONDITIONAL: {
            AST_Conditional* tern = (AST_Conditional*)node;
            set_return_type_recursive(tern->condition, return_type);
            set_return_type_recursive(tern->then_expr, return_type);
            set_return_type_recursive(tern->else_expr, return_type);
            break;
        }
        case AST_SWITCH: {
                    AST_Switch* sw = (AST_Switch*)node;
                    set_return_type_recursive(sw->condition, return_type);
                    for (int i = 0; i < sw->case_count; i++) {
                        if (sw->cases[i]) {
                            set_return_type_recursive(sw->cases[i]->value, return_type);
                            set_return_type_recursive(sw->cases[i]->body, return_type);
                        }
                    }
                    if (sw->has_default && sw->default_body) {
                        set_return_type_recursive(sw->default_body, return_type);
                    }
                    break;
        }
        case AST_CASE: {
                    AST_Case* cs = (AST_Case*)node;
                    if (cs->value) set_return_type_recursive(cs->value, return_type);
                    if (cs->body)  set_return_type_recursive(cs->body, return_type);
                    break;
        }
        default:
            break;
    }

    set_return_type_recursive(node->next, return_type);
}

ASTNode* parser_constructor_def(Parser* parser, int modifier)
{
    AST_Function* ast_function = calloc(1, sizeof(AST_Function));
    if (!ast_function) {
        OUT_OF_MEMORY();
    }
    ast_function->max_parameters = 4;
    ast_function->parameterType = malloc(sizeof(DataType) * ast_function->max_parameters);
    ast_function->return_Count = 0;
    ast_function->parameterCount = 0;
    ast_function->Parameter = NULL;
    ast_function->Return = NULL;
    ast_function->Modifier = modifier;
    ast_function->internal_name = NULL;
    DataType return_type = DATA_IDENTIFIER;
    ast_function->base.value = parser->current;
    ast_function->name = parser->current;
    parser_advance(parser);
    parser_eat(parser, TOKEN_PAREN_LEFT);
    while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT)
           && !parser_current_is_type(parser, TOKEN_EOF))
    {
        ASTNode* param = parser_var(parser,false);
        if (param == NULL) {
            while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT)
                   && !parser_current_is_type(parser, TOKEN_COMMA)
                   && !parser_current_is_type(parser, TOKEN_EOF))
                parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_COMMA))
                parser_advance(parser);
            continue;
        }
        DataType param_dtype;
        if (param->type == AST_ARRAY_DECL) {
            AST_ArrayDecl* arr = (AST_ArrayDecl*)param;
            param_dtype = arr->elementType;
        } else if (param->type == AST_VAR || param->type == AST_UNINIT_VAR) {
            AST_Var* var = (AST_Var*)param;
            var->dataType = Get_Datatype(var->type);
            param_dtype = var->dataType;
        } else {
            param_dtype = DATA_ERROR;
        }
        function_add_parameter(ast_function, param, param_dtype);
        if (parser_current_is_type(parser, TOKEN_COMMA)) {
            parser_eat(parser, TOKEN_COMMA);
        } else {
            break;
        }
    }
    parser_eat(parser, TOKEN_PAREN_RIGHT);
    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ast_function->Code = parser_parse_code_block(parser);
    set_return_type_recursive(ast_function->Code, return_type);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);
    ASTNode* scan = ast_function->Code;
    while (scan != NULL) {
        if (scan->type == AST_RETURN) {
            AST_Return* ret = (AST_Return*)scan;
            ast_function->Return = ret;
            ast_function->return_Count = 1;
            ret->returnType = return_type;
        }
        scan = scan->next;
    }
    ast_function->base.type = AST_FUNCTION;
    ast_function->base.next = NULL;
    return (ASTNode*)ast_function;
}

ASTNode* parser_class_def(Parser* parser, int modifier)
{
    parser_eat(parser, TOKEN_CLASS);
    char* name = strdup(parser->current.value);
    parser_eat(parser, TOKEN_IDENTIFIER);
    char** parents = NULL;
    int parent_count = 0;
    if (parser_current_is_type(parser, TOKEN_COLON)) {
        parser_advance(parser);
        int cap = 2;
        parents = malloc(sizeof(char*) * cap);
        do {
            if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
                ERROR(ERR_PARSE_EXPECTED_TOKEN,
                      LOC(parser->filename, parser->current.line, parser->current.column),
                      "expected parent class name after ':'");
                break;
            }
            if (parent_count >= cap) {
                cap *= 2;
                parents = realloc(parents, sizeof(char*) * cap);
            }
            parents[parent_count] = strdup(parser->current.value);
            parent_count++;
            parser_eat(parser, TOKEN_IDENTIFIER);
        } while (parser_current_is_type(parser, TOKEN_COMMA)
                 && (parser_advance(parser), 1));
    }
    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ASTNode* members_head = NULL;
    ASTNode* members_tail = NULL;
    while (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
           !parser_current_is_type(parser, TOKEN_EOF))
    {
        ASTNode* stmt = parser_Sentence(parser, true);
        if (stmt) {
            ASTNode* stmt_tail = stmt;
            while (stmt_tail->next != NULL)
                stmt_tail = stmt_tail->next;
            if (!members_head) {
                members_head = stmt;
                members_tail = stmt_tail;
            } else {
                members_tail->next = stmt;
                members_tail = stmt_tail;
            }
        }
    }
    parser_eat(parser, TOKEN_BRACKET_RIGHT);
    AST_ClassDef* class_def = calloc(1, sizeof(AST_ClassDef));
    class_def->base.type = AST_CLASS_DEF;
    class_def->name = name;
    class_def->parent_names = parents;
    class_def->parent_count = parent_count;
    class_def->modifier = modifier;
    class_def->members = members_head;
    return (ASTNode*)class_def;
}

ASTNode* parser_enum_def(Parser* parser)
{
    parser_eat(parser, TOKEN_ENUM);

    char* name = NULL;
    if (parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
        name = strdup(parser->current.value);
        parser_advance(parser);
    }

    parser_eat(parser, TOKEN_BRACKET_LEFT);

    AST_EnumDef* enum_def = calloc(1, sizeof(AST_EnumDef));
    enum_def->base.type = AST_ENUM_DEF;
    enum_def->base.next = NULL;
    enum_def->name = name;
    enum_def->member_count = 0;
    enum_def->member_capacity = 4;
    enum_def->members = malloc(sizeof(AST_EnumMember*) * enum_def->member_capacity);
    enum_def->modifier = 0;

    while (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
           !parser_current_is_type(parser, TOKEN_EOF))
    {
        if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected enum member name");
            while (!parser_current_is_type(parser, TOKEN_COMMA) &&
                   !parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
                   !parser_current_is_type(parser, TOKEN_EOF))
                parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_COMMA))
                parser_advance(parser);
            continue;
        }

        AST_EnumMember* member = calloc(1, sizeof(AST_EnumMember));
        member->base.type = AST_ENUM_MEMBER;
        member->base.next = NULL;
        member->name = strdup(parser->current.value);
        member->value = -1;
        member->has_explicit_value = false;
        member->explicit_token = NULL;

        parser_advance(parser);

        if (parser_current_is_type(parser, TOKEN_ASSIGN)) {
            parser_advance(parser);

            if (parser_current_is_type(parser, TOKEN_NUMBER)) {
                member->has_explicit_value = true;
                member->explicit_token = malloc(sizeof(Token));
                *member->explicit_token = parser->current;
                member->value = atoi(parser->current.value);
                parser_advance(parser);
            } else if (parser_current_is_type(parser, TOKEN_SUB) &&
                       parser->tokens[parser->index + 1].type == TOKEN_NUMBER) {
                member->has_explicit_value = true;
                member->explicit_token = malloc(sizeof(Token));
                member->explicit_token->type = TOKEN_NUMBER;
                member->explicit_token->line = parser->current.line;
                member->explicit_token->column = parser->current.column;
                int len = strlen(parser->current.value) +
                          strlen(parser->tokens[parser->index + 1].value) + 1;
                member->explicit_token->value = malloc(len);
                snprintf(member->explicit_token->value, len, "-%s",
                         parser->tokens[parser->index + 1].value);
                member->value = -atoi(parser->tokens[parser->index + 1].value);
                parser_advance(parser);
                parser_advance(parser);
            } else {
                ERROR(ERR_PARSE_ILLEGAL_OPERAND,
                      LOC(parser->filename, parser->current.line, parser->current.column),
                      "enum member '%s' requires an integer constant",
                      member->name);
                while (!parser_current_is_type(parser, TOKEN_COMMA) &&
                       !parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
                       !parser_current_is_type(parser, TOKEN_EOF))
                    parser_advance(parser);
            }
        }

        if (enum_def->member_count >= enum_def->member_capacity) {
            enum_def->member_capacity *= 2;
            enum_def->members = realloc(enum_def->members,
                                        sizeof(AST_EnumMember*) * enum_def->member_capacity);
        }
        enum_def->members[enum_def->member_count++] = member;

        if (parser_current_is_type(parser, TOKEN_COMMA)) {
            parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_BRACKET_RIGHT))
                break;
        } else {
            break;
        }
    }

    parser_eat(parser, TOKEN_BRACKET_RIGHT);

    return (ASTNode*)enum_def;
}

ASTNode* parser_class_def_with_modifier(Parser* parser, int modifier) {
    parser_eat(parser, TOKEN_CLASS);
    char* name = strdup(parser->current.value);
    parser_eat(parser, TOKEN_IDENTIFIER);
    char** parents = NULL;
    int parent_count = 0;
    if (parser_current_is_type(parser, TOKEN_COLON)) {
        parser_advance(parser);
        int cap = 2;
        parents = malloc(sizeof(char*) * cap);
        do {
            if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
                ERROR(ERR_PARSE_EXPECTED_TOKEN,
                      LOC(parser->filename, parser->current.line, parser->current.column),
                      "expected parent class name after ':'");
                break;
            }
            if (parent_count >= cap) {
                cap *= 2;
                parents = realloc(parents, sizeof(char*) * cap);
            }
            parents[parent_count] = strdup(parser->current.value);
            parent_count++;
            parser_eat(parser, TOKEN_IDENTIFIER);
        } while (parser_current_is_type(parser, TOKEN_COMMA)
                 && (parser_advance(parser), 1));
    }
    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ASTNode* members_head = NULL;
    ASTNode* members_tail = NULL;
    while (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
           !parser_current_is_type(parser, TOKEN_EOF))
    {
        ASTNode* stmt = parser_Sentence(parser, true);
        if (stmt) {
            if (stmt->type == AST_FUNCTION) {
                AST_Function* func = (AST_Function*)stmt;
                int len = strlen(name) + 1 + strlen(func->name.value) + 1;
                char* mangled = malloc(len);
                if (mangled) {
                    snprintf(mangled, len, "%s_%s", name, func->name.value);
                }
                if (func->internal_name) free(func->internal_name);
                func->internal_name = mangled;
            }
            if (!members_head) {
                members_head = stmt;
                members_tail = stmt;
            } else {
                members_tail->next = stmt;
                while (members_tail->next != NULL) {
                    members_tail = members_tail->next;
                }
            }
        }
    }
    parser_eat(parser, TOKEN_BRACKET_RIGHT);
    AST_ClassDef* class_def = calloc(1, sizeof(AST_ClassDef));
    class_def->base.type = AST_CLASS_DEF;
    class_def->name = name;
    class_def->parent_names = parents;
    class_def->parent_count = parent_count;
    class_def->modifier = modifier;
    class_def->members = members_head;
    return (ASTNode*)class_def;
}

ASTNode* parser_def_function_with_modifier(Parser* parser, int modifier) {
    AST_Function* ast_function = malloc(sizeof(AST_Function));
    ast_function->max_parameters = 4;
    ast_function->parameterType = malloc(sizeof(DataType) * ast_function->max_parameters);
    ast_function->return_Count = 0;
    ast_function->parameterCount = 0;
    ast_function->Parameter = NULL;
    ast_function->Return = NULL;
    ast_function->Modifier = modifier;
    ast_function->internal_name = NULL;
    ast_function->returns_array       = false;
    ast_function->return_element_type = DATA_VOID;
    ast_function->return_dim_count    = 0;
    ast_function->return_dim_sizes    = NULL;

    DataType return_type = DATA_VOID;
    if (is_var_type(parser->current.type)) {
        return_type = Get_Datatype(parser->current.type);
    } else {
        ERROR(ERR_PARSE_INVALID_TYPE,
              LOC(parser->filename, parser->current.line, parser->current.column),
              "expected type keyword before '%s'",
              type_to_string(parser->current.type));
    }
    parser_advance(parser);

    if (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT))
    {
        int dim_cap = 4;
        int dim_cnt = 0;
        int* dim_sizes = malloc(sizeof(int) * dim_cap);
        if (!dim_sizes) OUT_OF_MEMORY();
        while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT))
        {
            parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
            int size = -1;
            if (parser_current_is_type(parser, TOKEN_NUMBER))
            {
                size = atoi(parser->current.value);
                parser_eat(parser, TOKEN_NUMBER);
            }
            else if (!parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_RIGHT))
            {
                ERROR(ERR_PARSE_EXPECTED_TOKEN,
                      LOC(parser->filename, parser->current.line,
                          parser->current.column),
                      "expected array size or ']' in return type");
                free(dim_sizes);
                return NULL;
            }
            parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);
            if (dim_cnt >= dim_cap)
            {
                dim_cap *= 2;
                dim_sizes = realloc(dim_sizes, sizeof(int) * (size_t)dim_cap);
                if (!dim_sizes) OUT_OF_MEMORY();
            }
            dim_sizes[dim_cnt++] = size;
        }
        if (dim_cnt > 0)
        {
            ast_function->returns_array       = true;
            ast_function->return_element_type = return_type;
            ast_function->return_dim_count    = dim_cnt;
            ast_function->return_dim_sizes    = dim_sizes;
            return_type = DATA_VOID;
        }
        else
        {
            free(dim_sizes);
        }
    }

    ast_function->base.value = parser->current;
    ast_function->name = parser->current;
    parser_advance(parser);

    parser_eat(parser, TOKEN_PAREN_LEFT);
    while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT) && !parser_current_is_type(parser, TOKEN_EOF)) {
        ASTNode* param = parser_var(parser,false);
        if (param == NULL) {
            while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT) && !parser_current_is_type(parser, TOKEN_EOF))
                parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_COMMA)) parser_advance(parser);
            continue;
        }
        DataType param_dtype;
        if (param->type == AST_ARRAY_DECL) {
            AST_ArrayDecl* arr = (AST_ArrayDecl*)param;
            param_dtype = arr->elementType;
        } else if (param->type == AST_VAR || param->type == AST_UNINIT_VAR) {
            AST_Var* var = (AST_Var*)param;
            var->dataType = Get_Datatype(var->type);
            param_dtype = var->dataType;
        } else {
            param_dtype = DATA_ERROR;
        }
        function_add_parameter(ast_function, param, param_dtype);
        if (parser_current_is_type(parser, TOKEN_COMMA)) parser_advance(parser);
        else break;
    }
    parser_eat(parser, TOKEN_PAREN_RIGHT);

    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ast_function->Code = parser_parse_code_block(parser);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);

    set_return_type_recursive(ast_function->Code, return_type);

    ASTNode* temp = ast_function->Code;
    while (temp) {
        if (temp->type == AST_RETURN) {
            AST_Return* ret = (AST_Return*)temp;
            ast_function->Return = ret;
            ast_function->return_Count = 1;
            ret->returnType = return_type;
        }
        temp = temp->next;
    }

    ast_function->base.type = AST_FUNCTION;
    ast_function->base.next = NULL;
    return (ASTNode*)ast_function;
}

ASTNode* parser_var_with_typename(Parser* parser) {
    char* className = strdup(parser->current.value);
    parser_advance(parser);

    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    do {
        if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected variable name after type");
            free(className);
            AST_Free(head);
            return NULL;
        }

        Token varName = parser->current;
        parser_advance(parser);

        ASTNode* init = NULL;
        if (parser_current_is_type(parser, TOKEN_ASSIGN)) {
            parser_advance(parser);
            init = parser_Binary(parser);
        }

        AST_Var* var = calloc(1, sizeof(AST_Var));
        var->base.type = (init != NULL) ? AST_VAR : AST_UNINIT_VAR;
        var->base.next = NULL;
        var->base.value = varName;
        var->type = TOKEN_IDENTIFIER;
        var->name = varName;
        var->Value = init;
        var->dataType = DATA_IDENTIFIER;
        var->internal_name = className;
        var->dim_count = 0;
        var->dim_sizes = NULL;
        var->modifier = 0;

        ASTNode* node = (ASTNode*)var;

        if (!head) {
            head = tail = node;
        } else {
            var->internal_name = strdup(className);
            tail->next = node;
            tail = node;
        }

        if (!parser_current_is_type(parser, TOKEN_COMMA))
            break;
        parser_advance(parser);

    } while (true);

    return head;
}

ASTNode* parser_def_function(Parser* parser)
{
    AST_Function* ast_function = malloc(sizeof(AST_Function));
    ast_function->max_parameters = 4;
    ast_function->parameterType = malloc(sizeof(DataType) * ast_function->max_parameters);
    ast_function->return_Count = 0;
    ast_function->parameterCount = 0;
    ast_function->Parameter = NULL;
    ast_function->Return = NULL;
    ast_function->internal_name = NULL;
    ast_function->returns_array      = false;
    ast_function->return_element_type = DATA_VOID;
    ast_function->return_dim_count   = 0;
    ast_function->return_dim_sizes   = NULL;

    if (parser_current_is_type(parser, TOKEN_PUB))
    {
        ast_function->Modifier = 0;
        parser_eat(parser, TOKEN_PUB);
    } else if (parser_current_is_type(parser, TOKEN_PRO))
    {
        ast_function->Modifier = 1;
        parser_eat(parser, TOKEN_PRO);
    } else if (parser_current_is_type(parser, TOKEN_PRI))
    {
        ast_function->Modifier = 2;
        parser_eat(parser, TOKEN_PRI);
    }
    DataType return_type = DATA_VOID;
    if (is_var_type(parser->current.type))
    {
        return_type = Get_Datatype(parser->current.type);
    } else
    {
        ERROR(ERR_PARSE_INVALID_TYPE,
              LOC(parser->filename, parser->current.line, parser->current.column),
              "expected type keyword before '%s'",
              type_to_string(parser->current.type));
    }
    parser_advance(parser);

    if (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT))
    {
        int dim_cap = 4;
        int dim_cnt = 0;
        int* dim_sizes = malloc(sizeof(int) * dim_cap);
        if (!dim_sizes) OUT_OF_MEMORY();
        while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT))
        {
            parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
            int size = -1;
            if (parser_current_is_type(parser, TOKEN_NUMBER))
            {
                size = atoi(parser->current.value);
                parser_eat(parser, TOKEN_NUMBER);
            }
            else if (!parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_RIGHT))
            {
                ERROR(ERR_PARSE_EXPECTED_TOKEN,
                      LOC(parser->filename, parser->current.line,
                          parser->current.column),
                      "expected array size or ']' in return type");
                free(dim_sizes);
                return NULL;
            }
            parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);
            if (dim_cnt >= dim_cap)
            {
                dim_cap *= 2;
                dim_sizes = realloc(dim_sizes, sizeof(int) * (size_t)dim_cap);
                if (!dim_sizes) OUT_OF_MEMORY();
            }
            dim_sizes[dim_cnt++] = size;
        }
        if (dim_cnt > 0)
        {
            ast_function->returns_array       = true;
            ast_function->return_element_type = return_type;
            ast_function->return_dim_count    = dim_cnt;
            ast_function->return_dim_sizes    = dim_sizes;
            return_type = DATA_VOID;
        }
        else
        {
            free(dim_sizes);
        }
    }

    ast_function->base.value = parser->current;
    ast_function->name = parser->current;
    parser_advance(parser);

    parser_eat(parser, TOKEN_PAREN_LEFT);
    while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT) && !parser_current_is_type(parser, TOKEN_EOF))
    {
        ASTNode* temp = parser_var(parser,false);
        if (temp == NULL) {
            while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT) &&
                   !parser_current_is_type(parser, TOKEN_COMMA) &&
                   !parser_current_is_type(parser, TOKEN_EOF))
                parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_COMMA))
                parser_advance(parser);
            continue;
        }

        DataType param_dtype;
        if (temp->type == AST_ARRAY_DECL) {
            AST_ArrayDecl* arr = (AST_ArrayDecl*)temp;
            param_dtype = arr->elementType;
        } else if (temp->type == AST_VAR || temp->type == AST_UNINIT_VAR) {
            AST_Var* var = (AST_Var*)temp;
            var->dataType = Get_Datatype(var->type);
            param_dtype = var->dataType;
        } else {
            param_dtype = DATA_ERROR;
        }

        function_add_parameter(ast_function, temp, param_dtype);

        if (parser_current_is_type(parser, TOKEN_COMMA)) {
            parser_eat(parser, TOKEN_COMMA);
        } else {
            break;
        }
    }
    parser_eat(parser, TOKEN_PAREN_RIGHT);
    parser_eat(parser, TOKEN_BRACKET_LEFT);

    ast_function->Code = parser_parse_code_block(parser);
    set_return_type_recursive(ast_function->Code, return_type);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);
    ASTNode* temp = ast_function->Code;
    while (temp != NULL)
    {
        if (temp->type == AST_RETURN)
        {
            AST_Return* ast_return = (AST_Return*)temp;
            ast_function->Return = ast_return;
            ast_function->return_Count = 1;
        }
        temp = temp->next;
    }
    ast_function->base.type = AST_FUNCTION;
    ast_function->base.next = NULL;
    return (ASTNode*)ast_function;
}

ASTNode* parser_while(Parser* parser)
{
    AST_While* ast_while = malloc(sizeof(AST_While));
    ast_while->base.value = parser->current;
    parser_eat(parser, TOKEN_WHILE);
    ast_while->Condition = parser_Binary(parser);
    ast_while->has_do = false;
    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ast_while->code = parser_parse_code_block(parser);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);
    ast_while->base.next = NULL;
    ast_while->base.type = AST_WHILE;
    return (ASTNode*)ast_while;
}

ASTNode* parser_do_while(Parser* parser)
{
    AST_While* ast_while = malloc(sizeof(AST_While));
    ast_while->base.value = parser->current;
    parser_eat(parser, TOKEN_DO);

    ast_while->has_do = true;

    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ast_while->code = parser_parse_code_block(parser);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);

    parser_eat(parser, TOKEN_WHILE);

    ast_while->Condition = parser_Binary(parser);

    ast_while->base.next = NULL;
    ast_while->base.type = AST_WHILE;
    return (ASTNode*)ast_while;
}

static ASTNode* parser_for_init_part(Parser* parser) {
    ASTNode *head = NULL;
    ASTNode *tail = NULL;

    while (!parser_current_is_type(parser, TOKEN_SEMICOLON) &&
           !parser_current_is_type(parser, TOKEN_EOF)) {
        ASTNode* item = NULL;

        if (is_var_type(parser->current.type)) {
            TokenType saved_type = parser->current.type;
            parser_advance(parser);

            if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
                ERROR(ERR_PARSE_EXPECTED_TOKEN,
                      LOC(parser->filename, parser->current.line, parser->current.column),
                      "expected variable name");
                return head;
            }

            Token var_name = parser->current;
            parser_advance(parser);

            ASTNode* init_value = NULL;
            if (parser_current_is_type(parser, TOKEN_ASSIGN)) {
                parser_advance(parser);
                init_value = parser_Binary(parser);
            }

            AST_Var* var = calloc(1, sizeof(AST_Var));
            var->base.type = (init_value != NULL) ? AST_VAR : AST_UNINIT_VAR;
            var->base.next = NULL;
            var->base.value = var_name;
            var->type = saved_type;
            var->name = var_name;
            var->Value = init_value;
            var->dataType = DATA_ERROR;
            var->internal_name = NULL;
            var->dim_count = 0;
            var->dim_sizes = NULL;

            item = (ASTNode*)var;
        } else {
            item = parser_Binary(parser);
        }

        if (!item) break;

        if (!head) {
            head = item;
            tail = item;
        } else {
            tail->next = item;
            tail = item;
        }

        if (parser_current_is_type(parser, TOKEN_COMMA)) {
            parser_advance(parser);
        } else {
            break;
        }
           }
    parser_eat(parser, TOKEN_SEMICOLON);
    return head;
}

static ASTNode* parser_statement_expr(Parser* parser) {
    if (parser_current_is_type(parser, TOKEN_IDENTIFIER))
    {
        TokenType next = parser->tokens[parser->index + 1].type;
        if (next == TOKEN_INCREMENT || next == TOKEN_DECREMENT ||
            next == TOKEN_ADD_ASSIGN || next == TOKEN_SUB_ASSIGN ||
            next == TOKEN_MUL_ASSIGN || next == TOKEN_DIV_ASSIGN ||
            next == TOKEN_MOD_ASSIGN) {
            return parser_compound_assign_or_incdec(parser);
            }

        if (next == TOKEN_ASSIGN) {
            Token var_token = parser->current;
            parser_advance(parser);
            parser_advance(parser);
            ASTNode* value = parser_Binary(parser);
            AST_Var* var = calloc(1, sizeof(AST_Var));
            if (!var) {
                OUT_OF_MEMORY();
            }
            var->base.type = AST_VAR_ASS;
            var->base.next = NULL;
            var->base.value = var_token;
            var->name = var_token;
            var->Value = value;
            var->type = var_token.type;
            var->dataType = DATA_ERROR;
            var->internal_name = NULL;
            var->is_field_ref = false;
            var->field_offset = 0;
            return (ASTNode*)var;
        }

        if (next == TOKEN_SQUARE_BRACKET_LEFT) {
            return parser_def_var(parser);
        }
    }

    if (parser_current_is_type(parser, TOKEN_THIS)) {
        TokenType next = parser->tokens[parser->index + 1].type;
        if (next == TOKEN_DOT) {

            ASTNode* left = parser_Unary(parser);

            if (parser_current_is_type(parser, TOKEN_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_ADD_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_SUB_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_MUL_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_DIV_ASSIGN) ||
                parser_current_is_type(parser, TOKEN_MOD_ASSIGN)) {
                return parser_member_assign(parser, left);
            }
            return left;
        }
    }

    if (parser_current_is_type(parser, TOKEN_INCREMENT) ||
        parser_current_is_type(parser, TOKEN_DECREMENT)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Unary(parser);
        AST_Unary* unary = malloc(sizeof(AST_Unary));
        unary->base.type = AST_UNARY;
        unary->base.next = NULL;
        unary->base.value = op;
        unary->op = op;
        unary->right = right;
        unary->dataType = DATA_ERROR;
        return (ASTNode*)unary;
    }

    return parser_Binary(parser);
}

ASTNode* parser_for(Parser* parser) {
    parser_eat(parser, TOKEN_FOR);

    if (is_var_type(parser->current.type)) {
        int saved_idx = parser->index;
        TokenType t1 = parser->tokens[saved_idx + 1].type;
        TokenType t2 = parser->tokens[saved_idx + 2].type;
        if (t1 == TOKEN_IDENTIFIER && t2 == TOKEN_COLON) {
            DataType declared_type = Get_Datatype(parser->current.type);
            parser_advance(parser);
            Token iter_token = parser->current;
            parser_advance(parser);
            parser_eat(parser, TOKEN_COLON);
            ASTNode* array_expr = parser_Binary(parser);
            parser_eat(parser, TOKEN_BRACKET_LEFT);
            ASTNode* body = parser_parse_code_block(parser);
            parser_eat(parser, TOKEN_BRACKET_RIGHT);
            AST_ForEach* foreach_node = calloc(1, sizeof(AST_ForEach));
            foreach_node->base.type = AST_FOREACH;
            foreach_node->iterator = iter_token;
            foreach_node->array = array_expr;
            foreach_node->code = body;
            foreach_node->internal_iterator = NULL;
            foreach_node->elemType = declared_type;
            foreach_node->has_explicit_type = true;
            return (ASTNode*)foreach_node;
        }
    }

    if (parser_current_is_type(parser, TOKEN_IDENTIFIER) &&
        parser->tokens[parser->index + 1].type == TOKEN_COLON) {

        Token iter_token = parser->current;
        parser_advance(parser);
        parser_eat(parser, TOKEN_COLON);

        ASTNode* array_expr = parser_Binary(parser);

        parser_eat(parser, TOKEN_BRACKET_LEFT);
        ASTNode* body = parser_parse_code_block(parser);
        parser_eat(parser, TOKEN_BRACKET_RIGHT);

        AST_ForEach* foreach_node = calloc(1, sizeof(AST_ForEach));
        foreach_node->base.type = AST_FOREACH;
        foreach_node->iterator = iter_token;
        foreach_node->array = array_expr;
        foreach_node->code = body;
        foreach_node->internal_iterator = NULL;
        foreach_node->has_explicit_type = false;
        foreach_node->elemType = DATA_ERROR;
        return (ASTNode*)foreach_node;
    }

    ASTNode* init = parser_for_init_part(parser);

    ASTNode* condition = NULL;
    if (!parser_current_is_type(parser, TOKEN_SEMICOLON)) {
        condition = parser_Binary(parser);
    }
    parser_eat(parser, TOKEN_SEMICOLON);

    ASTNode* increment = NULL;
    ASTNode* incr_tail = NULL;

    while (!parser_current_is_type(parser, TOKEN_BRACKET_LEFT) &&
           !parser_current_is_type(parser, TOKEN_EOF)) {
               ASTNode* stmt = parser_statement_expr(parser);
               if (!stmt) break;
               if (!increment) {
                   increment = stmt;
                   incr_tail = stmt;
               } else {
                   incr_tail->next = stmt;
                   incr_tail = stmt;
               }
               if (parser_current_is_type(parser, TOKEN_COMMA)) {
                   parser_advance(parser);
               } else {
                   break;
               }
           }

    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ASTNode* body = parser_parse_code_block(parser);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);

    AST_For* for_node = calloc(1, sizeof(AST_For));
    for_node->base.type = AST_FOR;
    for_node->init = init;
    for_node->condition = condition;
    for_node->increment = increment;
    for_node->code = body;
    return (ASTNode*)for_node;
}

void parser_elif_resize(AST_If* ast_if)
{

    ast_if->elif_condition = realloc(ast_if->elif_condition, sizeof(ASTNode*) * (ast_if->elif_Count + 2));
    if (ast_if->elif_condition == NULL)
    {
        INTERNAL_ERROR("parser_elif_resize: realloc failed");
    }
    ast_if->Elif = realloc(ast_if->Elif, sizeof(ASTNode*) * (ast_if->elif_Count + 2));
    if (ast_if->Elif == NULL)
    {
        INTERNAL_ERROR("parser_elif_resize: realloc failed");
    }
}

void parser_elif_add(AST_If* ast_if, ASTNode* condition, ASTNode* code)
{
    ast_if->elif_condition[ast_if->elif_Count - 1] = condition;

    ast_if->Elif[ast_if->elif_Count - 1] = code;

    parser_elif_resize(ast_if);
}

ASTNode* parser_if(Parser* parser)
{
    AST_If* ast_if = malloc(sizeof(AST_If));
    ast_if->base.value = parser->current;
    ast_if->elif_Count = 0;
    parser_eat(parser, TOKEN_IF);

    ASTNode* Condition = parser_Binary(parser);
    parser_eat(parser, TOKEN_BRACKET_LEFT);
    ASTNode* code = parser_parse_code_block(parser);
    parser_eat(parser, TOKEN_BRACKET_RIGHT);

    ast_if->elif_condition = calloc(1, sizeof(ASTNode*));
    ast_if->Elif = calloc(1, sizeof(ASTNode*));

    ASTNode* el_code = NULL;
    while (parser_current_is_type(parser, TOKEN_ELSE))
    {
        parser_eat(parser, TOKEN_ELSE);
        if (parser_current_is_type(parser, TOKEN_IF))
        {
            parser_eat(parser, TOKEN_IF);
            ast_if->elif_Count++;
            ASTNode* elif_condition_temp = parser_Binary(parser);

            parser_eat(parser, TOKEN_BRACKET_LEFT);
            ASTNode* elif_code_temp = parser_parse_code_block(parser);
            parser_eat(parser, TOKEN_BRACKET_RIGHT);

            parser_elif_add(ast_if, elif_condition_temp, elif_code_temp);
        } else
        {
            ast_if->has_else = true;
            parser_eat(parser, TOKEN_BRACKET_LEFT);
            el_code = parser_parse_code_block(parser);
            parser_eat(parser, TOKEN_BRACKET_RIGHT);
            break;
        }
    }

    ast_if->Condition = Condition;
    ast_if->code = code;
    ast_if->Else = el_code;
    ast_if->base.next = NULL;
    ast_if->base.type = AST_IF;
    return (ASTNode*)ast_if;
}

ASTNode* parser_switch(Parser* parser) {
    parser_eat(parser, TOKEN_SWITCH);
    ASTNode* cond = parser_Binary(parser);
    parser_eat(parser, TOKEN_BRACKET_LEFT);

    AST_Switch* sw = calloc(1, sizeof(AST_Switch));
    sw->base.type = AST_SWITCH;
    sw->condition = cond;
    sw->cases = NULL;
    sw->case_count = 0;
    sw->case_capacity = 0;
    sw->has_default = false;
    sw->default_body = NULL;

    while (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
           !parser_current_is_type(parser, TOKEN_EOF)) {
        if (parser_current_is_type(parser, TOKEN_CASE)) {
            parser_eat(parser, TOKEN_CASE);
            ASTNode* case_val = parser_Binary(parser);
            parser_eat(parser, TOKEN_COLON);

            ASTNode* body_head = NULL;
            ASTNode* body_tail = NULL;
            while (!parser_current_is_type(parser, TOKEN_CASE) &&
                   !parser_current_is_type(parser, TOKEN_DEFAULT) &&
                   !parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
                   !parser_current_is_type(parser, TOKEN_EOF)) {
                ASTNode* stmt = parser_Sentence(parser, true);
                if (stmt) {
                    // 找到 stmt 链表的真正尾节点
                    ASTNode* stmt_tail = stmt;
                    while (stmt_tail->next != NULL)
                        stmt_tail = stmt_tail->next;

                    if (!body_head) {
                        body_head = stmt;
                        body_tail = stmt_tail;
                    } else {
                        body_tail->next = stmt;
                        body_tail = stmt_tail;
                    }
                }
            }

            AST_Case* cs = calloc(1, sizeof(AST_Case));
            cs->base.type = AST_CASE;
            cs->value = case_val;
            cs->body = body_head;
            cs->is_default = false;

            if (sw->case_count >= sw->case_capacity) {
                sw->case_capacity = (sw->case_capacity == 0) ? 4 : sw->case_capacity * 2;
                sw->cases = realloc(sw->cases, sizeof(AST_Case*) * sw->case_capacity);
            }
            sw->cases[sw->case_count++] = cs;
        }
        else if (parser_current_is_type(parser, TOKEN_DEFAULT)) {
            parser_eat(parser, TOKEN_DEFAULT);
            parser_eat(parser, TOKEN_COLON);

            ASTNode* body_head = NULL;
            ASTNode* body_tail = NULL;
            while (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT) &&
                   !parser_current_is_type(parser, TOKEN_EOF)) {
                ASTNode* stmt = parser_Sentence(parser, true);
                if (stmt) {
                    ASTNode* stmt_tail = stmt;
                    while (stmt_tail->next != NULL)
                        stmt_tail = stmt_tail->next;
                    if (!body_head) {
                        body_head = stmt;
                        body_tail = stmt_tail;
                    } else {
                        body_tail->next = stmt;
                        body_tail = stmt_tail;
                    }
                }
            }

            sw->has_default = true;
            sw->default_body = body_head;
        } else {
            parser_advance(parser);
        }
    }

    parser_eat(parser, TOKEN_BRACKET_RIGHT);
    return (ASTNode*)sw;
}

ASTNode* parser_break(Parser* parser) {
    AST_Break* b = calloc(1, sizeof(AST_Break));
    b->base.type = AST_BREAK;
    b->base.value = parser->current;
    b->base.next = NULL;
    parser_advance(parser);
    return (ASTNode*)b;
}

ASTNode* parser_continue(Parser* parser) {
    AST_Continue* c = calloc(1, sizeof(AST_Continue));
    c->base.type = AST_CONTINUE;
    c->base.value = parser->current;
    parser_advance(parser);
    return (ASTNode*)c;
}

ASTNode* parser_def_var(Parser* parser)
{
    Token name_token = parser->current;
    parser_advance(parser);

    if (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
        int idx_cap = 4, idx_cnt = 0;
        ASTNode** indices = malloc(sizeof(ASTNode*) * idx_cap);
        while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
            parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
            ASTNode* idx = parser_Binary(parser);
            parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);
            if (idx_cnt >= idx_cap) {
                idx_cap *= 2;
                indices = realloc(indices, sizeof(ASTNode*) * idx_cap);
            }
            indices[idx_cnt++] = idx;
        }
        parser_eat(parser, TOKEN_ASSIGN);
        ASTNode* value = parser_Binary(parser);
        AST_ArrayAccess* acc = malloc(sizeof(AST_ArrayAccess));
        acc->base.type = AST_ARRAY_ACCESS;
        acc->array = parser_create_literal(name_token);
        acc->indices = indices;
        acc->index_count = idx_cnt;
        acc->elementType = DATA_ERROR;
        acc->dim_count = 0;
        acc->dim_sizes = NULL;
        acc->full_dim_sizes = NULL;
        acc->full_dim_count = 0;
        AST_ArrayAss* ass = malloc(sizeof(AST_ArrayAss));
        ass->base.type = AST_ARRAY_ASS;
        ass->target = (ASTNode*)acc;
        ass->value = value;
        ass->elementType = DATA_ERROR;
        ass->base.next = NULL;
        acc->base.next = NULL;
        return (ASTNode*)ass;
    }

    AST_Var* ast_var = calloc(1, sizeof(AST_Var));
    if (ast_var == NULL)
    {
        OUT_OF_MEMORY();
    }

    ast_var->base.next = NULL;
    ast_var->base.type = AST_VAR_ASS;
    ast_var->base.value = name_token;

    ast_var->name = name_token;
    parser_eat(parser, TOKEN_ASSIGN);
    ast_var->Value = parser_Binary(parser);
    ast_var->type = ast_var->Value->value.type;
    ast_var->dataType = DATA_ERROR;
    ast_var->internal_name = NULL;

    return (ASTNode*)ast_var;
}

ASTNode* parser_var(Parser* parser, bool allow_multi_decl)
{
    Token type_token = parser->current;

    if (!is_var_type(type_token.type)) {
        ERROR(ERR_PARSE_INVALID_TYPE,
              LOC(parser->filename, parser->current.line, parser->current.column),
              "expected type keyword, got '%s'",
              type_to_string(type_token.type));
        return NULL;
    }

    parser_advance(parser);

    int type_dim_count = 0;
    int type_dim_capacity = 4;
    int* type_dim_sizes = malloc(sizeof(int) * type_dim_capacity);

    while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
        parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);

        int size = -1;
        if (parser_current_is_type(parser, TOKEN_NUMBER)) {
            size = atoi(parser->current.value);
            parser_eat(parser, TOKEN_NUMBER);
        } else if (!parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_RIGHT)) {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected array size or ']'");
            free(type_dim_sizes);
            return NULL;
        }

        parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);

        if (type_dim_count >= type_dim_capacity) {
            type_dim_capacity *= 2;
            type_dim_sizes = realloc(type_dim_sizes, sizeof(int) * type_dim_capacity);
        }
        type_dim_sizes[type_dim_count++] = size;
    }

    ASTNode* head = NULL;
    ASTNode* tail = NULL;

    do {
        if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected variable name after type");
            free(type_dim_sizes);
            AST_Free(head);
            return NULL;
        }

        Token var_name_token = parser->current;
        char* var_name = strdup(var_name_token.value);
        parser_advance(parser);

        int var_dim_count = 0;
        int var_dim_capacity = 4;
        int* var_dim_sizes = malloc(sizeof(int) * var_dim_capacity);

        while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
            parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);

            int size = -1;
            if (parser_current_is_type(parser, TOKEN_NUMBER)) {
                size = atoi(parser->current.value);
                parser_eat(parser, TOKEN_NUMBER);
            } else if (!parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_RIGHT)) {
                ERROR(ERR_PARSE_EXPECTED_TOKEN,
                      LOC(parser->filename, parser->current.line, parser->current.column),
                      "expected array size or ']'");
                free(var_dim_sizes);
                free(var_name);
                free(type_dim_sizes);
                AST_Free(head);
                return NULL;
            }

            parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);

            if (var_dim_count >= var_dim_capacity) {
                var_dim_capacity *= 2;
                var_dim_sizes = realloc(var_dim_sizes, sizeof(int) * var_dim_capacity);
            }
            var_dim_sizes[var_dim_count++] = size;
        }

        int total_dim_count = type_dim_count + var_dim_count;
        int* total_dim_sizes = NULL;
        if (total_dim_count > 0) {
            total_dim_sizes = malloc(sizeof(int) * total_dim_count);
            memcpy(total_dim_sizes, type_dim_sizes, sizeof(int) * type_dim_count);
            memcpy(total_dim_sizes + type_dim_count, var_dim_sizes, sizeof(int) * var_dim_count);
        }
        free(var_dim_sizes);

        ASTNode* init = NULL;
        if (parser_current_is_type(parser, TOKEN_ASSIGN)) {
            parser_eat(parser, TOKEN_ASSIGN);
            if (parser_current_is_type(parser, TOKEN_BRACKET_LEFT))
                init = parser_init_list(parser);
            else
                init = parser_Binary(parser);
        }

        ASTNode* node = NULL;

        if (total_dim_count > 0) {
            bool has_unknown = false;
            for (int i = 0; i < total_dim_count; i++) {
                if (total_dim_sizes[i] == -1) {
                    has_unknown = true;
                    break;
                }
            }

            AST_ArrayDecl* arr = calloc(1, sizeof(AST_ArrayDecl));
            if (!arr) { OUT_OF_MEMORY(); }
            arr->base.type = AST_ARRAY_DECL;
            arr->base.next = NULL;
            arr->base.value = type_token;
            arr->elementType = Get_Datatype(type_token.type);
            arr->name = var_name;
            arr->dim_sizes = total_dim_sizes;
            arr->dim_count = total_dim_count;
            arr->internal_name = NULL;
            arr->init_list = init;
            arr->modifier = 0;
            node = (ASTNode*)arr;
        } else {
            free(total_dim_sizes);

            AST_Var* var = calloc(1, sizeof(AST_Var));
            if (!var) { OUT_OF_MEMORY(); }
            var->base.type = (init != NULL) ? AST_VAR : AST_UNINIT_VAR;
            var->base.next = NULL;
            var->base.value = type_token;
            var->type = type_token.type;
            var->name = var_name_token;
            var->Value = init;
            var->dataType = DATA_ERROR;
            var->internal_name = NULL;
            var->dim_count = 0;
            var->dim_sizes = NULL;
            var->modifier = 0;
            node = (ASTNode*)var;
        }

        if (!head) {
            head = tail = node;
        } else {
            tail->next = node;
            tail = node;
        }

        if (!allow_multi_decl)
            break;
        if (!parser_current_is_type(parser, TOKEN_COMMA))
            break;

        parser_advance(parser);

    } while (true);

    free(type_dim_sizes);
    return head;
}

ASTNode* parser_Print(Parser* parser)
{
    AST_Print* ast_print = malloc(sizeof(AST_Print));
    ast_print->base.next = NULL;
    ast_print->base.type = AST_PRINT;
    ast_print->base.value = parser->current;
    parser_eat(parser, TOKEN_PRINT);

    ASTNode* head_Parament = parser_Binary(parser);
    ASTNode* temp = head_Parament;
    while (!parser_current_is_type(parser, TOKEN_SEMICOLON) &&
        !parser_current_is_type(parser, TOKEN_EOF) && parser_current_is_type(parser, TOKEN_COMMA))
    {
        parser_eat(parser, TOKEN_COMMA);
        ASTNode* Temp = parser_Binary(parser);
        temp->next = Temp;
        temp = temp->next;
    }
    ast_print->Parameter = head_Parament;
    return (ASTNode*)ast_print;
}

ASTNode* parser_input(Parser* parser) {
    parser_eat(parser, TOKEN_INPUT);
    parser_eat(parser, TOKEN_PAREN_LEFT);

    ASTNode* fmt = parser_Binary(parser);
    parser_eat(parser, TOKEN_COMMA);


    ASTNode** args = NULL;
    int arg_count = 0, arg_cap = 0;
    while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT)) {
        if (parser_current_is_type(parser, TOKEN_BITWISE_AND)) {
            parser_advance(parser);
        } else {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected '&' before variable");
            break;
        }
        if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected variable name");
            break;
        }
        AST_Literal* var = calloc(1, sizeof(AST_Literal));
        var->base.type = AST_LITERAL;
        var->base.value = parser->current;
        var->base.value.value = strdup(parser->current.value);
        var->dataType = DATA_IDENTIFIER;
        parser_advance(parser);

        if (arg_count >= arg_cap) {
            arg_cap = arg_cap ? arg_cap * 2 : 4;
            args = realloc(args, sizeof(ASTNode*) * arg_cap);
        }
        args[arg_count++] = (ASTNode*)var;
        if (!parser_current_is_type(parser, TOKEN_COMMA)) break;
        parser_eat(parser, TOKEN_COMMA);
    }
    parser_eat(parser, TOKEN_PAREN_RIGHT);

    AST_Input* input = calloc(1, sizeof(AST_Input));
    input->base.type = AST_INPUT;
    input->format_string = fmt;
    input->args = args;
    input->arg_count = arg_count;
    return (ASTNode*)input;
}

int* parse_dim_sizes(Parser* parser, int* count, DataType elemType) {
    int capacity = 4;
    int* sizes = malloc(sizeof(int) * capacity);
    *count = 0;
    while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
        parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
        int size = -1;
        if (parser_current_is_type(parser, TOKEN_NUMBER)) {
            size = atoi(parser->current.value);
            parser_eat(parser, TOKEN_NUMBER);
        } else if (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_RIGHT)) {
            size = -1;
        } else {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected array size or ']'");
            free(sizes);
            return NULL;
        }
        parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);
        if (*count >= capacity) {
            capacity *= 2;
            sizes = realloc(sizes, sizeof(int) * capacity);
        }
        sizes[(*count)++] = size;
    }
    return sizes;
}

ASTNode* parser_init_list(Parser* parser) {
    parser_eat(parser, TOKEN_BRACKET_LEFT);

    ASTNode** elems = NULL;
    int count = 0;
    int capacity = 0;

    if (!parser_current_is_type(parser, TOKEN_BRACKET_RIGHT)) {
        do {
            ASTNode* elem;
            if (parser_current_is_type(parser, TOKEN_BRACKET_LEFT))
                elem = parser_init_list(parser);
            else
                elem = parser_Binary(parser);

            if (count >= capacity) {
                capacity = (capacity == 0) ? 4 : capacity * 2;
                elems = realloc(elems, sizeof(ASTNode*) * capacity);
            }
            elems[count++] = elem;

        } while (parser_current_is_type(parser, TOKEN_COMMA) &&
                 (parser_advance(parser), 1));
    }

    parser_eat(parser, TOKEN_BRACKET_RIGHT);

    AST_InitList* list = calloc(1, sizeof(AST_InitList));
    list->base.type = AST_INIT_LIST;
    list->elements = elems;
    list->count = count;
    return (ASTNode*)list;
}

static ASTNode* parser_ArrayAccessSuffix(Parser* parser, ASTNode* left) {
    int idx_cap = 4, idx_cnt = 0;
    ASTNode** indices = malloc(sizeof(ASTNode*) * idx_cap);

    while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
        parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
        ASTNode* idx = parser_Binary(parser);
        if (idx == NULL) {
            free(indices);
            parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);
            return NULL;
        }
        parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);

        if (idx_cnt >= idx_cap) {
            idx_cap *= 2;
            indices = realloc(indices, sizeof(ASTNode*) * idx_cap);
        }
        indices[idx_cnt++] = idx;
    }

    if (idx_cnt > 0) {
        AST_ArrayAccess* acc = malloc(sizeof(AST_ArrayAccess));
        acc->base.type = AST_ARRAY_ACCESS;
        acc->base.next = NULL;
        acc->base.value = (Token){NULL, TOKEN_NONE, 0, 0};
        acc->array = left;
        acc->indices = indices;
        acc->index_count = idx_cnt;
        acc->elementType = DATA_ERROR;
        acc->dim_count = 0;
        acc->dim_sizes = NULL;
        acc->full_dim_count = 0;
        acc->full_dim_sizes = NULL;
        return (ASTNode*)acc;
    } else {
        free(indices);
        return left;
    }
}

ASTNode* parser_Postfix(Parser* parser, ASTNode* left) {
    while (1) {
        if (parser_current_is_type(parser, TOKEN_DOT)) {
            parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_IDENTIFIER) &&
                strcmp(parser->current.value, "length") == 0) {
                parser_advance(parser);
                AST_ArrayLength* len = calloc(1, sizeof(AST_ArrayLength));
                len->base.type = AST_ARRAY_LENGTH;
                len->array = left;
                len->base.next = NULL;
                len->base.value = (Token){NULL, TOKEN_NONE, 0, 0};
                left = (ASTNode*)len;
            } else if (parser_current_is_type(parser, TOKEN_IDENTIFIER) &&
                       strcmp(parser->current.value, "toString") == 0) {
                parser_advance(parser);
                parser_eat(parser, TOKEN_PAREN_LEFT);
                parser_eat(parser, TOKEN_PAREN_RIGHT);
                AST_Literal* call = calloc(1, sizeof(AST_Literal));
                call->base.type = AST_LITERAL;
                call->base.next = NULL;
                call->base.value = (Token){NULL, TOKEN_NONE, 0, 0};
                call->base.value.type = TOKEN_IDENTIFIER;
                call->base.value.value = strdup("_arrayToString");
                call->dataType = DATA_STRING;
                call->internal_name = strdup("_arrayToString");
                call->is_call_function = true;
                call->call_function_Parameter = left;
                call->call_function_ParameterCount = 1;
                left = (ASTNode*)call;
            } else {
                char* member = strdup(parser->current.value);
                parser_advance(parser);

                AST_MemberAccess* access = calloc(1, sizeof(AST_MemberAccess));
                access->base.type = AST_MEMBER_ACCESS;
                access->object = left;
                access->member_name = member;
                access->base.next = NULL;
                access->base.value = (Token){NULL, TOKEN_NONE, 0, 0};

                left = (ASTNode*)access;
                if (parser_current_is_type(parser, TOKEN_PAREN_LEFT)) {
                    parser_eat(parser, TOKEN_PAREN_LEFT);

                    ASTNode* args_head = NULL;
                    ASTNode* args_tail = NULL;
                    int arg_count = 0;

                    if (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT)) {
                        do {
                            ASTNode* arg = parser_Binary(parser);
                            arg_count++;
                            if (!args_head) { args_head = arg; args_tail = arg; }
                            else { args_tail->next = arg; args_tail = arg; }
                        } while (parser_current_is_type(parser, TOKEN_COMMA) && (parser_advance(parser), 1));
                    }
                    parser_eat(parser, TOKEN_PAREN_RIGHT);

                    AST_Literal* call = calloc(1, sizeof(AST_Literal));
                    call->base.type = AST_LITERAL;
                    call->base.next = NULL;
                    call->base.value = (Token){access->member_name ? strdup(access->member_name) : NULL,
                                               TOKEN_IDENTIFIER,
                                               parser->current.line, parser->current.column};
                    call->base.value = call->base.value;
                    call->dataType = DATA_IDENTIFIER;
                    call->internal_name = NULL;
                    call->is_call_function = true;

                    call->method_arguments = args_head;
                    call->method_arg_count = arg_count;

                    call->call_function_Parameter = (ASTNode*)access;
                    call->call_function_ParameterCount = arg_count + 1;

                    left = (ASTNode*)call;
                }
            }
        } else if (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
            int idx_cap = 4, idx_cnt = 0;
            ASTNode** indices = malloc(sizeof(ASTNode*) * idx_cap);

            while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
                parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
                ASTNode* idx = parser_Binary(parser);
                if (idx == NULL) {
                    free(indices);
                    parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);
                    return NULL;
                }
                parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);

                if (idx_cnt >= idx_cap) {
                    idx_cap *= 2;
                    indices = realloc(indices, sizeof(ASTNode*) * idx_cap);
                }
                indices[idx_cnt++] = idx;
            }

            if (idx_cnt > 0) {
                AST_ArrayAccess* acc = malloc(sizeof(AST_ArrayAccess));
                acc->base.type = AST_ARRAY_ACCESS;
                acc->base.next = NULL;
                acc->base.value = (Token){NULL, TOKEN_NONE, 0, 0};
                acc->array = left;
                acc->indices = indices;
                acc->index_count = idx_cnt;
                acc->elementType = DATA_ERROR;
                acc->dim_count = 0;
                acc->dim_sizes = NULL;
                acc->full_dim_count = 0;
                acc->full_dim_sizes = NULL;
                left = (ASTNode*)acc;
            } else {
                free(indices);
                return left;
            }
        } else if (parser_current_is_type(parser, TOKEN_PAREN_LEFT)) {
            if (left->type != AST_LITERAL || ((AST_Literal*)left)->is_call_function) {
                break;
            }
            AST_Literal* func_lit = (AST_Literal*)left;

            parser_eat(parser, TOKEN_PAREN_LEFT);
            func_lit->is_call_function = true;

            ASTNode* head = NULL;
            ASTNode* tail = NULL;
            int count = 0;

            while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT) &&
                   !parser_current_is_type(parser, TOKEN_EOF)) {
                ASTNode* arg = parser_Binary(parser);
                count++;

                if (head == NULL) {
                    head = arg;
                    head->next = NULL;
                    tail = arg;
                } else {
                    tail->next = arg;
                    tail = arg;
                    tail->next = NULL;
                }

                if (!parser_current_is_type(parser, TOKEN_COMMA)) {
                    break;
                }
                parser_eat(parser, TOKEN_COMMA);
            }

            parser_eat(parser, TOKEN_PAREN_RIGHT);

            func_lit->call_function_Parameter = head;
            func_lit->call_function_ParameterCount = count;
        } else if (parser_current_is_type(parser, TOKEN_INCREMENT)) {
            Token op = parser->current;
            parser_advance(parser);
            AST_Unary* unary = malloc(sizeof(AST_Unary));
            unary->base.type = AST_UNARY;
            unary->base.next = NULL;
            unary->base.value = op;
            unary->op = op;
            unary->right = left;
            unary->dataType = DATA_ERROR;
            left = (ASTNode*)unary;
        } else if (parser_current_is_type(parser, TOKEN_DECREMENT))
        {
            Token op = parser->current;
            parser_advance(parser);
            AST_Unary* unary = malloc(sizeof(AST_Unary));
            unary->base.type = AST_UNARY;
            unary->base.next = NULL;
            unary->base.value = op;
            unary->op = op;
            unary->right = left;
            unary->dataType = DATA_ERROR;
            left = (ASTNode*)unary;
        } else {
            break;
        }
    }
    return left;
}

static ASTNode* parser_primary(Parser* parser)
{
    if (parser->current.type == TOKEN_PAREN_LEFT) {
        parser_eat(parser, TOKEN_PAREN_LEFT);
        ASTNode* node = parser_Binary(parser);
        parser_eat(parser, TOKEN_PAREN_RIGHT);
        return node;
    }

    if (parser->current.type == TOKEN_THIS) {
        parser_advance(parser);
        AST_This* this_node = calloc(1, sizeof(AST_This));
        this_node->base.type = AST_THIS;
        this_node->base.next = NULL;
        this_node->base.value = (Token){NULL, TOKEN_NONE,
                                        parser->current.line, parser->current.column};
        return (ASTNode*)this_node;
    }

    if (parser->current.type == TOKEN_NEW)
    {
        parser_advance(parser);

        if (is_var_type(parser->current.type)) {
            DataType elemType = Get_Datatype(parser->current.type);
            parser_advance(parser);

            int dim_cap = 4, dim_cnt = 0;
            int* dim_sizes = malloc(sizeof(int) * dim_cap);
            ASTNode** dim_exprs = calloc(dim_cap, sizeof(ASTNode*));
            if (!dim_sizes || !dim_exprs) OUT_OF_MEMORY();

            while (parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_LEFT)) {
                parser_eat(parser, TOKEN_SQUARE_BRACKET_LEFT);
                int size = -1;
                ASTNode* expr = NULL;
                if (parser_current_is_type(parser, TOKEN_NUMBER)) {
                    size = atoi(parser->current.value);
                    parser_eat(parser, TOKEN_NUMBER);
                } else if (!parser_current_is_type(parser, TOKEN_SQUARE_BRACKET_RIGHT)) {
                    expr = parser_Binary(parser);
                    if (expr == NULL) {
                        free(dim_sizes);
                        free(dim_exprs);
                        return NULL;
                    }
                    if (expr->type == AST_LITERAL &&
                        ((AST_Literal*)expr)->base.value.type == TOKEN_NUMBER) {
                        size = atoi(((AST_Literal*)expr)->base.value.value);
                    }
                }
                parser_eat(parser, TOKEN_SQUARE_BRACKET_RIGHT);

                if (dim_cnt >= dim_cap) {
                    dim_cap *= 2;
                    dim_sizes = realloc(dim_sizes, sizeof(int) * dim_cap);
                    dim_exprs = realloc(dim_exprs, sizeof(ASTNode*) * dim_cap);
                    if (!dim_sizes || !dim_exprs) OUT_OF_MEMORY();
                }
                dim_sizes[dim_cnt] = size;
                dim_exprs[dim_cnt] = expr;
                dim_cnt++;
            }

            AST_NewArray* newArr = calloc(1, sizeof(AST_NewArray));
            newArr->base.type = AST_NEW_ARRAY;
            newArr->elementType = elemType;
            newArr->dim_count = dim_cnt;
            newArr->dim_sizes = dim_sizes;
            newArr->dim_exprs = dim_exprs;
            return (ASTNode*)newArr;
        }
        else if (parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            char* class_name = strdup(parser->current.value);
            parser_advance(parser);
            parser_eat(parser, TOKEN_PAREN_LEFT);

            ASTNode* args_head = NULL;
            ASTNode* args_tail = NULL;
            int arg_count = 0;
            if (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT)) {
                do {
                    ASTNode* arg = parser_Binary(parser);
                    if (!arg) {
                        while (!parser_current_is_type(parser, TOKEN_PAREN_RIGHT)
                               && !parser_current_is_type(parser, TOKEN_EOF))
                            parser_advance(parser);
                        break;
                    }
                    arg_count++;
                    if (!args_head) {
                        args_head = arg;
                        args_tail = arg;
                    } else {
                        args_tail->next = arg;
                        args_tail = arg;
                    }
                } while (parser_current_is_type(parser, TOKEN_COMMA)
                         && (parser_advance(parser), 1));
            }
            parser_eat(parser, TOKEN_PAREN_RIGHT);
            AST_NewInstance* new_inst = calloc(1, sizeof(AST_NewInstance));
            new_inst->base.type = AST_NEW_INSTANCE;
            new_inst->class_name = class_name;
            new_inst->arguments = args_head;
            new_inst->has_constructor = false;
            new_inst->constructor_name = NULL;
            new_inst->constructor_arg_count = arg_count;
            return (ASTNode*)new_inst;
        }
        else {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected type or class name after 'new'");
            return NULL;
        }
    }

    AST_Literal* lit = malloc(sizeof(AST_Literal));
    lit->base.type = AST_LITERAL;
    lit->base.next = NULL;

    if (parser->current.type == TOKEN_NUMBER ||
        parser->current.type == TOKEN_STRING_LITERAL ||
        parser->current.type == TOKEN_CHAR_LITERAL ||
        parser->current.type == TOKEN_BOOL_LITERAL ||
        parser->current.type == TOKEN_FLOAT_LITERAL ||
        parser->current.type == TOKEN_IDENTIFIER) {

        if (parser->current.type == TOKEN_BOOL_LITERAL) {
            if (strcmp(parser->current.value, "true") == 0) {
                lit->base.value = (Token){strdup("1"), TOKEN_NUMBER,
                                           parser->current.line, parser->current.column};
            } else {
                lit->base.value = (Token){strdup("0"), TOKEN_NUMBER,
                                           parser->current.line, parser->current.column};
            }
        } else
        {
            lit->base.value = parser->current;
        }

        lit->dataType = Get_Datatype(parser->current.type);
        lit->internal_name = NULL;
        lit->is_call_function = false;
        lit->call_function_Parameter = NULL;
        lit->call_function_ParameterCount = 0;
        lit->method_arguments = NULL;
        lit->method_arg_count = 0;
        lit->is_field_ref = false;
        lit->field_offset = 0;
        parser_advance(parser);
        return (ASTNode*)lit;
        }

    ERROR(ERR_PARSE_ILLEGAL_OPERAND,
          LOC(parser->filename, parser->current.line, parser->current.column),
          "illegal operand '%s'",
          parser->current.value);
    AST_Literal* error_ast_node = malloc(sizeof(AST_Literal));
    error_ast_node->base.value = parser->current;
    error_ast_node->base.type = AST_ERROR;
    error_ast_node->base.next = NULL;
    error_ast_node->base.value = parser->current;
    error_ast_node->is_field_ref = false;
    error_ast_node->field_offset = 0;
    parser_advance(parser);
    return (ASTNode*)error_ast_node;
}

ASTNode* parser_Unary(Parser* parser) {
    if (parser_current_is_type(parser, TOKEN_SUB) || parser_current_is_type(parser, TOKEN_OPPOSE) ||
    parser_current_is_type(parser, TOKEN_BITWISE_NOT) || parser_current_is_type(parser, TOKEN_INCREMENT) ||
    parser_current_is_type(parser, TOKEN_DECREMENT)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Unary(parser);

        AST_Unary* unary = malloc(sizeof(AST_Unary));
        unary->base.type = AST_UNARY;
        unary->base.next = NULL;
        unary->base.value = op;
        unary->op = op;
        unary->right = right;
        unary->dataType = DATA_ERROR;
        return (ASTNode*)unary;
    }

    ASTNode* atom = parser_primary(parser);
    if (!atom) return NULL;
    return parser_Postfix(parser, atom);
}

static ASTNode* parser_compound_assign_or_incdec(Parser* parser)
{
    Token var_token = parser->current;
    parser_advance(parser);
    Token op_token = parser->current;
    parser_advance(parser);

    ASTNode* right_expr = NULL;

    if (op_token.type == TOKEN_INCREMENT || op_token.type == TOKEN_DECREMENT) {
        AST_Literal* one = calloc(1, sizeof(AST_Literal));
        one->base.type = AST_LITERAL;
        one->base.value.value = strdup("1");
        one->base.value.type = TOKEN_NUMBER;
        one->dataType = DATA_INT;
        one->is_call_function = false;

        AST_Literal* var_expr = calloc(1, sizeof(AST_Literal));
        var_expr->base.type = AST_LITERAL;
        var_expr->base.value = var_token;
        var_expr->base.value.value = strdup(var_token.value);
        var_expr->dataType = DATA_IDENTIFIER;
        var_expr->is_call_function = false;

        AST_Binary* bin = calloc(1, sizeof(AST_Binary));
        bin->base.type = AST_BINARY;
        bin->left = (ASTNode*)var_expr;
        bin->op.type = (op_token.type == TOKEN_INCREMENT) ? TOKEN_ADD : TOKEN_SUB;
        bin->op.value = strdup((bin->op.type == TOKEN_ADD) ? "+" : "-");
        bin->right = (ASTNode*)one;
        bin->resultType = DATA_ERROR;
        right_expr = (ASTNode*)bin;
    } else {
        ASTNode* expr = parser_Binary(parser);

        AST_Literal* var_expr = calloc(1, sizeof(AST_Literal));
        var_expr->base.type = AST_LITERAL;
        var_expr->base.value = var_token;
        var_expr->base.value.value = strdup(var_token.value);
        var_expr->dataType = DATA_IDENTIFIER;
        var_expr->is_call_function = false;

        TokenType bin_type;
        switch (op_token.type) {
            case TOKEN_ADD_ASSIGN: bin_type = TOKEN_ADD; break;
            case TOKEN_SUB_ASSIGN: bin_type = TOKEN_SUB; break;
            case TOKEN_MUL_ASSIGN: bin_type = TOKEN_MUL; break;
            case TOKEN_DIV_ASSIGN: bin_type = TOKEN_DIV; break;
            case TOKEN_MOD_ASSIGN: bin_type = TOKEN_MOD; break;
            case TOKEN_SHIFT_LEFT_ASSIGN:  bin_type = TOKEN_SHIFT_LEFT;  break;
            case TOKEN_SHIFT_RIGHT_ASSIGN: bin_type = TOKEN_SHIFT_RIGHT; break;
            case TOKEN_BITWISE_XOR_ASSIGN: bin_type = TOKEN_BITWISE_XOR; break;
            default: bin_type = TOKEN_ADD;
        }

        AST_Binary* bin = calloc(1, sizeof(AST_Binary));
        bin->base.type = AST_BINARY;
        bin->left = (ASTNode*)var_expr;
        bin->op.type = bin_type;
        bin->op.value = strdup(type_to_string(bin_type));
        bin->right = expr;
        bin->resultType = DATA_ERROR;
        right_expr = (ASTNode*)bin;
    }

    AST_Var* assign = calloc(1, sizeof(AST_Var));
    assign->base.type = AST_VAR_ASS;
    assign->base.value = var_token;
    assign->name = var_token;
    assign->Value = right_expr;
    assign->type = var_token.type;
    assign->dataType = DATA_ERROR;
    assign->internal_name = NULL;

    return (ASTNode*)assign;
}

ASTNode* parser_Mul_Div(Parser* parser)
{
    ASTNode* left = parser_Unary(parser);
    while (parser_current_is_type(parser, TOKEN_MUL) || parser_current_is_type(parser, TOKEN_DIV) || parser_current_is_type(parser, TOKEN_MOD))
    {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Unary(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

ASTNode* parser_Add_Sub(Parser* parser)
{
    ASTNode* left = parser_Mul_Div(parser);
    while (parser_current_is_type(parser, TOKEN_ADD) || parser_current_is_type(parser, TOKEN_SUB))
    {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Mul_Div(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_Shift(Parser* parser)
{
    ASTNode* left = parser_Add_Sub(parser);
    while (parser_current_is_type(parser, TOKEN_SHIFT_LEFT) ||
           parser_current_is_type(parser, TOKEN_SHIFT_RIGHT))
    {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Add_Sub(parser);
        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left  = left;
        binary->op    = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_BitwiseAnd(Parser* parser) {
    ASTNode* left = parser_Shift(parser);
    while (parser_current_is_type(parser, TOKEN_BITWISE_AND)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Shift(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_BitwiseXor(Parser* parser) {
    ASTNode* left = parser_BitwiseAnd(parser);
    while (parser_current_is_type(parser, TOKEN_BITWISE_XOR)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_BitwiseAnd(parser);
        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left  = left;
        binary->op    = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_BitwiseOr(Parser* parser) {
    ASTNode* left = parser_BitwiseXor(parser);
    while (parser_current_is_type(parser, TOKEN_BITWISE_OR)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_BitwiseXor(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

ASTNode* parser_Logic(Parser* parser)
{
    ASTNode* left = parser_BitwiseOr(parser);
    while (parser_current_is_type(parser, TOKEN_EQ) ||
           parser_current_is_type(parser, TOKEN_NE) ||
           parser_current_is_type(parser, TOKEN_LT) ||
           parser_current_is_type(parser, TOKEN_GT) ||
           parser_current_is_type(parser, TOKEN_LE) ||
           parser_current_is_type(parser, TOKEN_GE))
    {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_BitwiseOr(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_LogicalAnd(Parser* parser) {
    ASTNode* left = parser_Logic(parser);
    while (parser_current_is_type(parser, TOKEN_LOGICAL_AND)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_Logic(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_LogicalOr(Parser* parser) {
    ASTNode* left = parser_LogicalAnd(parser);
    while (parser_current_is_type(parser, TOKEN_LOGICAL_OR)) {
        Token op = parser->current;
        parser_advance(parser);
        ASTNode* right = parser_LogicalAnd(parser);

        AST_Binary* binary = malloc(sizeof(AST_Binary));
        binary->left = left;
        binary->op = op;
        binary->right = right;
        binary->base.next = NULL;
        binary->base.type = AST_BINARY;
        binary->base.value = binary->op;
        left = (ASTNode*)binary;
    }
    return left;
}

static ASTNode* parser_Conditional(Parser* parser) {
    ASTNode* cond = parser_LogicalOr(parser);
    if (parser_current_is_type(parser, TOKEN_QUESTION)) {
        parser_eat(parser, TOKEN_QUESTION);
        ASTNode* then_expr = parser_Binary(parser);
        parser_eat(parser, TOKEN_COLON);
        ASTNode* else_expr = parser_Conditional(parser);
        AST_Conditional* tern = calloc(1, sizeof(AST_Conditional));
        if (!tern) OUT_OF_MEMORY();
        tern->base.type  = AST_CONDITIONAL;
        tern->base.next  = NULL;
        tern->base.value = (Token){ .value = strdup("?:"),
                                    .type  = TOKEN_QUESTION,
                                    .line  = cond->value.line,
                                    .column = cond->value.column };
        tern->condition  = cond;
        tern->then_expr  = then_expr;
        tern->else_expr  = else_expr;
        tern->resultType = DATA_ERROR;
        return (ASTNode*)tern;
    }
    return cond;
}

ASTNode* parser_Binary(Parser* parser)
{
    return parser_Conditional(parser);
}

ASTNode* parser_import(Parser* parser) {
    parser_eat(parser, TOKEN_IMPORT);
    char* module_name = NULL;
    if (parser_current_is_type(parser, TOKEN_STRING_LITERAL)) {
        module_name = strdup(parser->current.value);
        parser_advance(parser);
    }
    else if (parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
        int cap = 64;
        module_name = malloc(cap);
        module_name[0] = '\0';
        while (parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            if (strlen(module_name) > 0) strcat(module_name, ".");
            strcat(module_name, parser->current.value);
            parser_advance(parser);
            if (parser_current_is_type(parser, TOKEN_DOT)) {
                parser_advance(parser);
                if (!parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
                    ERROR(ERR_PARSE_EXPECTED_TOKEN,
                          LOC(parser->filename, parser->current.line, parser->current.column),
                          "expected module name after '.'");
                    free(module_name);
                    return NULL;
                }
            } else {
                break;
            }
        }
    } else {
        ERROR(ERR_PARSE_EXPECTED_TOKEN,
              LOC(parser->filename, parser->current.line, parser->current.column),
              "expected module name after 'import'");
        return NULL;
    }
    char* alias = NULL;
    if (parser_current_is_type(parser, TOKEN_AS)) {
        parser_advance(parser);
        if (parser_current_is_type(parser, TOKEN_IDENTIFIER)) {
            alias = strdup(parser->current.value);
            parser_advance(parser);
        } else {
            ERROR(ERR_PARSE_EXPECTED_TOKEN,
                  LOC(parser->filename, parser->current.line, parser->current.column),
                  "expected alias name after 'as'");
            free(module_name);
            return NULL;
        }
    }
    char* abs_path = resolve_module(module_name);
    if (!abs_path) {
        ERROR(ERR_PARSE_MODULE_NOT_FOUND,
              LOC(parser->filename, 0, 0),
              "module '%s' not found",
              module_name);
        free(module_name);
        free(alias);
        return NULL;
    }
    if (is_already_imported(abs_path)) {
        free(module_name);
        free(alias);
        free(abs_path);
        return NULL;
    }
    mark_imported(abs_path);
    char* source = read_source(abs_path);
    if (!source) {
        ERROR(ERR_PARSE_MODULE_NOT_FOUND,
              LOC(parser->filename, 0, 0),
              "failed to read module '%s'",
              module_name);
        free(module_name);
        free(alias);
        free(abs_path);
        return NULL;
    }
    Lexer* lex = lexer(source, abs_path);
    Parser* sub_parser = parser_init(lex->tokens, abs_path);
    ASTNode* imported_ast = parser_parse(sub_parser);

    free(module_name);
    free(alias);

    return imported_ast;
}

char* AST_Type_to_String(ASTType type)
{
    switch (type)
    {
    case AST_LITERAL:
        return "literal";
    case AST_BINARY:
        return "binary";
    case AST_PRINT:
        return "print";
    case AST_UNARY:
        return "unary";
    case AST_VAR:
        return "var";
    case AST_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void Print_AST(ASTNode* node, int indent, bool after_sem) {
    while (node != NULL) {
        for (int i = 0; i < indent; i++) {
            printf("  ");
        }

        switch (node->type) {
        case AST_LITERAL: {
                AST_Literal* literal = (AST_Literal*)node;
                printf("LITERAL(type: %s,%s): %s\n",
                       after_sem ? data_type_to_string(literal->dataType) : type_to_string(literal->base.value.type),
                       literal->is_call_function ? "is call function" : "not a call function",
                       literal->base.value.value ? literal->base.value.value : "(null)");

                if (literal->is_call_function && literal->call_function_Parameter) {
                    for (int i = 0; i < indent + 1; i++) printf("  ");
                    printf("Call target:\n");
                    Print_AST(literal->call_function_Parameter, indent + 2, after_sem);
                }

                if (literal->is_call_function && literal->method_arguments) {
                    for (int i = 0; i < indent + 1; i++) printf("  ");
                    printf("Method arguments (%d):\n", literal->method_arg_count);
                    Print_AST(literal->method_arguments, indent + 2, after_sem);
                }
                break;
        }


        case AST_BINARY: {
                AST_Binary* binary = (AST_Binary*)node;
                printf("BINARY_EXPR(type: %s):\n",
                       after_sem ? data_type_to_string(binary->resultType) : "unknown");
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Operator: %s (line %d, col %d)\n",
                       type_to_string(binary->op.type), binary->op.line, binary->op.column);
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Left:\n");
                if (binary->left) Print_AST(binary->left, indent + 2, after_sem);
                else { for (int i = 0; i < indent+2;i++) printf("  "); printf("(NULL)\n"); }
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Right:\n");
                if (binary->right) Print_AST(binary->right, indent + 2, after_sem);
                else { for (int i = 0; i < indent+2;i++) printf("  "); printf("(NULL)\n"); }
                break;
        }

        case AST_UNARY: {
                AST_Unary* unary = (AST_Unary*)node;
                if (after_sem) printf("UNARY_EXPR(type:%s):\n",data_type_to_string(unary->dataType));
                else printf("UNARY_EXPR(type:%s):\n",type_to_string(unary->base.type));
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Operator: %s\n", type_to_string(unary->op.type));
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Operand:\n");
                if (unary->right) Print_AST(unary->right, indent + 2, after_sem);
                else { for (int i = 0; i < indent+2;i++) printf("  "); printf("(NULL)\n"); }
                break;
        }

        case AST_CONDITIONAL: {
                AST_Conditional* tern = (AST_Conditional*)node;
                printf("CONDITIONAL(result: %s):\n",
                       after_sem ? data_type_to_string(tern->resultType) : "unknown");
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Condition:\n");
                if (tern->condition) Print_AST(tern->condition, indent + 2, after_sem);
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Then:\n");
                if (tern->then_expr) Print_AST(tern->then_expr, indent + 2, after_sem);
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Else:\n");
                if (tern->else_expr) Print_AST(tern->else_expr, indent + 2, after_sem);
                break;
        }

        case AST_UNINIT_VAR: {
                AST_Var* var = (AST_Var*)node;
                printf("UNINIT_VAR(type: %s):\n",
                       after_sem ? data_type_to_string(var->dataType) : type_to_string(var->type));
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("name: %s\n", var->name.value ? var->name.value : "(null)");
                if (var->dim_count > 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("dims: [");
                    for (int i = 0; i < var->dim_count; i++) {
                        if (i > 0) printf(", ");
                        printf("%d", var->dim_sizes ? var->dim_sizes[i] : -1);
                    }
                    printf("]\n");
                }
                break;
        }

        case AST_VAR:
        case AST_VAR_ASS: {
                AST_Var* var = (AST_Var*)node;
                printf("%s(type: %s):\n",
                       node->type == AST_VAR ? "VARIABLE" : "VAR_ASS",
                       after_sem ? data_type_to_string(var->dataType) : type_to_string(var->type));
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("name: %s\n", var->name.value ? var->name.value : "(null)");
                if (var->Value != NULL) {
                    for (int i = 0; i < indent + 1; i++) printf("  ");
                    printf("Value:\n");
                    Print_AST(var->Value, indent + 2, after_sem);
                } else {
                    for (int i = 0; i < indent + 1; i++) printf("  ");
                    printf("Value: (uninitialized)\n");
                }
                if (var->dim_count > 0) {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("dims: [");
                    for (int i = 0; i < var->dim_count; i++) {
                        if (i > 0) printf(", ");
                        printf("%d", var->dim_sizes ? var->dim_sizes[i] : -1);
                    }
                    printf("]\n");
                }
                break;
        }

        case AST_PRINT: {
                AST_Print* print = (AST_Print*)node;
                printf("PRINT:\n");
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Parameter:\n");
                if (print->Parameter) Print_AST(print->Parameter, indent + 2, after_sem);
                else { for (int i = 0; i < indent+2;i++) printf("  "); printf("(NULL)\n"); }
                break;
        }
        case AST_IF: {
                AST_If* ast_if = (AST_If*)node;
                printf("IF:\n");
                if (ast_if->Condition) Print_AST(ast_if->Condition, indent + 2, after_sem);
                if (ast_if->code) Print_AST(ast_if->code, indent + 2, after_sem);
                printf("else if(count: %d):\n", ast_if->elif_Count);
                for (int i = 0; i < ast_if->elif_Count; i++) {
                    printf("the %d branch\n", i + 1);
                    if (ast_if->elif_condition[i]) Print_AST(ast_if->elif_condition[i], indent + 2, after_sem);
                    if (ast_if->Elif[i]) Print_AST(ast_if->Elif[i], indent + 2, after_sem);
                }
                printf("else:\n");
                if (ast_if->Else) Print_AST(ast_if->Else, indent + 2, after_sem);
                break;
        }

        case AST_WHILE: {
                AST_While* ast_while = (AST_While*)node;
                printf("WHILE:\n");
                if (ast_while->Condition) Print_AST(ast_while->Condition, indent + 2, after_sem);
                if (ast_while->code) Print_AST(ast_while->code, indent + 2, after_sem);
                break;
        }

        case AST_FOR: {
                AST_For* f = (AST_For*)node;
                printf("FOR:\n");
                if (f->init)
                {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Init:\n");
                    Print_AST(f->init, indent+2, after_sem);
                }
                if (f->condition)
                {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Condition:\n");
                    Print_AST(f->condition, indent+2, after_sem);
                }
                if (f->increment)
                {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Increment:\n");
                    Print_AST(f->increment, indent+2, after_sem);
                }
                if (f->code)
                {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Body:\n");
                    Print_AST(f->code, indent+2, after_sem);
                }
                break;
        }

        case AST_FOREACH: {
                AST_ForEach* fe = (AST_ForEach*)node;
                printf("FOREACH (iterator: %s):\n", fe->iterator.value);
                if (after_sem) {
                    printf(", element type: %s", data_type_to_string(fe->elemType));
                    if (fe->has_explicit_type)
                        printf(" [explicit]");
                }
                if (fe->array)
                {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Array:\n");
                    Print_AST(fe->array, indent+2, after_sem);
                }
                if (fe->code)
                {
                    for (int i = 0; i < indent+1; i++) printf("  ");
                    printf("Body:\n");
                    Print_AST(fe->code, indent+2, after_sem);
                }
                break;
        }

        case AST_SWITCH:
        {
            AST_Switch* sw = (AST_Switch*)node;
            printf("SWITCH:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Condition:\n");
            if (sw->condition) Print_AST(sw->condition, indent + 2, after_sem);

            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Cases (%d):\n", sw->case_count);
            for (int i = 0; i < sw->case_count; i++) {
                AST_Case* cs = sw->cases[i];
                for (int j = 0; j < indent + 2; j++) printf("  ");
                printf("Case %d:\n", i + 1);

                if (!cs->is_default && cs->value) {
                    for (int j = 0; j < indent + 3; j++) printf("  ");
                    printf("Value:\n");
                    Print_AST(cs->value, indent + 4, after_sem);
                } else if (cs->is_default) {
                    for (int j = 0; j < indent + 3; j++) printf("  ");
                    printf("(default)\n");
                }

                if (cs->body) {
                    for (int j = 0; j < indent + 3; j++) printf("  ");
                    printf("Body:\n");
                    Print_AST(cs->body, indent + 4, after_sem);
                }
            }

            if (sw->has_default && sw->default_body) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Default:\n");
                Print_AST(sw->default_body, indent + 2, after_sem);
            }
            break;
        }

    case AST_CASE: {
        AST_Case* cs = (AST_Case*)node;
        printf("CASE%s:\n", cs->is_default ? " (default)" : "");
        if (!cs->is_default && cs->value) {
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Value:\n");
            Print_AST(cs->value, indent + 2, after_sem);
        }
        if (cs->body) {
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("Body:\n");
            Print_AST(cs->body, indent + 2, after_sem);
        }
        break;
    }

    case AST_BREAK: {
        AST_Break* brk = (AST_Break*)node;
        printf("BREAK (line %d, col %d)\n",
               brk->base.value.line, brk->base.value.column);
        break;
    }

    case AST_CONTINUE: {
        AST_Continue* cont = (AST_Continue*)node;
        printf("CONTINUE (line %d, col %d)\n",
               cont->base.value.line, cont->base.value.column);
        break;
    }

        case AST_FUNCTION:{
                AST_Function* ast_function = (AST_Function*)node;
                printf("FUNCTION(%s):\n", ast_function->name.value);
                if (ast_function->Parameter != NULL) {
                    printf("parameters:\n");
                    Print_AST(ast_function->Parameter, indent + 2, after_sem);
                } else {
                    printf("no have parameter\n");
                }
                printf("code:\n");
                if (ast_function->Code) Print_AST(ast_function->Code, indent + 2, after_sem);
                else { for (int i = 0; i < indent+2;i++) printf("  "); printf("(NULL)\n"); }
                if (ast_function->Return != NULL) {
                    printf("return:\n");
                    Print_AST((ASTNode*)ast_function->Return, indent + 2, after_sem);
                } else {
                    printf("no have return\n");
                }
                break;
        }

        case AST_CLASS_DEF: {
                AST_ClassDef* cls = (AST_ClassDef*)node;
                printf("CLASS_DEF (name: %s, modifier: %s",
                       cls->name ? cls->name : "(null)",
                       cls->modifier == 0 ? "pub" : cls->modifier == 1 ? "pro" : "pri");
                if (cls->parent_count > 0) {
                    printf(", parents: [");
                    for (int i = 0; i < cls->parent_count; i++) {
                        if (i > 0) printf(", ");
                        printf("%s", cls->parent_names[i]);
                    }
                    printf("]");
                }
                printf("):\n");

                if (cls->members) {
                    for (int i = 0; i < indent + 1; i++) printf("  ");
                    printf("Members:\n");
                    Print_AST(cls->members, indent + 2, after_sem);
                } else {
                    for (int i = 0; i < indent + 1; i++) printf("  ");
                    printf("(no members)\n");
                }
                break;
        }

        case AST_NEW_INSTANCE: {
                AST_NewInstance* ni = (AST_NewInstance*)node;
                printf("NEW_INSTANCE (class: %s, size: %d)\n",
                       ni->class_name ? ni->class_name : "(null)",
                       after_sem ? ni->instance_size : -1);
                break;
        }

        case AST_CLASS_MEMBER:
            {
                printf("class_member");
                break;
            }

        case AST_THIS:
            {
                printf("this\n");
                break;
            }

        case AST_MEMBER_ACCESS: {
                AST_MemberAccess* ma = (AST_MemberAccess*)node;
                printf("MEMBER_ACCESS (member: %s", ma->member_name == NULL ? "(null)" : ma->member_name);
                if (after_sem) {
                    printf(", type: %s", data_type_to_string(ma->member_type));
                    if (ma->is_method) {
                        printf(", method: true");
                    } else {
                        printf(", offset: %d", ma->member_offset);
                    }
                }
                printf("):\n");

                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("Object:\n");
                if (ma->object) {
                    Print_AST(ma->object, indent + 2, after_sem);
                } else {
                    for (int i = 0; i < indent + 2; i++) printf("  ");
                    printf("(NULL)\n");
                }
                break;
        }

        case AST_MEMBER_ASS: {
                AST_MemberAss* ass = (AST_MemberAss*)node;
                printf("MEMBER_ASS (member: %s", ass->member_name);
                if (after_sem) {
                    printf(", type: %s, offset: %d", data_type_to_string(ass->member_type), ass->member_offset);
                }
                printf("):\n");
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("Object:\n");
                if (ass->object) Print_AST(ass->object, indent+2, after_sem);
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("Value:\n");
                if (ass->value) Print_AST(ass->value, indent+2, after_sem);
                break;
        }

        case AST_RETURN:{
            AST_Return* ast_return = (AST_Return*)node;
            printf("RETURN(type:%s):\n", data_type_to_string(ast_return->returnType));
            if (ast_return->value) Print_AST(ast_return->value, indent + 2, after_sem);
            break;
        }

        case AST_ARRAY_DECL: {
            AST_ArrayDecl* arr = (AST_ArrayDecl*)node;
            printf("ARRAY_DECL(type: %s, dims: [",
                   after_sem ? data_type_to_string(arr->elementType) : "unknown");
            for (int i = 0; i < arr->dim_count; i++) {
                if (i > 0) printf(", ");
                printf("%d", arr->dim_sizes[i]);
            }
            printf("]):\n");
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("name: %s\n", arr->name ? arr->name : "(null)");
            break;
        }

        case AST_ARRAY_ACCESS: {
            AST_ArrayAccess* acc = (AST_ArrayAccess*)node;
            printf("ARRAY_ACCESS(type: %s, indices:%d):\n",
                   after_sem ? data_type_to_string(acc->elementType) : "unknown",
                   acc->index_count);
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("Array:\n");
            if (acc->array) Print_AST(acc->array, indent+2, after_sem);
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("Indices:\n");
            for (int i = 0; i < acc->index_count; i++) {
                for (int j = 0; j < indent+2; j++) printf("  ");
                printf("Index[%d]:\n", i);
                for (int i = 0; i < acc->index_count; i++) {
                    for (int j = 0; j < indent+2; j++) printf("  ");
                    printf("Index[%d]:\n", i);
                    if (acc->indices[i]) Print_AST(acc->indices[i], indent+3, after_sem);
                    else printf("(NULL)\n");
                }
                if (acc->indices[i]) Print_AST(acc->indices[i], indent+3, after_sem);
            }
            break;
        }

        case AST_ARRAY_ASS: {
            AST_ArrayAss* ass = (AST_ArrayAss*)node;
            printf("ARRAY_ASS(type: %s):\n",
                   after_sem ? data_type_to_string(ass->elementType) : "unknown");
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("Target:\n");
            if (ass->target) Print_AST(ass->target, indent+2, after_sem);
            for (int i = 0; i < indent+1; i++) printf("  ");
            printf("Value:\n");
            if (ass->value) Print_AST(ass->value, indent+2, after_sem);
            break;
        }

        case AST_NEW_ARRAY: {
                AST_NewArray* narr = (AST_NewArray*)node;
                printf("NEW_ARRAY(type: %s, dims: [",
                       data_type_to_string(narr->elementType));
                for (int i = 0; i < narr->dim_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%d", narr->dim_sizes[i]);
                }
                printf("])\n");
                break;
        }

        case AST_INIT_LIST: {
                AST_InitList* list = (AST_InitList*)node;
                printf("INIT_LIST(count:%d, dims:", list->count);
                if (list->dim_sizes) {
                    for (int i=0; i<list->dim_count; i++) printf("%d,", list->dim_sizes[i]);
                } else printf("null");
                printf("):\n");
                for (int i=0; i<list->count; i++) {
                    for (int j=0;j<indent+1;j++) printf("  ");
                    printf("Element[%d]:\n", i);
                    if (list->elements[i]) Print_AST(list->elements[i], indent+2, after_sem);
                }
                break;
        }

        case AST_ARRAY_LENGTH: {
                AST_ArrayLength* len = (AST_ArrayLength*)node;
                printf("ARRAY_LENGTH:\n");
                for (int i = 0; i < indent+1; i++) printf("  ");
                printf("Array:\n");
                if (len->array) Print_AST(len->array, indent+2, after_sem);
                break;
        }

        case AST_ENUM_DEF: {
                AST_EnumDef* ed = (AST_EnumDef*)node;
                printf("ENUM_DEF(name: %s, members: %d):\n",
                       ed->name ? ed->name : "(anonymous)", ed->member_count);
                for (int i = 0; i < ed->member_count; i++) {
                    for (int j = 0; j < indent + 1; j++) printf("  ");
                    printf("%s", ed->members[i]->name);
                    if (ed->members[i]->has_explicit_value) {
                        printf(" = %d", ed->members[i]->value);
                    }
                    if (after_sem) {
                        printf("  [value: %d]", ed->members[i]->value);
                    }
                    printf("\n");
                }
                break;
        }

        case AST_ENUM_MEMBER: {
                AST_EnumMember* em = (AST_EnumMember*)node;
                printf("ENUM_MEMBER(%s = %d)\n", em->name, em->value);
                break;
        }

        case AST_ERROR:
            printf("ERROR NODE\n");
            break;

        default:
            printf("Unknown AST type: %d\n", node->type);
            break;
        }

        node = node->next;
    }
}

void AST_Free(ASTNode* node)
{
    while (node != NULL) {
        ASTNode* next = node->next;
        switch (node->type) {

        case AST_LITERAL: {
            AST_Literal* lit = (AST_Literal*)node;

            if (lit->internal_name) { free(lit->internal_name); lit->internal_name = NULL; }

            if (lit->call_function_Parameter) {
                AST_Free(lit->call_function_Parameter);
                lit->call_function_Parameter = NULL;
            }

            if (lit->method_arguments) {
                AST_Free(lit->method_arguments);
                lit->method_arguments = NULL;
            }
            break;
        }

        case AST_BINARY: {
            AST_Binary* bin = (AST_Binary*)node;
            if (bin->left)  { AST_Free(bin->left);  bin->left  = NULL; }
            if (bin->right) { AST_Free(bin->right); bin->right = NULL; }
            break;
        }

        case AST_UNARY: {
            AST_Unary* un = (AST_Unary*)node;
            if (un->right) { AST_Free(un->right); un->right = NULL; }
            break;
        }

        case AST_CONDITIONAL: {
                AST_Conditional* tern = (AST_Conditional*)node;
                if (tern->condition) { AST_Free(tern->condition); tern->condition = NULL; }
                if (tern->then_expr) { AST_Free(tern->then_expr); tern->then_expr = NULL; }
                if (tern->else_expr) { AST_Free(tern->else_expr); tern->else_expr = NULL; }
                break;
        }

        case AST_VAR:
        case AST_VAR_ASS:
        case AST_UNINIT_VAR: {
            AST_Var* var = (AST_Var*)node;
            if (var->internal_name) { free(var->internal_name); var->internal_name = NULL; }
            if (var->dim_sizes)     { free(var->dim_sizes);     var->dim_sizes     = NULL; }
            if (var->Value)         { AST_Free(var->Value);     var->Value         = NULL; }
            break;
        }

        case AST_PRINT: {
            AST_Print* prt = (AST_Print*)node;
            if (prt->Parameter) { AST_Free(prt->Parameter); prt->Parameter = NULL; }
            break;
        }

        case AST_INPUT: {
            AST_Input* inp = (AST_Input*)node;
            if (inp->format_string) { AST_Free(inp->format_string); inp->format_string = NULL; }
            if (inp->args) {
                for (int i = 0; i < inp->arg_count; i++) {
                    if (inp->args[i]) AST_Free(inp->args[i]);
                }
                free(inp->args);
                inp->args = NULL;
            }
            break;
        }

        case AST_IF: {
            AST_If* ifn = (AST_If*)node;
            if (ifn->Condition) { AST_Free(ifn->Condition); ifn->Condition = NULL; }
            if (ifn->code)      { AST_Free(ifn->code);      ifn->code      = NULL; }
            if (ifn->Else)      { AST_Free(ifn->Else);      ifn->Else      = NULL; }
            if (ifn->elif_condition) {
                for (int i = 0; i < ifn->elif_Count; i++) {
                    if (ifn->elif_condition[i]) AST_Free(ifn->elif_condition[i]);
                }
                free(ifn->elif_condition);
                ifn->elif_condition = NULL;
            }
            if (ifn->Elif) {
                for (int i = 0; i < ifn->elif_Count; i++) {
                    if (ifn->Elif[i]) AST_Free(ifn->Elif[i]);
                }
                free(ifn->Elif);
                ifn->Elif = NULL;
            }
            break;
        }

        case AST_SWITCH: {
            AST_Switch* sw = (AST_Switch*)node;
            if (sw->condition) { AST_Free(sw->condition); sw->condition = NULL; }
            if (sw->cases) {
                for (int i = 0; i < sw->case_count; i++) {
                    if (sw->cases[i]) {
                        if (sw->cases[i]->value) AST_Free(sw->cases[i]->value);
                        if (sw->cases[i]->body)  AST_Free(sw->cases[i]->body);
                        free(sw->cases[i]);
                    }
                }
                free(sw->cases);
                sw->cases = NULL;
            }
            if (sw->default_body) { AST_Free(sw->default_body); sw->default_body = NULL; }
            break;
        }

        case AST_CASE: {
            AST_Case* cs = (AST_Case*)node;
            if (cs->value) { AST_Free(cs->value); cs->value = NULL; }
            if (cs->body)  { AST_Free(cs->body);  cs->body  = NULL; }
            break;
        }

        case AST_BREAK:
        case AST_CONTINUE:
            break;

        case AST_WHILE: {
            AST_While* w = (AST_While*)node;
            if (w->Condition) { AST_Free(w->Condition); w->Condition = NULL; }
            if (w->code)      { AST_Free(w->code);      w->code      = NULL; }
            break;
        }

        case AST_FOR: {
            AST_For* f = (AST_For*)node;
            if (f->init)      { AST_Free(f->init);      f->init      = NULL; }
            if (f->condition) { AST_Free(f->condition); f->condition = NULL; }
            if (f->increment) { AST_Free(f->increment); f->increment = NULL; }
            if (f->code)      { AST_Free(f->code);      f->code      = NULL; }
            break;
        }

        case AST_FOREACH: {
            AST_ForEach* fe = (AST_ForEach*)node;
            if (fe->array)             { AST_Free(fe->array); fe->array = NULL; }
            if (fe->code)              { AST_Free(fe->code);  fe->code  = NULL; }
            if (fe->internal_iterator) { free(fe->internal_iterator); fe->internal_iterator = NULL; }
            break;
        }

        case AST_FUNCTION: {
            AST_Function* func = (AST_Function*)node;
            if (func->internal_name) { free(func->internal_name); func->internal_name = NULL; }
            if (func->Parameter)     { AST_Free(func->Parameter); func->Parameter     = NULL; }
            if (func->Code)          { AST_Free(func->Code);      func->Code          = NULL; }
            if (func->parameterType) { free(func->parameterType); func->parameterType = NULL; }
            break;
        }

        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            if (ret->value) { AST_Free(ret->value); ret->value = NULL; }
            break;
        }

        case AST_CLASS_DEF: {
            AST_ClassDef* cls = (AST_ClassDef*)node;
            if (cls->name)   { free(cls->name);   cls->name   = NULL; }
            if (cls->parent_name) { free(cls->parent_name); cls->parent_name = NULL; }
            if (cls->parent_names) {
                for (int i = 0; i < cls->parent_count; i++) {
                    free(cls->parent_names[i]);
                }
                free(cls->parent_names);
                cls->parent_names = NULL;
            }
            if (cls->members) { AST_Free(cls->members); cls->members = NULL; }
            break;
        }

        case AST_NEW_INSTANCE: {
            AST_NewInstance* ni = (AST_NewInstance*)node;
            if (ni->class_name)       { free(ni->class_name);       ni->class_name       = NULL; }
            if (ni->constructor_name) { free(ni->constructor_name); ni->constructor_name = NULL; }
            if (ni->arguments)        { AST_Free(ni->arguments);    ni->arguments        = NULL; }
            if (ni->field_inits) {
                for (int i = 0; i < ni->field_init_count; i++) {
                    if (ni->field_inits[i].value_str)
                        free(ni->field_inits[i].value_str);
                }
                free(ni->field_inits);
                ni->field_inits = NULL;
            }
            break;
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* ma = (AST_MemberAccess*)node;
            if (ma->member_name) { free(ma->member_name); ma->member_name = NULL; }
            if (ma->object)      { AST_Free(ma->object);  ma->object      = NULL; }
            break;
        }

        case AST_MEMBER_ASS: {
            AST_MemberAss* mass = (AST_MemberAss*)node;
            if (mass->member_name) { free(mass->member_name); mass->member_name = NULL; }
            if (mass->object)      { AST_Free(mass->object);  mass->object      = NULL; }
            if (mass->value)       { AST_Free(mass->value);   mass->value       = NULL; }
            break;
        }

        case AST_THIS:
            break;

        case AST_ARRAY_DECL: {
            AST_ArrayDecl* arr = (AST_ArrayDecl*)node;
            if (arr->name)          { free(arr->name);          arr->name          = NULL; }
            if (arr->internal_name) { free(arr->internal_name); arr->internal_name = NULL; }
            if (arr->dim_sizes)     { free(arr->dim_sizes);     arr->dim_sizes     = NULL; }
            if (arr->init_list)     { AST_Free(arr->init_list); arr->init_list     = NULL; }
            break;
        }

        case AST_ARRAY_ACCESS: {
            AST_ArrayAccess* acc = (AST_ArrayAccess*)node;
            if (acc->array)    { AST_Free(acc->array); acc->array = NULL; }
            if (acc->indices) {
                for (int i = 0; i < acc->index_count; i++) {
                    if (acc->indices[i]) AST_Free(acc->indices[i]);
                }
                free(acc->indices);
                acc->indices = NULL;
            }
            if (acc->dim_sizes)      { free(acc->dim_sizes);      acc->dim_sizes      = NULL; }
            if (acc->full_dim_sizes) { free(acc->full_dim_sizes); acc->full_dim_sizes = NULL; }
            break;
        }

        case AST_ARRAY_ASS: {
            AST_ArrayAss* ass = (AST_ArrayAss*)node;
            if (ass->target) { AST_Free(ass->target); ass->target = NULL; }
            if (ass->value)  { AST_Free(ass->value);  ass->value  = NULL; }
            break;
        }

        case AST_NEW_ARRAY: {
                AST_NewArray* narr = (AST_NewArray*)node;
                if (narr->dim_sizes) { free(narr->dim_sizes); narr->dim_sizes = NULL; }
                if (narr->dim_exprs) {
                    for (int i = 0; i < narr->dim_count; i++) {
                        if (narr->dim_exprs[i]) AST_Free(narr->dim_exprs[i]);
                    }
                    free(narr->dim_exprs);
                    narr->dim_exprs = NULL;
                }
                break;
        }

        case AST_INIT_LIST: {
            AST_InitList* list = (AST_InitList*)node;
            if (list->elements) {
                for (int i = 0; i < list->count; i++) {
                    if (list->elements[i]) AST_Free(list->elements[i]);
                }
                free(list->elements);
                list->elements = NULL;
            }
            if (list->dim_sizes) { free(list->dim_sizes); list->dim_sizes = NULL; }
            break;
        }

        case AST_ARRAY_LENGTH: {
            AST_ArrayLength* alen = (AST_ArrayLength*)node;
            if (alen->array) { AST_Free(alen->array); alen->array = NULL; }
            break;
        }

        case AST_SUBARRAY: {
            AST_SubArray* sub = (AST_SubArray*)node;
            if (sub->array)     { AST_Free(sub->array); sub->array = NULL; }
            if (sub->dim_sizes) { free(sub->dim_sizes); sub->dim_sizes = NULL; }
            if (sub->base_temp) { free(sub->base_temp); sub->base_temp = NULL; }
            break;
        }

        case AST_ENUM_DEF: {
            AST_EnumDef* ed = (AST_EnumDef*)node;
            if (ed->name) { free(ed->name); ed->name = NULL; }
            if (ed->members) {
                for (int i = 0; i < ed->member_count; i++) {
                    if (ed->members[i]) {
                        if (ed->members[i]->name) free(ed->members[i]->name);
                        if (ed->members[i]->explicit_token) {
                            if (ed->members[i]->explicit_token->value)
                                free(ed->members[i]->explicit_token->value);
                            free(ed->members[i]->explicit_token);
                        }
                        free(ed->members[i]);
                    }
                }
                free(ed->members);
                ed->members = NULL;
            }
            break;
        }

        case AST_ENUM_MEMBER: {
            AST_EnumMember* em = (AST_EnumMember*)node;
            if (em->name) { free(em->name); em->name = NULL; }
            if (em->explicit_token) {
                if (em->explicit_token->value) free(em->explicit_token->value);
                free(em->explicit_token);
                em->explicit_token = NULL;
            }
            break;
        }

        case AST_ERROR:
            break;
        default:
            break;
        }

        free(node);
        node = next;
    }
}