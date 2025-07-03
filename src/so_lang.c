/*
 * so_lang.c - So Lang Programming Language Implementation
 * A fast, simple toy programming language built in C
 */

#include "so_lang.h"

static bool has_error = false;

static bool detected_solana = false;
static char* detected_program_name = NULL;

// ============================================================================
// UTILITY FUNCTIONS
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
// SOLANA PROGRAM ID GENERATION AND DETECTION
// ============================================================================

bool detect_solana_program(ASTNode* ast) {
    if (!ast) return false;
    
    if (ast->type == NODE_PROGRAM_DECL) {
        detected_solana = true;
        if (detected_program_name) free(detected_program_name);
        detected_program_name = malloc(strlen(ast->value) + 1);
        strcpy(detected_program_name, ast->value);
        return true;
    }
    
    if (ast->type == NODE_INSTRUCTION_DECL || 
        ast->type == NODE_ACCOUNT_CONSTRAINT ||
        ast->type == NODE_TRANSFER_STMT ||
        ast->type == NODE_REQUIRE_STMT ||
        ast->type == NODE_EMIT_STMT) {
        detected_solana = true;
        return true;
    }
    
    for (int i = 0; i < ast->child_count; i++) {
        if (detect_solana_program(ast->children[i])) {
            return true;
        }
    }
    
    return false;
}

char* generate_program_id(const char* program_name) {
    system("mkdir -p keypairs");
    
    char keypair_path[512];
    snprintf(keypair_path, sizeof(keypair_path), "keypairs/%s-keypair.json", program_name);
    
    FILE* check = fopen(keypair_path, "r");
    if (check) {
        fclose(check);
        printf("Found existing keypair for %s\n", program_name);
    } else {
        char command[1024];
        snprintf(command, sizeof(command), 
                "solana-keygen new --no-passphrase --outfile %s", keypair_path);
        
        printf("Generating new program keypair for %s...\n", program_name);
        int result = system(command);
        if (result != 0) {
            fprintf(stderr, "Failed to generate keypair. Is Solana CLI installed?\n");
            fprintf(stderr, "Install with: sh -c \"$(curl -sSfL https://release.solana.com/v1.16.0/install)\"\n");
            return NULL;
        }
    }
    
    char command[1024];
    snprintf(command, sizeof(command), "solana-keygen pubkey %s", keypair_path);
    
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        fprintf(stderr, "Failed to get program ID from keypair\n");
        return NULL;
    }
    
    char* program_id = malloc(45); // Base58 pubkey is 44 chars + null terminator
    if (fgets(program_id, 45, pipe) != NULL) {
        char* newline = strchr(program_id, '\n');
        if (newline) *newline = '\0';
    } else {
        free(program_id);
        program_id = NULL;
    }
    
    pclose(pipe);
    
    if (program_id) {
        printf("Program ID for %s: %s\n", program_name, program_id);
    }
    
    return program_id;
}

char* get_or_create_program_keypair(const char* program_name) {
    return generate_program_id(program_name);
}

void validate_program_id(const char* program_id) {
    if (!program_id) return;
    
    size_t len = strlen(program_id);
    if (len < 32 || len > 44) {
        fprintf(stderr, "Warning: Program ID length seems invalid (%zu chars)\n", len);
        return;
    }
    
    const char* valid_chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    for (size_t i = 0; i < len; i++) {
        if (!strchr(valid_chars, program_id[i])) {
            fprintf(stderr, "Warning: Program ID contains invalid character: %c\n", program_id[i]);
            return;
        }
    }
    
    printf("Program ID validation passed: %s\n", program_id);
}

