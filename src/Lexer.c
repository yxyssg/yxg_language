#include "Lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Error.h"

static Lexer* lexers = NULL;
static const char* current_filename = NULL;

Lexer* lexer(char* source, const char* filename)
{
    if (source == NULL)
    {
        INTERNAL_ERROR("lexer: null source");
    }
    current_filename = filename;
    if (filename) {
        error_register_source(filename, source);
    }
    lexers = (Lexer*)malloc(sizeof(Lexer));
    if (lexers == NULL)
    {
        OUT_OF_MEMORY();
    }
    lexers->source = source;
    lexers->filename = filename ? strdup(filename) : NULL;
    lexers->current = lexers->source[0];
    lexers->index = 0;
    lexers->size = 16;
    lexers->i = 0;
    lexers->tokens = malloc(sizeof(Token) * lexers->size);
    if (lexers->tokens == NULL)
    {
        OUT_OF_MEMORY();
    }
    lexer_parse();
    lexers->size = lexers->index;
    lexers->tokens = realloc(lexers->tokens, lexers->size * sizeof(Token));
    if (lexers->tokens == NULL)
    {
        OUT_OF_MEMORY();
    }
    return lexers;
}

void tokens_resize()
{
    if (!lexers) return;

    int old_size = lexers->size;
    int new_size;

    if (old_size == 0) {
        new_size = TOKENS_INITIAL_SIZE;
    } else {
        new_size = old_size * 2;
    }

    Token* new_tokens = realloc(lexers->tokens, new_size * sizeof(Token));
    if (!new_tokens) {
        INTERNAL_ERROR("tokens_resize: failed to resize tokens array");
    }

    lexers->tokens = new_tokens;
    lexers->size = new_size;

#ifdef DEBUG
    fprintf(stderr, "Debug: Tokens array resized from %d to %d\n", old_size, new_size);
#endif
}

void tokens_add(Token token)
{
    // °²È«¼ì²é
    if (!lexers) {
        INTERNAL_ERROR("tokens_add: lexers is NULL");
    }

    if (lexers->index >= lexers->size) {
        tokens_resize();
    }

    lexers->tokens[lexers->index] = token;

    lexers->index++;
}

void token_advance()
{
    lexers->current = lexers->source[++lexers->i];
}

void create_token(String* string, int line, int column)
{
    struct key_token key_token = find_key(string_cstr(string));
    char* temp = malloc(string_length(string) + 1);
    if (!temp) { OUT_OF_MEMORY(); }
    strcpy(temp, string_cstr(string));
    Token token = {temp, key_token.type, line, column - string_length(string)};
    tokens_add(token);
}

