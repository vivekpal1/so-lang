/*
 * so_lang.h - So Lang Programming Language Header
 * A fast, simple toy programming language built in C
 */

#ifndef SO_LANG_H
#define SO_LANG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_TOKEN_LEN 256
#define MAX_TOKENS 1000
#define MAX_VARS 100
#define MAX_FUNCTIONS 50

// Token types
typedef enum {
    TOKEN_EOF,
    TOKEN_LET,
    TOKEN_FN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_PRINT,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_NEWLINE
} TokenType;

typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LEN];
    int line;
    int column;
} Token;

typedef enum {
    NODE_PROGRAM,
    NODE_VAR_DECL,
    NODE_FUNC_DECL,
    NODE_IF_STMT,
    NODE_RETURN_STMT,
    NODE_PRINT_STMT,
    NODE_BINARY_OP,
    NODE_IDENTIFIER,
    NODE_NUMBER,
    NODE_STRING,
    NODE_FUNC_CALL
} NodeType;

typedef struct ASTNode {
    NodeType type;
    char value[MAX_TOKEN_LEN];
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* condition;
    struct ASTNode* then_branch;
    struct ASTNode* else_branch;
    struct ASTNode** children;
    int child_count;
} ASTNode;

typedef struct {
    char* source;
    int pos;
    int line;
    int column;
    Token tokens[MAX_TOKENS];
    int token_count;
} Lexer;

typedef struct {
    Token* tokens;
    int pos;
    int token_count;
} Parser;

typedef struct {
    FILE* output;
    bool to_rust;
} Compiler;

// Function declarations
Lexer* lexer_create(char* source);
void lexer_tokenize(Lexer* lexer);
void lexer_free(Lexer* lexer);

Parser* parser_create(Token* tokens, int token_count);
ASTNode* parser_parse(Parser* parser);
void parser_free(Parser* parser);

ASTNode* ast_create_node(NodeType type);
void ast_free(ASTNode* node);

Compiler* compiler_create(FILE* output, bool to_rust);
void compiler_compile(Compiler* compiler, ASTNode* ast);
void compiler_free(Compiler* compiler);

void error(const char* message, int line, int column);
char* read_file(const char* filename);

#endif