// ============================================================================
// LEXER IMPLEMENTATION
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
    
    lexer_advance(lexer); // Skip opening quote
    
    while (lexer_current_char(lexer) != '"' && lexer_current_char(lexer) != '\0') {
        if (i < MAX_TOKEN_LEN - 1) {
            buffer[i++] = lexer_current_char(lexer);
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
    else if (strcmp(buffer, "program") == 0) type = TOKEN_PROGRAM;
    else if (strcmp(buffer, "instruction") == 0) type = TOKEN_INSTRUCTION;
    else if (strcmp(buffer, "account") == 0) type = TOKEN_ACCOUNT;
    else if (strcmp(buffer, "state") == 0) type = TOKEN_STATE;
    else if (strcmp(buffer, "pubkey") == 0) type = TOKEN_PUBKEY;
    else if (strcmp(buffer, "signer") == 0) type = TOKEN_SIGNER;
    else if (strcmp(buffer, "writable") == 0) type = TOKEN_WRITABLE;
    else if (strcmp(buffer, "init") == 0) type = TOKEN_INIT;
    else if (strcmp(buffer, "seeds") == 0) type = TOKEN_SEEDS;
    else if (strcmp(buffer, "bump") == 0) type = TOKEN_BUMP;
    else if (strcmp(buffer, "transfer") == 0) type = TOKEN_TRANSFER;
    else if (strcmp(buffer, "require") == 0) type = TOKEN_REQUIRE;
    else if (strcmp(buffer, "emit") == 0) type = TOKEN_EMIT;
    
    lexer_add_token(lexer, type, buffer);
}

static void lexer_read_number(Lexer* lexer) {
    char buffer[MAX_TOKEN_LEN];
    int i = 0;
    
    while (isdigit(lexer_current_char(lexer)) || lexer_current_char(lexer) == '.') {
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
                case '@': lexer_add_token(lexer, TOKEN_AT_SYMBOL, token_str); break;
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
// AST IMPLEMENTATION
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
    
    node->program_id = NULL;
    node->is_signer = false;
    node->is_writable = false;
    node->is_init = false;
    node->seeds = NULL;
    node->seed_count = 0;
    
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
    
    if (node->program_id) {
        free(node->program_id);
    }
    if (node->seeds) {
        for (int i = 0; i < node->seed_count; i++) {
            free(node->seeds[i]);
        }
        free(node->seeds);
    }
    
    free(node);
}

// ============================================================================
// PARSER IMPLEMENTATION
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

static ASTNode* parser_parse_program_declaration(Parser* parser);
static ASTNode* parser_parse_instruction_declaration(Parser* parser);
static ASTNode* parser_parse_account_constraint(Parser* parser);

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
        
        if (parser_current_token(parser)->type == TOKEN_LPAREN) {
            node->type = NODE_FUNC_CALL;
            parser_advance(parser); // Skip '('
            
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

static ASTNode* parser_parse_statement(Parser* parser) {
    Token* token = parser_current_token(parser);
    ASTNode* node = NULL;
    
    if (token->type == TOKEN_PROGRAM) {
        node = parser_parse_program_declaration(parser);
    } else if (token->type == TOKEN_INSTRUCTION) {
        node = parser_parse_instruction_declaration(parser);
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
        
        if (parser_match(parser, TOKEN_LBRACE)) {
            node->then_branch = parser_parse_statement(parser);
            parser_match(parser, TOKEN_RBRACE);
            
            if (parser_match(parser, TOKEN_ELSE)) {
                if (parser_match(parser, TOKEN_LBRACE)) {
                    node->else_branch = parser_parse_statement(parser);
                    parser_match(parser, TOKEN_RBRACE);
                }
            }
        }
    } else if (token->type == TOKEN_RETURN) {
        parser_advance(parser);
        node = ast_create_node(NODE_RETURN_STMT);
        node->left = parser_parse_expression(parser);
    } else {
        node = parser_parse_expression(parser);
    }
    
    while (parser_match(parser, TOKEN_NEWLINE) || parser_match(parser, TOKEN_SEMICOLON)) {
        // Skip
    }
    
    return node;
}

ASTNode* parser_parse(Parser* parser) {
    ASTNode* program = ast_create_node(NODE_PROGRAM);
    program->children = malloc(sizeof(ASTNode*) * 100);
    program->child_count = 0;
    
    while (parser_current_token(parser)->type != TOKEN_EOF) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            program->children[program->child_count++] = stmt;
        }
    }
    
    return program;
}

void parser_free(Parser* parser) {
    free(parser);
}

// ============================================================================
// COMPILER IMPLEMENTATION
// ============================================================================

Compiler* compiler_create(FILE* output, bool to_rust) {
    Compiler* compiler = malloc(sizeof(Compiler));
    compiler->output = output;
    compiler->to_rust = to_rust;
    compiler->is_solana_program = false;
    compiler->use_anchor = false;
    compiler->detected_program_id = NULL;
    return compiler;
}

static void compiler_emit_c_headers(Compiler* compiler) {
    fprintf(compiler->output, "#include <stdio.h>\n");
    fprintf(compiler->output, "#include <stdlib.h>\n");
    fprintf(compiler->output, "#include <string.h>\n\n");
}

static void compiler_emit_rust_headers(Compiler* compiler) {
    if (compiler->is_solana_program) {
        if (compiler->use_anchor) {
            fprintf(compiler->output, "use anchor_lang::prelude::*;\n");
            fprintf(compiler->output, "use anchor_spl::token::{self, Token, TokenAccount, Mint};\n\n");
        } else {
            fprintf(compiler->output, "use solana_program::{\n");
            fprintf(compiler->output, "    account_info::{next_account_info, AccountInfo},\n");
            fprintf(compiler->output, "    entrypoint,\n");
            fprintf(compiler->output, "    entrypoint::ProgramResult,\n");
            fprintf(compiler->output, "    msg,\n");
            fprintf(compiler->output, "    program_error::ProgramError,\n");
            fprintf(compiler->output, "    pubkey::Pubkey,\n");
            fprintf(compiler->output, "};\n\n");
            fprintf(compiler->output, "entrypoint!(process_instruction);\n\n");
        }
        
        if (compiler->detected_program_id) {
            fprintf(compiler->output, "declare_id!(\"%s\");\n\n", compiler->detected_program_id);
        }
    } else {
        fprintf(compiler->output, "fn main() {\n");
    }
}

static void compiler_compile_node(Compiler* compiler, ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_PROGRAM:
            compiler->is_solana_program = detect_solana_program(node);
            
            if (compiler->to_rust) {
                if (compiler->is_solana_program) {
                    compiler_emit_rust_headers(compiler);
                } else {
                    compiler_emit_rust_headers(compiler);
                }
            } else {
                compiler_emit_c_headers(compiler);
                fprintf(compiler->output, "int main() {\n");
            }
            
            for (int i = 0; i < node->child_count; i++) {
                compiler_compile_node(compiler, node->children[i]);
            }
            
            if (compiler->to_rust) {
                if (!compiler->is_solana_program) {
                    fprintf(compiler->output, "}\n");
                }
            } else {
                fprintf(compiler->output, "    return 0;\n}\n");
            }
            break;
            
        case NODE_PROGRAM_DECL:
            compiler->is_solana_program = true;
            if (node->program_id) {
                compiler->detected_program_id = malloc(strlen(node->program_id) + 1);
                strcpy(compiler->detected_program_id, node->program_id);
            }
            
            if (compiler->use_anchor) {
                fprintf(compiler->output, "#[program]\n");
                fprintf(compiler->output, "pub mod %s {\n", node->value);
                fprintf(compiler->output, "    use super::*;\n\n");
            } else {
                fprintf(compiler->output, "pub fn process_instruction(\n");
                fprintf(compiler->output, "    program_id: &Pubkey,\n");
                fprintf(compiler->output, "    accounts: &[AccountInfo],\n");
                fprintf(compiler->output, "    instruction_data: &[u8],\n");
                fprintf(compiler->output, ") -> ProgramResult {\n");
            }
            
            for (int i = 0; i < node->child_count; i++) {
                compiler_compile_node(compiler, node->children[i]);
            }
            
            if (compiler->use_anchor) {
                fprintf(compiler->output, "}\n");
            } else {
                fprintf(compiler->output, "    Ok(())\n");
                fprintf(compiler->output, "}\n");
            }
            break;
            
        case NODE_INSTRUCTION_DECL:
            if (compiler->use_anchor) {
                fprintf(compiler->output, "    pub fn %s(ctx: Context<%sContext>) -> Result<()> {\n",
                        node->value, node->value);
                
                if (node->left) {
                    compiler_compile_node(compiler, node->left);
                }
                
                fprintf(compiler->output, "        Ok(())\n");
                fprintf(compiler->output, "    }\n\n");
            } else {
                fprintf(compiler->output, "    // Instruction: %s\n", node->value);
                fprintf(compiler->output, "    msg!(\"Executing %s\");\n", node->value);
                
                if (node->left) {
                    compiler_compile_node(compiler, node->left);
                }
            }
            break;
            
        case NODE_ACCOUNT_CONSTRAINT:
            break;
        case NODE_PROGRAM:
            if (compiler->to_rust) {
                compiler_emit_rust_headers(compiler);
            } else {
                compiler_emit_c_headers(compiler);
                fprintf(compiler->output, "int main() {\n");
            }
            
            for (int i = 0; i < node->child_count; i++) {
                compiler_compile_node(compiler, node->children[i]);
            }
            
            if (compiler->to_rust) {
                fprintf(compiler->output, "}\n");
            } else {
                fprintf(compiler->output, "    return 0;\n}\n");
            }
            break;
            
        case NODE_VAR_DECL:
            if (compiler->to_rust) {
                fprintf(compiler->output, "    let %s = ", node->value);
            } else {
                fprintf(compiler->output, "    int %s = ", node->value);
            }
            compiler_compile_node(compiler, node->right);
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
            
        case NODE_BINARY_OP:
            compiler_compile_node(compiler, node->left);
            fprintf(compiler->output, " %s ", node->value);
            compiler_compile_node(compiler, node->right);
            break;
            
        case NODE_NUMBER:
            fprintf(compiler->output, "%s", node->value);
            break;
            
        case NODE_STRING:
            fprintf(compiler->output, "\"%s\"", node->value);
            break;
            
        case NODE_IDENTIFIER:
            fprintf(compiler->output, "%s", node->value);
            break;
            
        default:
            break;
    }
}

