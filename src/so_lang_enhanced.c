/*
 * so_lang_enhanced.c - Enhanced So Lang with Function Support
 * Extended version supporting functions for self-hosting
 */

#include "so_lang.h"

// Enhanced global variables for function support
static bool has_error = false;
static char** function_names = NULL;
static int function_count = 0;
static bool in_function = false;

// ============================================================================
// ENHANCED UTILITY FUNCTIONS
// ============================================================================

void error(const char* message, int line, int column) {
    fprintf(stderr, "Error at line %d, column %d: %s\n", line, column, message);
    has_error = true;
}

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    
    fclose(file);
    return content;
}

// ============================================================================
// ENHANCED LEXER WITH FUNCTION SUPPORT
// ============================================================================

Lexer* lexer_create(char* source) {
    Lexer* lexer = malloc(sizeof(Lexer));
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_count = 0;
    return lexer;
}

static char lexer_current_char(Lexer* lexer) {
    if (lexer->pos >= strlen(lexer->source)) return '\0';
    return lexer->source[lexer->pos];
}

static char lexer_advance(Lexer* lexer) {
    char c = lexer_current_char(lexer);
    lexer->pos++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void lexer_skip_whitespace(Lexer* lexer) {
    while (isspace(lexer_current_char(lexer)) && lexer_current_char(lexer) != '\n') {
        lexer_advance(lexer);
    }
}

static void lexer_skip_comment(Lexer* lexer) {
    if (lexer_current_char(lexer) == '/' && lexer->source[lexer->pos + 1] == '/') {
        while (lexer_current_char(lexer) != '\n' && lexer_current_char(lexer) != '\0') {
            lexer_advance(lexer);
        }
    }
}

static void lexer_add_token(Lexer* lexer, TokenType type, const char* value) {
    if (lexer->token_count >= MAX_TOKENS) {
        error("Too many tokens", lexer->line, lexer->column);
        return;
    }
    
    Token* token = &lexer->tokens[lexer->token_count++];
    token->type = type;
    strncpy(token->value, value, MAX_TOKEN_LEN - 1);
    token->value[MAX_TOKEN_LEN - 1] = '\0';
    token->line = lexer->line;
    token->column = lexer->column;
}

static void lexer_read_string(Lexer* lexer) {
    char buffer[MAX_TOKEN_LEN];
    int i = 0;
    
    lexer_advance(lexer);
    
    while (lexer_current_char(lexer) != '"' && lexer_current_char(lexer) != '\0') {
        if (lexer_current_char(lexer) == '\\') {
            lexer_advance(lexer); // Skip escape character
            char escaped = lexer_current_char(lexer);
            switch (escaped) {
                case 'n': buffer[i++] = '\n'; break;
                case 't': buffer[i++] = '\t'; break;
                case 'r': buffer[i++] = '\r'; break;
                case '\\': buffer[i++] = '\\'; break;
                case '"': buffer[i++] = '"'; break;
                default: buffer[i++] = escaped; break;
            }
        } else {
            if (i < MAX_TOKEN_LEN - 1) {
                buffer[i++] = lexer_current_char(lexer);
            }
        }
        lexer_advance(lexer);
    }
    
    if (lexer_current_char(lexer) == '"') {
        lexer_advance(lexer); // Skip closing quote
    }
    
    buffer[i] = '\0';
    lexer_add_token(lexer, TOKEN_STRING, buffer);
}

static void lexer_read_identifier(Lexer* lexer) {
    char buffer[MAX_TOKEN_LEN];
    int i = 0;
    
    while (isalnum(lexer_current_char(lexer)) || lexer_current_char(lexer) == '_') {
        if (i < MAX_TOKEN_LEN - 1) {
            buffer[i++] = lexer_current_char(lexer);
        }
        lexer_advance(lexer);
    }
    
    buffer[i] = '\0';
    
    TokenType type = TOKEN_IDENTIFIER;
    if (strcmp(buffer, "let") == 0) type = TOKEN_LET;
    else if (strcmp(buffer, "fn") == 0) type = TOKEN_FN;
    else if (strcmp(buffer, "if") == 0) type = TOKEN_IF;
    else if (strcmp(buffer, "else") == 0) type = TOKEN_ELSE;
    else if (strcmp(buffer, "return") == 0) type = TOKEN_RETURN;
    else if (strcmp(buffer, "print") == 0) type = TOKEN_PRINT;
    
    lexer_add_token(lexer, type, buffer);
}

static void lexer_read_number(Lexer* lexer) {
    char buffer[MAX_TOKEN_LEN];
    int i = 0;
    bool has_dot = false;
    
    while (isdigit(lexer_current_char(lexer)) || 
           (lexer_current_char(lexer) == '.' && !has_dot)) {
        if (lexer_current_char(lexer) == '.') has_dot = true;
        if (i < MAX_TOKEN_LEN - 1) {
            buffer[i++] = lexer_current_char(lexer);
        }
        lexer_advance(lexer);
    }
    
    buffer[i] = '\0';
    lexer_add_token(lexer, TOKEN_NUMBER, buffer);
}

void lexer_tokenize(Lexer* lexer) {
    while (lexer_current_char(lexer) != '\0') {
        char c = lexer_current_char(lexer);
        
        if (isspace(c) && c != '\n') {
            lexer_skip_whitespace(lexer);
        } else if (c == '/' && lexer->source[lexer->pos + 1] == '/') {
            lexer_skip_comment(lexer);
        } else if (c == '\n') {
            lexer_add_token(lexer, TOKEN_NEWLINE, "\n");
            lexer_advance(lexer);
        } else if (c == '"') {
            lexer_read_string(lexer);
        } else if (isalpha(c) || c == '_') {
            lexer_read_identifier(lexer);
        } else if (isdigit(c)) {
            lexer_read_number(lexer);
        } else {
            char token_str[2] = {c, '\0'};
            switch (c) {
                case '=':
                    if (lexer->source[lexer->pos + 1] == '=') {
                        lexer_advance(lexer);
                        lexer_add_token(lexer, TOKEN_EQUAL, "==");
                    } else {
                        lexer_add_token(lexer, TOKEN_ASSIGN, "=");
                    }
                    break;
                case '+': lexer_add_token(lexer, TOKEN_PLUS, token_str); break;
                case '-': lexer_add_token(lexer, TOKEN_MINUS, token_str); break;
                case '*': lexer_add_token(lexer, TOKEN_MULTIPLY, token_str); break;
                case '/': lexer_add_token(lexer, TOKEN_DIVIDE, token_str); break;
                case '<': lexer_add_token(lexer, TOKEN_LESS, token_str); break;
                case '>': lexer_add_token(lexer, TOKEN_GREATER, token_str); break;
                case '(': lexer_add_token(lexer, TOKEN_LPAREN, token_str); break;
                case ')': lexer_add_token(lexer, TOKEN_RPAREN, token_str); break;
                case '{': lexer_add_token(lexer, TOKEN_LBRACE, token_str); break;
                case '}': lexer_add_token(lexer, TOKEN_RBRACE, token_str); break;
                case ',': lexer_add_token(lexer, TOKEN_COMMA, token_str); break;
                case ';': lexer_add_token(lexer, TOKEN_SEMICOLON, token_str); break;
                default:
                    error("Unexpected character", lexer->line, lexer->column);
                    break;
            }
            lexer_advance(lexer);
        }
    }
    
    lexer_add_token(lexer, TOKEN_EOF, "");
}

void lexer_free(Lexer* lexer) {
    free(lexer);
}

// ============================================================================
// ENHANCED AST WITH FUNCTION SUPPORT
// ============================================================================

ASTNode* ast_create_node(NodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->value[0] = '\0';
    node->left = NULL;
    node->right = NULL;
    node->condition = NULL;
    node->then_branch = NULL;
    node->else_branch = NULL;
    node->children = NULL;
    node->child_count = 0;
    return node;
}

void ast_free(ASTNode* node) {
    if (!node) return;
    
    ast_free(node->left);
    ast_free(node->right);
    ast_free(node->condition);
    ast_free(node->then_branch);
    ast_free(node->else_branch);
    
    if (node->children) {
        for (int i = 0; i < node->child_count; i++) {
            ast_free(node->children[i]);
        }
        free(node->children);
    }
    
    free(node);
}

// ============================================================================
// ENHANCED PARSER WITH FUNCTION SUPPORT
// ============================================================================

Parser* parser_create(Token* tokens, int token_count) {
    Parser* parser = malloc(sizeof(Parser));
    parser->tokens = tokens;
    parser->pos = 0;
    parser->token_count = token_count;
    return parser;
}

static Token* parser_current_token(Parser* parser) {
    if (parser->pos >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1]; // EOF token
    }
    return &parser->tokens[parser->pos];
}