void lexer_parse()
{
    Lexer* lex = lexers;
    int line = 1;
    int column = 1;
    String* string = string_create_empty();

    while (lex->current != '\0')
    {
        if (isspace(lex->current))
        {
            if (!string_is_empty(string))
            {
                char* value = malloc(string_length(string) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, string_cstr(string));
                Token token = { value, find_key(value).type, line, column - string_length(string) };
                tokens_add(token);
                string_clear(string);
            }
            if (lex->current == '\n')
            {
                line++;
                column = 1;
            }
            else
            {
                column++;
            }
            token_advance();
            continue;
        }

        if (lex->current == '/' && lex->source[lex->i + 1] == '/')
        {
            if (!string_is_empty(string))
            {
                char* value = malloc(string_length(string) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, string_cstr(string));
                Token token = { value, find_key(value).type, line, column - string_length(string) };
                tokens_add(token);
                string_clear(string);
            }
            while (lex->current != '\0' && lex->current != '\n')
            {
                column++;
                token_advance();
            }

            continue;
        }

        if (lex->current == '/' && lex->source[lex->i + 1] == '*')
        {
            if (!string_is_empty(string))
            {
                char* value = malloc(string_length(string) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, string_cstr(string));
                Token token = { value, find_key(value).type, line, column - string_length(string) };
                tokens_add(token);
                string_clear(string);
            }

            token_advance();
            token_advance();
            column += 2;
            while (!(lex->current == '*' && lex->source[lex->i + 1] == '/') && lex->current != '\0')
            {
                if (lex->current == '\n')
                {
                    line++;
                    column = 1;
                }
                else
                {
                    column++;
                }
                token_advance();
            }
            if (lex->current == '\0')
            {
                ERROR(ERR_LEX_UNTERMINATED_COMMENT,
                      LOC(current_filename, line, column),
                      "unterminated multi-line comment");
            }
            token_advance();
            token_advance();
            column += 2;
            continue;
        }

        if (lex->current == '"')
        {
            if (!string_is_empty(string))
            {
                char* value = malloc(string_length(string) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, string_cstr(string));
                Token token = { value, find_key(value).type, line, column - string_length(string) };
                tokens_add(token);
                string_clear(string);
            }
            token_advance();
            column++;
            int start_line = line;
            int start_column = column;

            while (lex->current != '"' && lex->current != '\0')
            {
                if (lex->current == '\n')
                {
                    line++;
                    column = 1;
                }
                else
                {
                    column++;
                }

                if (lex->current == '\\')
                {
                    token_advance();
                    if (lex->current == '\0') break;
                    string_append_char(string, '\\');
                    string_append_char(string, lex->current);
                    token_advance();
                }
                else
                {
                    string_append_char(string, lex->current);
                    token_advance();
                }
            }
            if (lex->current == '\0')
            {
                ERROR(ERR_LEX_UNTERMINATED_STRING,
                      LOC(current_filename, start_line, start_column),
                      "unterminated string literal");
                string_clear(string);
                continue;
            }

            token_advance();
            column++;
            char* value = malloc(string_length(string) + 1);
            if (!value) { OUT_OF_MEMORY(); }
            strcpy(value, string_cstr(string));
            Token token = { value, TOKEN_STRING_LITERAL, start_line, start_column };
            tokens_add(token);
            string_clear(string);
            continue;
        }

        if (lex->current == '\'')
        {
            if (!string_is_empty(string))
            {
                char* value = malloc(string_length(string) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, string_cstr(string));
                Token token = { value, find_key(value).type, line, column - string_length(string) };
                tokens_add(token);
                string_clear(string);
            }

            token_advance();
            column++;
            int start_line = line;
            int start_column = column;
            int char_count = 0;
            int has_escape = 0;
            while (lex->current != '\'' && lex->current != '\0')
            {
                if (lex->current == '\n')
                {
                    line++;
                    column = 1;
                }
                else
                {
                    column++;
                }
                if (lex->current == '\\')
                {
                    has_escape = 1;
                    token_advance();
                    if (lex->current == '\0') break;
                    string_append_char(string, '\\');
                    string_append_char(string, lex->current);
                    token_advance();
                }
                else
                {
                    string_append_char(string, lex->current);
                    token_advance();
                }
                char_count++;
            }
            if (lex->current == '\0')
            {
                ERROR(ERR_LEX_UNTERMINATED_CHAR,
                      LOC(current_filename, start_line, start_column),
                      "unterminated character literal");
                string_clear(string);
                continue;
            }

            token_advance();
            column++;

            int len = string_length(string);
            char* value = malloc(len + 1);
            if (!value) { OUT_OF_MEMORY(); }
            strcpy(value, string_cstr(string));

            TokenType type;
            if (len > 1 && !has_escape)
            {
                WARNING(ERR_LEX_MULTICHAR_LITERAL, LOC(current_filename, start_line, start_column),
                        "multi-character literal treated as string");
                type = TOKEN_STRING_LITERAL;
            }
            else if (has_escape)
            {
                type = TOKEN_STRING_LITERAL;
            }
            else
            {
                type = TOKEN_CHAR_LITERAL;
            }
            Token token = { value, type, start_line, start_column };
            tokens_add(token);
            string_clear(string);
            continue;
        }

        if ((lex->current == '<' && lex->source[lex->i + 1] == '<') ||
            (lex->current == '>' && lex->source[lex->i + 1] == '>'))
        {
            int start_col = column;
            char first  = lex->current;
            token_advance(); column++;
            char second = lex->current;
            token_advance(); column++;
            if (lex->current == '=') {
                token_advance(); column++;
                char compound[4] = {first, second, '=', '\0'};
                char* val = malloc(strlen(compound) + 1);
                strcpy(val, compound);
                tokens_add((Token){val, find_key(val).type, line, start_col});
            } else {
                char compound[3] = {first, second, '\0'};
                char* val = malloc(strlen(compound) + 1);
                strcpy(val, compound);
                tokens_add((Token){val, find_key(val).type, line, start_col});
            }
            continue;
        }

        if (lex->current == '.') {
            bool absorb = false;
            if (!string_is_empty(string)) {
                const char* buf = string_cstr(string);
                int len = string_length(string);
                if (len > 0 && isdigit((unsigned char)buf[len - 1]))
                    absorb = true;
            } else {
                if (isdigit((unsigned char)lex->source[lex->i + 1]))
                    absorb = true;
            }
            if (absorb) {
                string_append_char(string, '.');
                column++;
                token_advance();
                continue;
            }
        }

        if (ispunct(lex->current) &&
                !(is_number_prefix(string_cstr(string)) && lex->current == '.') &&
                is_key_symbol(lex->current))
            {
            if (!string_is_empty(string))
            {
                char* value = malloc(string_length(string) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, string_cstr(string));
                Token token = { value, find_key(value).type, line, column - string_length(string) };
                tokens_add(token);
                string_clear(string);
            }

            if ((lex->current == '!' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '=' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '>' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '<' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '^' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '&' && lex->source[lex->i + 1] == '&') ||
                (lex->current == '|' && lex->source[lex->i + 1] == '|') ||
                (lex->current == '+' && (lex->source[lex->i + 1] == '+' || lex->source[lex->i + 1] == '=')) ||
                (lex->current == '-' && (lex->source[lex->i + 1] == '-' || lex->source[lex->i + 1] == '=')) ||
                (lex->current == '*' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '/' && lex->source[lex->i + 1] == '=') ||
                (lex->current == '%' && lex->source[lex->i + 1] == '='))
            {
                int start_col = column;
                char compound[3] = {lex->current, lex->source[lex->i + 1], '\0'};
                token_advance();
                column++;
                token_advance();
                column++;
                char* value = malloc(strlen(compound) + 1);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, compound);
                Token token = { value, find_key(value).type, line, start_col };
                tokens_add(token);
            }
            else
            {
                int start_col = column;
                char sym[2] = {lex->current, '\0'};
                char* value = malloc(2);
                if (!value) { OUT_OF_MEMORY(); }
                strcpy(value, sym);
                Token token = { value, find_key(value).type, line, start_col };
                tokens_add(token);
                column++;
                token_advance();
            }
            continue;
        }

        string_append_char(string, lex->current);
        column++;
        token_advance();
    }

    if (!string_is_empty(string))
    {
        char* value = malloc(string_length(string) + 1);
        if (!value) { OUT_OF_MEMORY(); }
        strcpy(value, string_cstr(string));
        Token token = { value, find_key(value).type, line, column - string_length(string) };
        tokens_add(token);
        string_clear(string);
    }
    string_destroy(string);

    char* eof_value = malloc(4);
    if (!eof_value) { OUT_OF_MEMORY(); }
    strcpy(eof_value, "EOF");
    Token eof_token = { eof_value, TOKEN_EOF, line, column };
    tokens_add(eof_token);
}

void lexer_free(Lexer* lexer)
{
    if (lexer == NULL) return;

    if (lexer->tokens != NULL) {
        for (int i = 0; i < lexer->index; i++) {
            if (lexer->tokens[i].value != NULL) {
                free(lexer->tokens[i].value);
                lexer->tokens[i].value = NULL;
            }
        }
        free(lexer->tokens);
        lexer->tokens = NULL;
    }

    if (lexer->source != NULL) {
        free(lexer->source);
        lexer->source = NULL;
    }

    if (lexer->filename != NULL) {
        free(lexer->filename);
        lexer->filename = NULL;
    }


    free(lexer);
}