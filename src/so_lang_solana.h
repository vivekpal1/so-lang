/*
 * so_lang_solana.h - So Lang Solana Extensions Header
 * Extended So Lang for Solana program development
 */

#ifndef SO_LANG_SOLANA_H
#define SO_LANG_SOLANA_H

#include "so_lang.h"

typedef enum {
    // Existing tokens...
    TOKEN_PROGRAM = 50,
    TOKEN_INSTRUCTION,
    TOKEN_ACCOUNT,
    TOKEN_STATE,
    TOKEN_PUBKEY,
    TOKEN_LAMPORTS,
    TOKEN_SIGNER,
    TOKEN_WRITABLE,
    TOKEN_INIT,
    TOKEN_SEEDS,
    TOKEN_BUMP,
    TOKEN_PDA,
    TOKEN_TRANSFER,
    TOKEN_INVOKE,
    TOKEN_REQUIRE,
    TOKEN_ERROR,
    TOKEN_EVENT,
    TOKEN_EMIT,
    TOKEN_ANCHOR,
    TOKEN_SOLANA,
    TOKEN_ENTRYPOINT,
    TOKEN_PROCESSOR,
    TOKEN_ACCOUNTS,
    TOKEN_DATA,
    TOKEN_INSTRUCTION_DATA,
    TOKEN_SYSTEM_PROGRAM,
    TOKEN_TOKEN_PROGRAM,
    TOKEN_RENT,
    TOKEN_CLOCK,
    TOKEN_AT_SYMBOL,     // @
    TOKEN_HASH,          // #
    TOKEN_ARROW          // ->
} SolanaTokenType;

typedef enum {
    NODE_PROGRAM_DECL = 200,
    NODE_INSTRUCTION_DECL,
    NODE_ACCOUNT_DECL,
    NODE_STATE_DECL,
    NODE_ACCOUNT_CONSTRAINT,
    NODE_PDA_DERIVATION,
    NODE_TRANSFER_STMT,
    NODE_INVOKE_STMT,
    NODE_REQUIRE_STMT,
    NODE_ERROR_DECL,
    NODE_EVENT_DECL,
    NODE_EMIT_STMT,
    NODE_ACCOUNT_ACCESS,
    NODE_INSTRUCTION_HANDLER,
    NODE_ACCOUNT_VALIDATION,
    NODE_SOLANA_TYPE,
    NODE_ANCHOR_ATTRIBUTE,
    NODE_SEEDS_EXPR,
    NODE_BUMP_EXPR
} SolanaNodeType;

typedef enum {
    CONSTRAINT_SIGNER,
    CONSTRAINT_WRITABLE,
    CONSTRAINT_INIT,
    CONSTRAINT_SEEDS,
    CONSTRAINT_BUMP,
    CONSTRAINT_OWNER,
    CONSTRAINT_RENT_EXEMPT,
    CONSTRAINT_TOKEN_MINT,
    CONSTRAINT_TOKEN_AUTHORITY
} ConstraintType;

// Solana data types
typedef enum {
    SOLANA_TYPE_PUBKEY,
    SOLANA_TYPE_LAMPORTS,
    SOLANA_TYPE_U64,
    SOLANA_TYPE_U32,
    SOLANA_TYPE_U8,
    SOLANA_TYPE_STRING,
    SOLANA_TYPE_BOOL,
    SOLANA_TYPE_ACCOUNT_INFO,
    SOLANA_TYPE_INSTRUCTION,
    SOLANA_TYPE_PROGRAM_ID
} SolanaDataType;

typedef struct SolanaASTNode {
    NodeType type;
    char value[MAX_TOKEN_LEN];
    struct SolanaASTNode* left;
    struct SolanaASTNode* right;
    struct SolanaASTNode* condition;
    struct SolanaASTNode* then_branch;
    struct SolanaASTNode* else_branch;
    struct SolanaASTNode** children;
    int child_count;
    
    // Solana-specific fields
    SolanaDataType solana_type;
    ConstraintType constraint_type;
    char* account_name;
    char* instruction_name;
    char* program_id;
    bool is_signer;
    bool is_writable;
    bool is_init;
    char** seeds;
    int seed_count;
    int bump;
} SolanaASTNode;

typedef struct {
    FILE* output;
    bool use_anchor;
    bool native_solana;
    char* program_name;
    char* program_id;
    int instruction_count;
    int account_count;
    int state_count;
} SolanaCompiler;

SolanaASTNode* solana_ast_create_node(NodeType type);
void solana_ast_free(SolanaASTNode* node);

void solana_lexer_tokenize(Lexer* lexer);
SolanaASTNode* solana_parser_parse(Parser* parser);

SolanaCompiler* solana_compiler_create(FILE* output, bool use_anchor);
void solana_compiler_compile(SolanaCompiler* compiler, SolanaASTNode* ast);
void solana_compiler_free(SolanaCompiler* compiler);

void emit_anchor_imports(SolanaCompiler* compiler);
void emit_native_solana_imports(SolanaCompiler* compiler);
void emit_program_structure(SolanaCompiler* compiler, SolanaASTNode* program);
void emit_instruction_handler(SolanaCompiler* compiler, SolanaASTNode* instruction);
void emit_account_validation(SolanaCompiler* compiler, SolanaASTNode* accounts);
void emit_state_structure(SolanaCompiler* compiler, SolanaASTNode* state);
void emit_error_types(SolanaCompiler* compiler);

bool validate_program_structure(SolanaASTNode* ast);
bool check_account_constraints(SolanaASTNode* accounts);
bool verify_instruction_signatures(SolanaASTNode* instructions);

#endif