static Token* parser_advance(Parser* parser) {
    Token* token = parser_current_token(parser);
    if (parser->pos < parser->token_count - 1) {
        parser->pos++;
    }
    return token;
}

static bool parser_match(Parser* parser, TokenType type) {
    if (parser_current_token(parser)->type == type) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static ASTNode* parser_parse_expression(Parser* parser);
static ASTNode* parser_parse_statement(Parser* parser);
static ASTNode* parser_parse_block(Parser* parser);

static ASTNode* parser_parse_primary(Parser* parser) {
    Token* token = parser_current_token(parser);
    ASTNode* node = NULL;
    
    if (token->type == TOKEN_NUMBER) {
        node = ast_create_node(NODE_NUMBER);
        strcpy(node->value, token->value);
        parser_advance(parser);
    } else if (token->type == TOKEN_STRING) {
        node = ast_create_node(NODE_STRING);
        strcpy(node->value, token->value);
        parser_advance(parser);
    } else if (token->type == TOKEN_IDENTIFIER) {
        node = ast_create_node(NODE_IDENTIFIER);
        strcpy(node->value, token->value);
        parser_advance(parser);
        
        // Check for function call
        if (parser_current_token(parser)->type == TOKEN_LPAREN) {
            node->type = NODE_FUNC_CALL;
            parser_advance(parser); // Skip '('
            
            // Parse arguments (simplified - no args for now)
            while (parser_current_token(parser)->type != TOKEN_RPAREN &&
                   parser_current_token(parser)->type != TOKEN_EOF) {
                // Skip arguments for now
                parser_advance(parser);
            }
            parser_match(parser, TOKEN_RPAREN);
        }
    } else if (token->type == TOKEN_LPAREN) {
        parser_advance(parser);
        node = parser_parse_expression(parser);
        parser_match(parser, TOKEN_RPAREN);
    }
    
    return node;
}

static ASTNode* parser_parse_binary(Parser* parser) {
    ASTNode* left = parser_parse_primary(parser);
    
    Token* op = parser_current_token(parser);
    if (op->type == TOKEN_PLUS || op->type == TOKEN_MINUS || 
        op->type == TOKEN_MULTIPLY || op->type == TOKEN_DIVIDE ||
        op->type == TOKEN_EQUAL || op->type == TOKEN_LESS || op->type == TOKEN_GREATER) {
        
        parser_advance(parser);
        ASTNode* right = parser_parse_primary(parser);
        
        ASTNode* binary = ast_create_node(NODE_BINARY_OP);
        strcpy(binary->value, op->value);
        binary->left = left;
        binary->right = right;
        return binary;
    }
    
    return left;
}

static ASTNode* parser_parse_expression(Parser* parser) {
    return parser_parse_binary(parser);
}

static ASTNode* parser_parse_block(Parser* parser) {
    ASTNode* block = ast_create_node(NODE_PROGRAM);
    block->children = malloc(sizeof(ASTNode*) * 50);
    block->child_count = 0;
    
    if (!parser_match(parser, TOKEN_LBRACE)) {
        error("Expected '{'", parser_current_token(parser)->line, parser_current_token(parser)->column);
        return block;
    }
    
    while (parser_current_token(parser)->type != TOKEN_RBRACE && 
           parser_current_token(parser)->type != TOKEN_EOF) {
        
        // Skip newlines
        if (parser_match(parser, TOKEN_NEWLINE)) {
            continue;
        }
        
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt && block->child_count < 50) {
            block->children[block->child_count++] = stmt;
        }
    }
    
    parser_match(parser, TOKEN_RBRACE);
    return block;
}

