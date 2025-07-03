/*
 * so_lang_solana.c - So Lang Solana Extensions Implementation
 * Compile So Lang to Solana Rust programs
 */

#include "so_lang_solana.h"

// Global Solana compiler state
static SolanaCompiler* current_solana_compiler = NULL;
static bool in_program_context = false;
static bool in_instruction_handler = false;

// ============================================================================
// SOLANA AST FUNCTIONS
// ============================================================================

SolanaASTNode* solana_ast_create_node(NodeType type) {
    SolanaASTNode* node = malloc(sizeof(SolanaASTNode));
    node->type = type;
    node->value[0] = '\0';
    node->left = NULL;
    node->right = NULL;
    node->condition = NULL;
    node->then_branch = NULL;
    node->else_branch = NULL;
    node->children = NULL;
    node->child_count = 0;
    
    node->solana_type = SOLANA_TYPE_U64;
    node->constraint_type = CONSTRAINT_SIGNER;
    node->account_name = NULL;
    node->instruction_name = NULL;
    node->program_id = NULL;
    node->is_signer = false;
    node->is_writable = false;
    node->is_init = false;
    node->seeds = NULL;
    node->seed_count = 0;
    node->bump = 0;
    
    return node;
}

void solana_ast_free(SolanaASTNode* node) {
    if (!node) return;
    
    solana_ast_free((SolanaASTNode*)node->left);
    solana_ast_free((SolanaASTNode*)node->right);
    solana_ast_free((SolanaASTNode*)node->condition);
    solana_ast_free((SolanaASTNode*)node->then_branch);
    solana_ast_free((SolanaASTNode*)node->else_branch);
    
    if (node->children) {
        for (int i = 0; i < node->child_count; i++) {
            solana_ast_free((SolanaASTNode*)node->children[i]);
        }
        free(node->children);
    }
    
    if (node->account_name) free(node->account_name);
    if (node->instruction_name) free(node->instruction_name);
    if (node->program_id) free(node->program_id);
    if (node->seeds) {
        for (int i = 0; i < node->seed_count; i++) {
            free(node->seeds[i]);
        }
        free(node->seeds);
    }
    
    free(node);
}

// ============================================================================
// ENHANCED LEXER FOR SOLANA
// ============================================================================

static void solana_lexer_read_attribute(Lexer* lexer) {
    char buffer[MAX_TOKEN_LEN];
    int i = 0;
    
    lexer_advance(lexer); // Skip '@'
    
    while (isalnum(lexer_current_char(lexer)) || lexer_current_char(lexer) == '_') {
        if (i < MAX_TOKEN_LEN - 1) {
            buffer[i++] = lexer_current_char(lexer);
        }
        lexer_advance(lexer);
    }
    
    buffer[i] = '\0';
    
    if (strcmp(buffer, "program") == 0) {
        lexer_add_token(lexer, TOKEN_PROGRAM, buffer);
    } else if (strcmp(buffer, "instruction") == 0) {
        lexer_add_token(lexer, TOKEN_INSTRUCTION, buffer);
    } else if (strcmp(buffer, "account") == 0) {
        lexer_add_token(lexer, TOKEN_ACCOUNT, buffer);
    } else if (strcmp(buffer, "signer") == 0) {
        lexer_add_token(lexer, TOKEN_SIGNER, buffer);
    } else if (strcmp(buffer, "writable") == 0) {
        lexer_add_token(lexer, TOKEN_WRITABLE, buffer);
    } else if (strcmp(buffer, "init") == 0) {
        lexer_add_token(lexer, TOKEN_INIT, buffer);
    } else {
        lexer_add_token(lexer, TOKEN_IDENTIFIER, buffer);
    }
}