void compiler_compile(Compiler* compiler, ASTNode* ast) {
    compiler_compile_node(compiler, ast);
}

void compiler_free(Compiler* compiler) {
    if (compiler->detected_program_id) {
        free(compiler->detected_program_id);
    }
    free(compiler);
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "So Lang Compiler v2.0 with Solana Support\n");
        fprintf(stderr, "Usage: %s <input.so> [options]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --rust           Compile to Rust\n");
        fprintf(stderr, "  --solana         Force Solana program compilation\n");
        fprintf(stderr, "  --anchor         Use Anchor framework (implies --solana --rust)\n");
        fprintf(stderr, "  --native-solana  Use native Solana (implies --solana --rust)\n");
        fprintf(stderr, "  --output FILE    Specify output file\n");
        return 1;
    }
    
    bool to_rust = false;
    bool force_solana = false;
    bool use_anchor = false;
    char* output_file = NULL;
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--rust") == 0) {
            to_rust = true;
        } else if (strcmp(argv[i], "--solana") == 0) {
            force_solana = true;
            to_rust = true;
        } else if (strcmp(argv[i], "--anchor") == 0) {
            force_solana = true;
            to_rust = true;
            use_anchor = true;
        } else if (strcmp(argv[i], "--native-solana") == 0) {
            force_solana = true;
            to_rust = true;
            use_anchor = false;
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
    }
    
    char* source = read_file(argv[1]);
    if (!source) return 1;
    
    printf("So Lang Compiler v2.0 with Solana Support\n");
    printf("Compiling: %s\n", argv[1]);
    
    Lexer* lexer = lexer_create(source);
    lexer_tokenize(lexer);
    
    if (has_error) {
        lexer_free(lexer);
        free(source);
        return 1;
    }
    
    printf("✓ Lexical analysis complete (%d tokens)\n", lexer->token_count);
    
    Parser* parser = parser_create(lexer->tokens, lexer->token_count);
    ASTNode* ast = parser_parse(parser);
    
    if (has_error) {
        parser_free(parser);
        lexer_free(lexer);
        ast_free(ast);
        free(source);
        return 1;
    }
    
    bool is_solana = force_solana || detect_solana_program(ast);
    
    if (is_solana) {
        printf("✓ Detected Solana program\n");
        if (detected_program_name) {
            printf("  Program name: %s\n", detected_program_name);
        }
        to_rust = true; // Solana programs must compile to Rust
    }
    
    printf("✓ Syntax analysis complete\n");
    
    if (!output_file) {
        if (is_solana) {
            if (use_anchor) {
                output_file = "lib.rs";
            } else {
                output_file = "program.rs";
            }
        } else {
            output_file = to_rust ? "output.rs" : "output.c";
        }
    }
    
    FILE* output_fp = fopen(output_file, "w");
    if (!output_fp) {
        fprintf(stderr, "Could not create output file: %s\n", output_file);
        return 1;
    }
    
    Compiler* compiler = compiler_create(output_fp, to_rust);
    compiler->is_solana_program = is_solana;
    compiler->use_anchor = use_anchor;
    
    compiler_compile(compiler, ast);
    
    printf("✓ Code generation complete\n");
    printf("Generated: %s\n", output_file);
    
    if (is_solana) {
        printf("\nSolana Program Details:\n");
        if (compiler->detected_program_id) {
            printf("  Program ID: %s\n", compiler->detected_program_id);
        }
        printf("  Framework: %s\n", use_anchor ? "Anchor" : "Native Solana");
        printf("  Keypair: keypairs/%s-keypair.json\n", 
               detected_program_name ? detected_program_name : "program");
        
        printf("\nNext steps:\n");
        if (use_anchor) {
            printf("  1. Create Anchor project: anchor init my_project\n");
            printf("  2. Replace programs/my_project/src/lib.rs with generated code\n");
            printf("  3. Build: anchor build\n");
            printf("  4. Deploy: anchor deploy\n");
        } else {
            printf("  1. Create Cargo project with solana-program dependency\n");
            printf("  2. Build: cargo build-bpf\n");
            printf("  3. Deploy: solana program deploy target/deploy/program.so\n");
        }
    } else if (to_rust) {
        printf("To build: rustc %s -o program\n", output_file);
    } else {
        printf("To build: gcc %s -o program\n", output_file);
    }
    
    // Cleanup
    fclose(output_fp);
    compiler_free(compiler);
    parser_free(parser);
    lexer_free(lexer);
    ast_free(ast);
    free(source);
    
    if (detected_program_name) {
        free(detected_program_name);
    }
    
    return 0;
}