static ASTNode* parser_parse_function(Parser* parser) {
    parser_advance(parser); // consume 'fn'
    
    ASTNode* func = ast_create_node(NODE_FUNC_DECL);
    
    // Get function name
    Token* name = parser_current_token(parser);
    if (name->type == TOKEN_IDENTIFIER) {
        strcpy(func->value, name->value);
        parser_advance(parser);
        
        // Parse parameters
        if (parser_match(parser, TOKEN_LPAREN)) {
            // Skip parameters for now (simplified)
            while (parser_current_token(parser)->type != TOKEN_RPAREN &&
                   parser_current_token(parser)->type != TOKEN_EOF) {
                parser_advance(parser);
            }
            parser_match(parser, TOKEN_RPAREN);
        }
        
        // Parse function body
        func->left = parser_parse_block(parser);
        
        // Add to function registry
        if (!function_names) {
            function_names = malloc(sizeof(char*) * MAX_FUNCTIONS);
        }
        if (function_count < MAX_FUNCTIONS) {
            function_names[function_count] = malloc(strlen(func->value) + 1);
            strcpy(function_names[function_count], func->value);
            function_count++;
        }
    }
    
    return func;
}

static ASTNode* parser_parse_statement(Parser* parser) {
    Token* token = parser_current_token(parser);
    ASTNode* node = NULL;
    
    if (token->type == TOKEN_FN) {
        node = parser_parse_function(parser);
    } else if (token->type == TOKEN_LET) {
        parser_advance(parser);
        node = ast_create_node(NODE_VAR_DECL);
        
        Token* name = parser_current_token(parser);
        if (name->type == TOKEN_IDENTIFIER) {
            strcpy(node->value, name->value);
            parser_advance(parser);
            
            if (parser_match(parser, TOKEN_ASSIGN)) {
                node->right = parser_parse_expression(parser);
            }
        }
    } else if (token->type == TOKEN_PRINT) {
        parser_advance(parser);
        node = ast_create_node(NODE_PRINT_STMT);
        
        if (parser_match(parser, TOKEN_LPAREN)) {
            node->left = parser_parse_expression(parser);
            parser_match(parser, TOKEN_RPAREN);
        }
    } else if (token->type == TOKEN_IF) {
        parser_advance(parser);
        node = ast_create_node(NODE_IF_STMT);
        
        node->condition = parser_parse_expression(parser);
        node->then_branch = parser_parse_block(parser);
        
        if (parser_match(parser, TOKEN_ELSE)) {
            if (parser_current_token(parser)->type == TOKEN_IF) {
                // else if
                node->else_branch = parser_parse_statement(parser);
            } else {
                // else
                node->else_branch = parser_parse_block(parser);
            }
        }
    } else if (token->type == TOKEN_RETURN) {
        parser_advance(parser);
        node = ast_create_node(NODE_RETURN_STMT);
        
        if (parser_current_token(parser)->type != TOKEN_NEWLINE &&
            parser_current_token(parser)->type != TOKEN_SEMICOLON &&
            parser_current_token(parser)->type != TOKEN_EOF) {
            node->left = parser_parse_expression(parser);
        }
    } else {
        // Expression statement
        node = parser_parse_expression(parser);
    }
    
    // Skip newlines and semicolons
    while (parser_match(parser, TOKEN_NEWLINE) || parser_match(parser, TOKEN_SEMICOLON)) {
        // Skip
    }
    
    return node;
}

