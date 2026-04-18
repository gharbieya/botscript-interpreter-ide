#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TOK_ENTIER,
  TOK_REEL,
  TOK_IDENT,
  TOK_MOTCLE,
  TOK_OP_ARTHM,
  TOK_OP_REL,
  TOK_AFFECT,
  TOK_PUNCT,
  TOK_CHAINE,
  TOK_FIN,
  TOK_ERROR
} WasmTokenType;

/* --- AST --- */
typedef enum {
  AST_PROGRAM,
  AST_VAR_DECL,
  AST_ASSIGNMENT,
  AST_REPEAT,
  AST_IF,
  AST_WHILE,
  AST_COMMAND,
  AST_BINARY_EXPR,
  AST_LITERAL,
  AST_IDENTIFIER
} AstType;

typedef struct AstNode AstNode;

struct AstNode {
  AstType type;

  /* generic location */
  int line;
  int col;

  union {
    struct { AstNode** body; int body_len; } program;

    struct { char* name; AstNode* value; } var_decl;
    struct { char* name; AstNode* value; } assignment;

    struct { AstNode* count; AstNode** body; int body_len; } repeat_stmt;

    struct {
      AstNode* condition;
      AstNode** then_body; int then_len;
      AstNode** else_body; int else_len; /* can be 0 */
    } if_stmt;

    struct { AstNode* condition; AstNode** body; int body_len; } while_stmt;

    struct { char* name; AstNode** args; int args_len; } command;

    struct { AstNode* left; char* op; AstNode* right; } binary;

    struct { char* literal_type; char* value; } literal;

    struct { char* name; } ident;
  } as;
};

/* token/error hooks */
void wasm_reset_buffers(void);
void wasm_set_source(const char* src);
void wasm_record_token(WasmTokenType type, const char* value, int line, int col);
void wasm_record_error(const char* type, const char* message, int line, int col);

/* parser<->runtime shared AST root */
void wasm_set_ast_root(AstNode* root);

/* ast builders */
AstNode* ast_make_program(AstNode** body, int body_len);
AstNode* ast_make_var_decl(const char* name, AstNode* value);
AstNode* ast_make_assignment(const char* name, AstNode* value);
AstNode* ast_make_repeat(AstNode* count, AstNode** body, int body_len);
AstNode* ast_make_if(AstNode* cond, AstNode** then_body, int then_len, AstNode** else_body, int else_len);
AstNode* ast_make_while(AstNode* cond, AstNode** body, int body_len);
AstNode* ast_make_command(const char* name, AstNode** args, int args_len);
AstNode* ast_make_binary(AstNode* left, const char* op, AstNode* right);
AstNode* ast_make_literal_number(double v);
AstNode* ast_make_literal_string(const char* s);
AstNode* ast_make_identifier(const char* name);

/* list helpers for bison */
AstNode** ast_list_new(AstNode* first, int* out_len);
AstNode** ast_list_push(AstNode** list, int* len, AstNode* item);

/* compile entry */
const char* compile_json(const char* source);

#ifdef __cplusplus
}
#endif

#endif