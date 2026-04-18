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

void wasm_reset_buffers(void);
void wasm_set_source(const char* src);
void wasm_record_token(WasmTokenType type, const char* value, int line, int col);
void wasm_record_error(const char* type, const char* message, int line, int col);

/* Exposed compile entry point */
const char* compile_json(const char* source);

#ifdef __cplusplus
}
#endif

#endif