ASTNode* parser_parse(Parser* parser) {
    ASTNode* program = ast_create_node(NODE_PROGRAM);
    program->children = malloc(sizeof(ASTNode*) * 200);
    program->child_count = 0;
    
    while (parser_current_token(parser)->type != TOKEN_EOF) {
        // Skip newlines at top level
        if (parser_match(parser, TOKEN_NEWLINE)) {
            continue;
        }
        
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt && program->child_count < 200) {
            program->children[program->child_count++] = stmt;
        }
    }
    
    return program;
}

void parser_free(Parser* parser) {
    free(parser);
}

// ============================================================================
// ENHANCED COMPILER WITH FUNCTION SUPPORT
// ============================================================================

Compiler* compiler_create(FILE* output, bool to_rust) {
    Compiler* compiler = malloc(sizeof(Compiler));
    compiler->output = output;
    compiler->to_rust = to_rust;
    return compiler;
}

static void compiler_emit_c_headers(Compiler* compiler) {
    fprintf(compiler->output, "#include <stdio.h>\n");
    fprintf(compiler->output, "#include <stdlib.h>\n");
    fprintf(compiler->output, "#include <string.h>\n\n");
}

static void compiler_emit_rust_headers(Compiler* compiler) {
    // Rust functions will be emitted first, then main
}

