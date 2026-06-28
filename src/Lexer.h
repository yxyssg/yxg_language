#ifndef LEXER_H
#define LEXER_H

#include "Token.h"
#include "YString.h"
#define TOKENS_INITIAL_SIZE 128

typedef struct Lexer
{
    char* source;
    char* filename;
    char current;
    Token* tokens;
    int size;
    int index;
    int i;
}Lexer;

void lexer_parse();
Lexer* lexer(char* source,const char* filename);
void lexer_free(Lexer* lexer);

#endif //LEXER_H