static void solana_lexer_read_solana_identifier(Lexer* lexer) {
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
    if (strcmp(buffer, "program") == 0) type = TOKEN_PROGRAM;
    else if (strcmp(buffer, "instruction") == 0) type = TOKEN_INSTRUCTION;
    else if (strcmp(buffer, "account") == 0) type = TOKEN_ACCOUNT;
    else if (strcmp(buffer, "state") == 0) type = TOKEN_STATE;
    else if (strcmp(buffer, "pubkey") == 0) type = TOKEN_PUBKEY;
    else if (strcmp(buffer, "lamports") == 0) type = TOKEN_LAMPORTS;
    else if (strcmp(buffer, "signer") == 0) type = TOKEN_SIGNER;
    else if (strcmp(buffer, "writable") == 0) type = TOKEN_WRITABLE;
    else if (strcmp(buffer, "init") == 0) type = TOKEN_INIT;
    else if (strcmp(buffer, "seeds") == 0) type = TOKEN_SEEDS;
    else if (strcmp(buffer, "bump") == 0) type = TOKEN_BUMP;
    else if (strcmp(buffer, "pda") == 0) type = TOKEN_PDA;
    else if (strcmp(buffer, "transfer") == 0) type = TOKEN_TRANSFER;
    else if (strcmp(buffer, "invoke") == 0) type = TOKEN_INVOKE;
    else if (strcmp(buffer, "require") == 0) type = TOKEN_REQUIRE;
    else if (strcmp(buffer, "error") == 0) type = TOKEN_ERROR;
    else if (strcmp(buffer, "event") == 0) type = TOKEN_EVENT;
    else if (strcmp(buffer, "emit") == 0) type = TOKEN_EMIT;
    else if (strcmp(buffer, "anchor") == 0) type = TOKEN_ANCHOR;
    else if (strcmp(buffer, "solana") == 0) type = TOKEN_SOLANA;
    else if (strcmp(buffer, "entrypoint") == 0) type = TOKEN_ENTRYPOINT;
    
    lexer_add_token(lexer, type, buffer);
}