static void compiler_compile_node(Compiler* compiler, ASTNode* node);

static void compiler_compile_function(Compiler* compiler, ASTNode* func) {
    if (compiler->to_rust) {
        fprintf(compiler->output, "fn %s() -> i32 {\n", func->value);
    } else {
        fprintf(compiler->output, "int %s() {\n", func->value);
    }
    
    // Compile function body
    if (func->left) {
        in_function = true;
        for (int i = 0; i < func->left->child_count; i++) {
            compiler_compile_node(compiler, func->left->children[i]);
        }
        in_function = false;
    }
    
    // Default return if no explicit return
    if (compiler->to_rust) {
        fprintf(compiler->output, "    0\n");
    } else {
        fprintf(compiler->output, "    return 0;\n");
    }
    fprintf(compiler->output, "}\n\n");
}

static void compiler_compile_node(Compiler* compiler, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_PROGRAM:
            if (!in_function) {
                if (compiler->to_rust) {
                    for (int i = 0; i < node->child_count; i++) {
                        if (node->children[i]->type == NODE_FUNC_DECL) {
                            compiler_compile_function(compiler, node->children[i]);
                        }
                    }
                    fprintf(compiler->output, "fn main() {\n");
                } else {
                    compiler_emit_c_headers(compiler);
                    for (int i = 0; i < node->child_count; i++) {
                        if (node->children[i]->type == NODE_FUNC_DECL) {
                            compiler_compile_function(compiler, node->children[i]);
                        }
                    }
                    fprintf(compiler->output, "int main() {\n");
                }
            }
            
            // Emit non-function statements
            for (int i = 0; i < node->child_count; i++) {
                if (node->children[i]->type != NODE_FUNC_DECL) {
                    compiler_compile_node(compiler, node->children[i]);
                }
            }
            
            if (!in_function) {
                if (compiler->to_rust) {
                    fprintf(compiler->output, "}\n");
                } else {
                    fprintf(compiler->output, "    return 0;\n}\n");
                }
            }
            break;
            
        case NODE_FUNC_DECL:
            break;
            
        case NODE_VAR_DECL:
            if (compiler->to_rust) {
                fprintf(compiler->output, "    let %s = ", node->value);
            } else {
                fprintf(compiler->output, "    int %s = ", node->value);
            }
            if (node->right) {
                compiler_compile_node(compiler, node->right);
            } else {
                fprintf(compiler->output, "0");
            }
            fprintf(compiler->output, ";\n");
            break;
            
        case NODE_PRINT_STMT:
            if (compiler->to_rust) {
                fprintf(compiler->output, "    println!(\"{}\"");
            } else {
                fprintf(compiler->output, "    printf(\"%%d\\n\", ");
            }
            if (node->left) {
                if (compiler->to_rust) fprintf(compiler->output, ", ");
                compiler_compile_node(compiler, node->left);
            }
            fprintf(compiler->output, ");\n");
            break;
            
        case NODE_IF_STMT:
            if (compiler->to_rust) {
                fprintf(compiler->output, "    if ");
            } else {
                fprintf(compiler->output, "    if (");
            }
            compiler_compile_node(compiler, node->condition);
            if (!compiler->to_rust) {
                fprintf(compiler->output, ")");
            }
            fprintf(compiler->output, " {\n");
            
            if (node->then_branch) {
                for (int i = 0; i < node->then_branch->child_count; i++) {
                    fprintf(compiler->output, "    ");
                    compiler_compile_node(compiler, node->then_branch->children[i]);
                }
            }
            
            fprintf(compiler->output, "    }");
            
            if (node->else_branch) {
                fprintf(compiler->output, " else {\n");
                if (node->else_branch->type == NODE_IF_STMT) {
                    fprintf(compiler->output, "    ");
                    compiler_compile_node(compiler, node->else_branch);
                } else {
                    for (int i = 0; i < node->else_branch->child_count; i++) {
                        fprintf(compiler->output, "    ");
                        compiler_compile_node(compiler, node->else_branch->children[i]);
                    }
                }
                fprintf(compiler->output, "    }");
            }
            fprintf(compiler->output, "\n");
            break;
            
        case NODE_RETURN_STMT:
            if (compiler->to_rust) {
                fprintf(compiler->output, "    return ");
            } else {
                fprintf(compiler->output, "    return ");
            }
            if (node->left) {
                compiler_compile_node(compiler, node->left);
            } else {
                fprintf(compiler->output, "0");
            }
            fprintf(compiler->output, ";\n");
            break;
            
        case NODE_BINARY_OP:
            compiler_compile_node(compiler, node->left);
            fprintf(compiler->output, " %s ", node->value);
            compiler_compile_node(compiler, node->right);
            break;
            
        case NODE_NUMBER:
            fprintf(compiler->output, "%s", node->value);
            break;
            
        case NODE_STRING:
            if (compiler->to_rust) {
                fprintf(compiler->output, "\"%s\"", node->value);
            } else {
                fprintf(compiler->output, "\"%s\"", node->value);
            }
            break;
            
        case NODE_IDENTIFIER:
            fprintf(compiler->output, "%s", node->value);
            break;
            
        case NODE_FUNC_CALL:
            fprintf(compiler->output, "%s()", node->value);
            break;
            
        default:
            break;
    }
}

void compiler_compile(Compiler* compiler, ASTNode* ast) {
    compiler_compile_node(compiler, ast);
}

void compiler_free(Compiler* compiler) {
    free(compiler);
}

// ============================================================================
// ENHANCED MAIN FUNCTION
// ============================================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "So Lang Enhanced Compiler v2.0\n");
        fprintf(stderr, "Usage: %s <input.so> [--rust] [--bootstrap]\n", argv[0]);
        fprintf(stderr, "  --rust      Compile to Rust instead of C\n");
        fprintf(stderr, "  --bootstrap Compile the bootstrap compiler\n");
        return 1;
    }
    
    bool to_rust = false;
    bool bootstrap = false;
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--rust") == 0) {
            to_rust = true;
        } else if (strcmp(argv[i], "--bootstrap") == 0) {
            bootstrap = true;
        }
    }
    
    // Read source file
    char* source = read_file(argv[1]);
    if (!source) return 1;
    
    printf("So Lang Enhanced Compiler v2.0\n");
    printf("Features: Functions, Enhanced Syntax, Self-hosting\n");
    printf("Compiling: %s\n", argv[1]);
    
    if (bootstrap) {
        printf("Bootstrap mode: Compiling self-hosting compiler\n");
    }
    
    // Tokenize
    Lexer* lexer = lexer_create(source);
    lexer_tokenize(lexer);
    
    if (has_error) {
        lexer_free(lexer);
        free(source);
        return 1;
    }
    
    printf("✓ Lexical analysis complete (%d tokens)\n", lexer->token_count);
    
    // Parse
    Parser* parser = parser_create(lexer->tokens, lexer->token_count);
    ASTNode* ast = parser_parse(parser);
    
    if (has_error) {
        parser_free(parser);
        lexer_free(lexer);
        ast_free(ast);
        free(source);
        return 1;
    }
    
    printf("✓ Syntax analysis complete (%d functions found)\n", function_count);
    
    // Compile
    const char* output_ext = to_rust ? ".rs" : ".c";
    char output_filename[256];
    if (bootstrap) {
        snprintf(output_filename, sizeof(output_filename), "solang_self_hosted%s", output_ext);
    } else {
        snprintf(output_filename, sizeof(output_filename), "output%s", output_ext);
    }
    
    FILE* output_file = fopen(output_filename, "w");
    if (!output_file) {
        fprintf(stderr, "Could not create output file: %s\n", output_filename);
        return 1;
    }
    
    Compiler* compiler = compiler_create(output_file, to_rust);
    compiler_compile(compiler, ast);
    
    printf("✓ Code generation complete\n");
    printf("Generated: %s\n", output_filename);
    
    if (to_rust) {
        printf("To build: rustc %s -o program\n", output_filename);
    } else {
        printf("To build: gcc %s -o program\n", output_filename);
    }
    
    // Cleanup
    fclose(output_file);
    compiler_free(compiler);
    parser_free(parser);
    lexer_free(lexer);
    ast_free(ast);
    free(source);
    
    if (function_names) {
        for (int i = 0; i < function_count; i++) {
            free(function_names[i]);
        }
        free(function_names);
    }
    
    return 0;
}