void solana_lexer_tokenize(Lexer* lexer) {
    while (lexer_current_char(lexer) != '\0') {
        char c = lexer_current_char(lexer);
        
        if (isspace(c) && c != '\n') {
            lexer_skip_whitespace(lexer);
        } else if (c == '\n') {
            lexer_add_token(lexer, TOKEN_NEWLINE, "\n");
            lexer_advance(lexer);
        } else if (c == '@') {
            solana_lexer_read_attribute(lexer);
        } else if (c == '#') {
            lexer_add_token(lexer, TOKEN_HASH, "#");
            lexer_advance(lexer);
        } else if (c == '-' && lexer->source[lexer->pos + 1] == '>') {
            lexer_add_token(lexer, TOKEN_ARROW, "->");
            lexer_advance(lexer);
            lexer_advance(lexer);
        } else if (c == '"') {
            lexer_read_string(lexer);
        } else if (isalpha(c) || c == '_') {
            solana_lexer_read_solana_identifier(lexer);
        } else if (isdigit(c)) {
            lexer_read_number(lexer);
        } else {
            char token_str[2] = {c, '\0'};
            switch (c) {
                case '=': lexer_add_token(lexer, TOKEN_ASSIGN, token_str); break;
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

// ============================================================================
// SOLANA PARSER
// ============================================================================

static SolanaASTNode* solana_parse_program_declaration(Parser* parser) {
    parser_advance(parser); // consume 'program'
    
    SolanaASTNode* program = solana_ast_create_node(NODE_PROGRAM_DECL);
    
    Token* name = parser_current_token(parser);
    if (name->type == TOKEN_IDENTIFIER) {
        strcpy(program->value, name->value);
        parser_advance(parser);
    }
    
    if (parser_match(parser, TOKEN_LPAREN)) {
        Token* id = parser_current_token(parser);
        if (id->type == TOKEN_STRING) {
            program->program_id = malloc(strlen(id->value) + 1);
            strcpy(program->program_id, id->value);
            parser_advance(parser);
        }
        parser_match(parser, TOKEN_RPAREN);
    }
    
    if (parser_match(parser, TOKEN_LBRACE)) {
        program->children = malloc(sizeof(SolanaASTNode*) * 100);
        program->child_count = 0;
        
        in_program_context = true;
        
        while (parser_current_token(parser)->type != TOKEN_RBRACE && 
               parser_current_token(parser)->type != TOKEN_EOF) {
            
            if (parser_match(parser, TOKEN_NEWLINE)) continue;
            
            SolanaASTNode* stmt = (SolanaASTNode*)solana_parser_parse(parser);
            if (stmt && program->child_count < 100) {
                program->children[program->child_count++] = stmt;
            }
        }
        
        in_program_context = false;
        parser_match(parser, TOKEN_RBRACE);
    }
    
    return program;
}

static SolanaASTNode* solana_parse_instruction_declaration(Parser* parser) {
    parser_advance(parser); // consume 'instruction'
    
    SolanaASTNode* instruction = solana_ast_create_node(NODE_INSTRUCTION_DECL);
    
    Token* name = parser_current_token(parser);
    if (name->type == TOKEN_IDENTIFIER) {
        strcpy(instruction->value, name->value);
        instruction->instruction_name = malloc(strlen(name->value) + 1);
        strcpy(instruction->instruction_name, name->value);
        parser_advance(parser);
    }
    
    if (parser_match(parser, TOKEN_LPAREN)) {
        while (parser_current_token(parser)->type != TOKEN_RPAREN &&
               parser_current_token(parser)->type != TOKEN_EOF) {
            parser_advance(parser); // Skip for now
        }
        parser_match(parser, TOKEN_RPAREN);
    }
    
    if (parser_match(parser, TOKEN_LBRACE)) {
        instruction->left = (struct SolanaASTNode*)solana_parser_parse(parser);
        parser_match(parser, TOKEN_RBRACE);
    }
    
    return instruction;
}

static SolanaASTNode* solana_parse_account_declaration(Parser* parser) {
    parser_advance(parser); // consume 'account'
    
    SolanaASTNode* account = solana_ast_create_node(NODE_ACCOUNT_DECL);
    
    Token* name = parser_current_token(parser);
    if (name->type == TOKEN_IDENTIFIER) {
        strcpy(account->value, name->value);
        account->account_name = malloc(strlen(name->value) + 1);
        strcpy(account->account_name, name->value);
        parser_advance(parser);
    }
    
    if (parser_match(parser, TOKEN_LPAREN)) {
        while (parser_current_token(parser)->type != TOKEN_RPAREN &&
               parser_current_token(parser)->type != TOKEN_EOF) {
            
            Token* constraint = parser_current_token(parser);
            if (constraint->type == TOKEN_SIGNER) {
                account->is_signer = true;
            } else if (constraint->type == TOKEN_WRITABLE) {
                account->is_writable = true;
            } else if (constraint->type == TOKEN_INIT) {
                account->is_init = true;
            }
            parser_advance(parser);
            
            if (parser_match(parser, TOKEN_COMMA)) continue;
        }
        parser_match(parser, TOKEN_RPAREN);
    }
    
    if (parser_match(parser, TOKEN_COLON)) {
        Token* type = parser_current_token(parser);
        if (type->type == TOKEN_PUBKEY) {
            account->solana_type = SOLANA_TYPE_PUBKEY;
        } else if (type->type == TOKEN_LAMPORTS) {
            account->solana_type = SOLANA_TYPE_LAMPORTS;
        }
        parser_advance(parser);
    }
    
    return account;
}

static SolanaASTNode* solana_parse_transfer_statement(Parser* parser) {
    parser_advance(parser); // consume 'transfer'
    
    SolanaASTNode* transfer = solana_ast_create_node(NODE_TRANSFER_STMT);
    
    if (parser_match(parser, TOKEN_LPAREN)) {
        transfer->left = (struct SolanaASTNode*)parser_parse_expression(parser);
        
        if (parser_match(parser, TOKEN_COMMA)) {
            transfer->right = (struct SolanaASTNode*)parser_parse_expression(parser);
        }
        
        if (parser_match(parser, TOKEN_COMMA)) {
            transfer->condition = (struct SolanaASTNode*)parser_parse_expression(parser);
        }
        
        parser_match(parser, TOKEN_RPAREN);
    }
    
    return transfer;
}

static SolanaASTNode* solana_parse_require_statement(Parser* parser) {
    parser_advance(parser); // consume 'require'
    
    SolanaASTNode* require_stmt = solana_ast_create_node(NODE_REQUIRE_STMT);
    
    if (parser_match(parser, TOKEN_LPAREN)) {
        require_stmt->condition = (struct SolanaASTNode*)parser_parse_expression(parser);
        
        if (parser_match(parser, TOKEN_COMMA)) {
            Token* error_msg = parser_current_token(parser);
            if (error_msg->type == TOKEN_STRING) {
                strcpy(require_stmt->value, error_msg->value);
                parser_advance(parser);
            }
        }
        
        parser_match(parser, TOKEN_RPAREN);
    }
    
    return require_stmt;
}

SolanaASTNode* solana_parser_parse(Parser* parser) {
    Token* token = parser_current_token(parser);
    
    if (token->type == TOKEN_PROGRAM) {
        return solana_parse_program_declaration(parser);
    } else if (token->type == TOKEN_INSTRUCTION) {
        return solana_parse_instruction_declaration(parser);
    } else if (token->type == TOKEN_ACCOUNT) {
        return solana_parse_account_declaration(parser);
    } else if (token->type == TOKEN_TRANSFER) {
        return solana_parse_transfer_statement(parser);
    } else if (token->type == TOKEN_REQUIRE) {
        return solana_parse_require_statement(parser);
    } else {
        return (SolanaASTNode*)parser_parse_statement(parser);
    }
}

// ============================================================================
// SOLANA COMPILER
// ============================================================================

SolanaCompiler* solana_compiler_create(FILE* output, bool use_anchor) {
    SolanaCompiler* compiler = malloc(sizeof(SolanaCompiler));
    compiler->output = output;
    compiler->use_anchor = use_anchor;
    compiler->native_solana = !use_anchor;
    compiler->program_name = NULL;
    compiler->program_id = NULL;
    compiler->instruction_count = 0;
    compiler->account_count = 0;
    compiler->state_count = 0;
    return compiler;
}

void emit_anchor_imports(SolanaCompiler* compiler) {
    fprintf(compiler->output, "use anchor_lang::prelude::*;\n");
    fprintf(compiler->output, "use anchor_spl::token::{self, Token, TokenAccount, Mint};\n");
    fprintf(compiler->output, "use anchor_spl::associated_token::AssociatedToken;\n");
    fprintf(compiler->output, "\n");
}

void emit_native_solana_imports(SolanaCompiler* compiler) {
    fprintf(compiler->output, "use solana_program::{\n");
    fprintf(compiler->output, "    account_info::{next_account_info, AccountInfo},\n");
    fprintf(compiler->output, "    entrypoint,\n");
    fprintf(compiler->output, "    entrypoint::ProgramResult,\n");
    fprintf(compiler->output, "    msg,\n");
    fprintf(compiler->output, "    program_error::ProgramError,\n");
    fprintf(compiler->output, "    pubkey::Pubkey,\n");
    fprintf(compiler->output, "    system_instruction,\n");
    fprintf(compiler->output, "    program::{invoke, invoke_signed},\n");
    fprintf(compiler->output, "};\n");
    fprintf(compiler->output, "\n");
}

void emit_program_structure(SolanaCompiler* compiler, SolanaASTNode* program) {
    if (compiler->use_anchor) {
        fprintf(compiler->output, "#[program]\n");
        fprintf(compiler->output, "pub mod %s {\n", program->value);
        fprintf(compiler->output, "    use super::*;\n\n");
        
        if (program->program_id) {
            fprintf(compiler->output, "    declare_id!(\"%s\");\n\n", program->program_id);
        }
    } else {
        fprintf(compiler->output, "entrypoint!(process_instruction);\n\n");
        
        if (program->program_id) {
            fprintf(compiler->output, "declare_id!(\"%s\");\n\n", program->program_id);
        }
        
        fprintf(compiler->output, "pub fn process_instruction(\n");
        fprintf(compiler->output, "    program_id: &Pubkey,\n");
        fprintf(compiler->output, "    accounts: &[AccountInfo],\n");
        fprintf(compiler->output, "    instruction_data: &[u8],\n");
        fprintf(compiler->output, ") -> ProgramResult {\n");
    }
}

void emit_instruction_handler(SolanaCompiler* compiler, SolanaASTNode* instruction) {
    if (compiler->use_anchor) {
        fprintf(compiler->output, "    pub fn %s(ctx: Context<%sContext>) -> Result<()> {\n", 
                instruction->instruction_name, instruction->instruction_name);
        
        if (instruction->left) {
            fprintf(compiler->output, "        // Generated instruction logic\n");
            solana_compiler_compile(compiler, instruction->left);
        }
        
        fprintf(compiler->output, "        Ok(())\n");
        fprintf(compiler->output, "    }\n\n");
    } else {
        fprintf(compiler->output, "    match instruction_data[0] {\n");
        fprintf(compiler->output, "        %d => {\n", compiler->instruction_count);
        fprintf(compiler->output, "            msg!(\"Executing %s\");\n", instruction->instruction_name);
        
        if (instruction->left) {
            solana_compiler_compile(compiler, instruction->left);
        }
        
        fprintf(compiler->output, "        },\n");
        fprintf(compiler->output, "    }\n");
    }
    
    compiler->instruction_count++;
}

void emit_account_validation(SolanaCompiler* compiler, SolanaASTNode* accounts) {
    if (compiler->use_anchor) {
        fprintf(compiler->output, "#[derive(Accounts)]\n");
        fprintf(compiler->output, "pub struct %sContext<'info> {\n", accounts->instruction_name);
        
        for (int i = 0; i < accounts->child_count; i++) {
            SolanaASTNode* account = (SolanaASTNode*)accounts->children[i];
            fprintf(compiler->output, "    #[account(");
            
            if (account->is_signer) fprintf(compiler->output, "signer, ");
            if (account->is_writable) fprintf(compiler->output, "mut, ");
            if (account->is_init) fprintf(compiler->output, "init, payer = payer, space = 8 + 32, ");
            
            fprintf(compiler->output, ")]\n");
            fprintf(compiler->output, "    pub %s: Account<'info, ", account->account_name);
            
            switch (account->solana_type) {
                case SOLANA_TYPE_PUBKEY:
                    fprintf(compiler->output, "Pubkey");
                    break;
                case SOLANA_TYPE_ACCOUNT_INFO:
                    fprintf(compiler->output, "AccountInfo");
                    break;
                default:
                    fprintf(compiler->output, "AccountInfo");
                    break;
            }
            
            fprintf(compiler->output, ">,\n");
        }
        
        fprintf(compiler->output, "}\n\n");
    }
}

void emit_state_structure(SolanaCompiler* compiler, SolanaASTNode* state) {
    if (compiler->use_anchor) {
        fprintf(compiler->output, "#[account]\n");
    }
    
    fprintf(compiler->output, "#[derive(Clone, Debug, PartialEq)]\n");
    fprintf(compiler->output, "pub struct %s {\n", state->value);
    
    for (int i = 0; i < state->child_count; i++) {
        SolanaASTNode* field = (SolanaASTNode*)state->children[i];
        fprintf(compiler->output, "    pub %s: ", field->value);
        
        switch (field->solana_type) {
            case SOLANA_TYPE_PUBKEY:
                fprintf(compiler->output, "Pubkey");
                break;
            case SOLANA_TYPE_U64:
                fprintf(compiler->output, "u64");
                break;
            case SOLANA_TYPE_U32:
                fprintf(compiler->output, "u32");
                break;
            case SOLANA_TYPE_BOOL:
                fprintf(compiler->output, "bool");
                break;
            case SOLANA_TYPE_STRING:
                fprintf(compiler->output, "String");
                break;
            default:
                fprintf(compiler->output, "u64");
                break;
        }
        
        fprintf(compiler->output, ",\n");
    }
    
    fprintf(compiler->output, "}\n\n");
}

void emit_error_types(SolanaCompiler* compiler) {
    if (compiler->use_anchor) {
        fprintf(compiler->output, "#[error_code]\n");
        fprintf(compiler->output, "pub enum ErrorCode {\n");
        fprintf(compiler->output, "    #[msg(\"Custom error message\")]\n");
        fprintf(compiler->output, "    CustomError,\n");
        fprintf(compiler->output, "}\n\n");
    }
}

void solana_compiler_compile(SolanaCompiler* compiler, SolanaASTNode* ast) {
    if (!ast) return;
    
    switch (ast->type) {
        case NODE_PROGRAM_DECL:
            if (compiler->use_anchor) {
                emit_anchor_imports(compiler);
            } else {
                emit_native_solana_imports(compiler);
            }
            
            emit_program_structure(compiler, ast);
            
            for (int i = 0; i < ast->child_count; i++) {
                solana_compiler_compile(compiler, (SolanaASTNode*)ast->children[i]);
            }
            
            if (compiler->use_anchor) {
                fprintf(compiler->output, "}\n"); // Close program module
            } else {
                fprintf(compiler->output, "    Ok(())\n");
                fprintf(compiler->output, "}\n"); // Close process_instruction
            }
            break;
            
        case NODE_INSTRUCTION_DECL:
            emit_instruction_handler(compiler, ast);
            break;
            
        case NODE_ACCOUNT_DECL:
            emit_account_validation(compiler, ast);
            break;
            
        case NODE_STATE_DECL:
            emit_state_structure(compiler, ast);
            break;
            
        case NODE_TRANSFER_STMT:
            if (compiler->use_anchor) {
                fprintf(compiler->output, "        token::transfer(\n");
                fprintf(compiler->output, "            CpiContext::new(\n");
                fprintf(compiler->output, "                ctx.accounts.token_program.to_account_info(),\n");
                fprintf(compiler->output, "                token::Transfer {\n");
                fprintf(compiler->output, "                    from: ctx.accounts.from.to_account_info(),\n");
                fprintf(compiler->output, "                    to: ctx.accounts.to.to_account_info(),\n");
                fprintf(compiler->output, "                    authority: ctx.accounts.authority.to_account_info(),\n");
                fprintf(compiler->output, "                },\n");
                fprintf(compiler->output, "            ),\n");
                fprintf(compiler->output, "            amount,\n");
                fprintf(compiler->output, "        )?;\n");
            } else {
                fprintf(compiler->output, "            let instruction = system_instruction::transfer(\n");
                fprintf(compiler->output, "                from.key,\n");
                fprintf(compiler->output, "                to.key,\n");
                fprintf(compiler->output, "                amount,\n");
                fprintf(compiler->output, "            );\n");
                fprintf(compiler->output, "            invoke(&instruction, &[from.clone(), to.clone()])?;\n");
            }
            break;
            
        case NODE_REQUIRE_STMT:
            if (compiler->use_anchor) {
                fprintf(compiler->output, "        require!(");
                if (ast->condition) {
                    solana_compiler_compile(compiler, ast->condition);
                }
                fprintf(compiler->output, ", ErrorCode::CustomError);\n");
            } else {
                fprintf(compiler->output, "            if !(");
                if (ast->condition) {
                    solana_compiler_compile(compiler, ast->condition);
                }
                fprintf(compiler->output, ") {\n");
                fprintf(compiler->output, "                return Err(ProgramError::InvalidArgument);\n");
                fprintf(compiler->output, "            }\n");
            }
            break;
            
        case NODE_PRINT_STMT:
            fprintf(compiler->output, "        msg!(\"");
            if (ast->left) {
                fprintf(compiler->output, "Debug: {}\"");
            }
            fprintf(compiler->output, ");\n");
            break;
            
        default:
            compiler_compile_node((Compiler*)compiler, (ASTNode*)ast);
            break;
    }
}

void solana_compiler_free(SolanaCompiler* compiler) {
    if (compiler->program_name) free(compiler->program_name);
    if (compiler->program_id) free(compiler->program_id);
    free(compiler);
}

// ============================================================================
// VALIDATION FUNCTIONS
// ============================================================================

bool validate_program_structure(SolanaASTNode* ast) {
    if (!ast || ast->type != NODE_PROGRAM_DECL) {
        return false;
    }
    
    bool has_instruction = false;
    for (int i = 0; i < ast->child_count; i++) {
        if (((SolanaASTNode*)ast->children[i])->type == NODE_INSTRUCTION_DECL) {
            has_instruction = true;
            break;
        }
    }
    
    return has_instruction;
}

bool check_account_constraints(SolanaASTNode* accounts) {
    return true; // Simplified for now
}

bool verify_instruction_signatures(SolanaASTNode* instructions) {
    return true; // Simplified for now
}