#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "wasm_bridge.h"
#include "botscript.tab.h"

/* Flex/Bison symbols */
extern int yyparse(void);
extern int yylex(void);
extern int yylineno;
extern FILE* yyin;
typedef void* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char* str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern void yyrestart(FILE* input_file);

/* -------------------- dynamic string builder -------------------- */
typedef struct {
  char* data;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf* sb) {
  sb->cap = 1024;
  sb->len = 0;
  sb->data = (char*)malloc(sb->cap);
  if (sb->data) sb->data[0] = '\0';
}

static void sb_free(StrBuf* sb) {
  free(sb->data);
  sb->data = NULL;
  sb->len = sb->cap = 0;
}

static void sb_ensure(StrBuf* sb, size_t extra) {
  if (sb->len + extra + 1 <= sb->cap) return;
  size_t ncap = sb->cap;
  while (sb->len + extra + 1 > ncap) ncap *= 2;
  char* nd = (char*)realloc(sb->data, ncap);
  if (!nd) return;
  sb->data = nd;
  sb->cap = ncap;
}

static void sb_append(StrBuf* sb, const char* s) {
  size_t n = strlen(s);
  sb_ensure(sb, n);
  memcpy(sb->data + sb->len, s, n);
  sb->len += n;
  sb->data[sb->len] = '\0';
}

static void sb_appendf(StrBuf* sb, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char tmp[2048];
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if ((size_t)n < sizeof(tmp)) {
    sb_append(sb, tmp);
    return;
  }
  char* big = (char*)malloc((size_t)n + 1);
  if (!big) return;
  va_start(ap, fmt);
  vsnprintf(big, (size_t)n + 1, fmt, ap);
  va_end(ap);
  sb_append(sb, big);
  free(big);
}

static void sb_append_json_string(StrBuf* sb, const char* s) {
  sb_append(sb, "\"");
  for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
    switch (*p) {
      case '\"': sb_append(sb, "\\\""); break;
      case '\\': sb_append(sb, "\\\\"); break;
      case '\n': sb_append(sb, "\\n"); break;
      case '\r': sb_append(sb, "\\r"); break;
      case '\t': sb_append(sb, "\\t"); break;
      default:
        if (*p < 0x20) sb_appendf(sb, "\\u%04x", *p);
        else {
          char c[2] = {(char)*p, '\0'};
          sb_append(sb, c);
        }
    }
  }
  sb_append(sb, "\"");
}

/* -------------------- token/error storage -------------------- */
typedef struct {
  WasmTokenType type;
  char* value;
  int line;
  int col;
} WasmToken;

typedef struct {
  char* type;    /* LEXICAL/SYNTACTIC/SEMANTIC */
  char* message;
  int line;
  int col;
} WasmError;

static WasmToken* g_tokens = NULL;
static size_t g_tokens_len = 0;
static size_t g_tokens_cap = 0;

static WasmError* g_errors = NULL;
static size_t g_errors_len = 0;
static size_t g_errors_cap = 0;

static const char* g_source = NULL;

static char* xstrdup(const char* s) {
  if (!s) return xstrdup("");
  size_t n = strlen(s);
  char* d = (char*)malloc(n + 1);
  if (!d) return NULL;
  memcpy(d, s, n + 1);
  return d;
}

static const char* tok_name(WasmTokenType t) {
  switch (t) {
    case TOK_ENTIER: return "ENTIER";
    case TOK_REEL: return "REEL";
    case TOK_IDENT: return "IDENT";
    case TOK_MOTCLE: return "MOTCLE";
    case TOK_OP_ARTHM: return "OP_ARTHM";
    case TOK_OP_REL: return "OP_REL";
    case TOK_AFFECT: return "AFFECT";
    case TOK_PUNCT: return "PUNCT";
    case TOK_CHAINE: return "CHAINE";
    case TOK_FIN: return "FIN";
    case TOK_ERROR: return "ERROR";
    default: return "ERROR";
  }
}

void wasm_set_source(const char* src) {
  g_source = src;
}

void wasm_reset_buffers(void) {
  for (size_t i = 0; i < g_tokens_len; ++i) free(g_tokens[i].value);
  free(g_tokens);
  g_tokens = NULL;
  g_tokens_len = 0;
  g_tokens_cap = 0;

  for (size_t i = 0; i < g_errors_len; ++i) {
    free(g_errors[i].type);
    free(g_errors[i].message);
  }
  free(g_errors);
  g_errors = NULL;
  g_errors_len = 0;
  g_errors_cap = 0;
}

void wasm_record_token(WasmTokenType type, const char* value, int line, int col) {
  if (g_tokens_len == g_tokens_cap) {
    size_t ncap = g_tokens_cap ? g_tokens_cap * 2 : 64;
    WasmToken* nt = (WasmToken*)realloc(g_tokens, ncap * sizeof(WasmToken));
    if (!nt) return;
    g_tokens = nt;
    g_tokens_cap = ncap;
  }
  g_tokens[g_tokens_len].type = type;
  g_tokens[g_tokens_len].value = xstrdup(value ? value : "");
  g_tokens[g_tokens_len].line = line;
  g_tokens[g_tokens_len].col = col;
  g_tokens_len++;
}

void wasm_record_error(const char* type, const char* message, int line, int col) {
  if (g_errors_len == g_errors_cap) {
    size_t ncap = g_errors_cap ? g_errors_cap * 2 : 16;
    WasmError* ne = (WasmError*)realloc(g_errors, ncap * sizeof(WasmError));
    if (!ne) return;
    g_errors = ne;
    g_errors_cap = ncap;
  }
  g_errors[g_errors_len].type = xstrdup(type ? type : "SYNTACTIC");
  g_errors[g_errors_len].message = xstrdup(message ? message : "Erreur");
  g_errors[g_errors_len].line = line;
  g_errors[g_errors_len].col = col;
  g_errors_len++;
}

/* Bison error hook */
void yyerror(const char* s) {
  int line = yylineno > 0 ? yylineno : 1;
  wasm_record_error("SYNTACTIC", s ? s : "Syntax error", line, 1);
}

static char* build_json(int ok) {
  StrBuf sb;
  sb_init(&sb);

  sb_append(&sb, "{");
  sb_appendf(&sb, "\"ok\":%s,", ok ? "true" : "false");

  /* errors */
  sb_append(&sb, "\"errors\":[");
  for (size_t i = 0; i < g_errors_len; ++i) {
    if (i) sb_append(&sb, ",");
    sb_append(&sb, "{");
    sb_append(&sb, "\"type\":");
    sb_append_json_string(&sb, g_errors[i].type);
    sb_append(&sb, ",\"message\":");
    sb_append_json_string(&sb, g_errors[i].message);
    sb_appendf(&sb, ",\"line\":%d,\"col\":%d", g_errors[i].line, g_errors[i].col);
    sb_append(&sb, "}");
  }
  sb_append(&sb, "],");

  /* tokens */
  sb_append(&sb, "\"tokens\":[");
  for (size_t i = 0; i < g_tokens_len; ++i) {
    if (i) sb_append(&sb, ",");
    sb_append(&sb, "{");
    sb_append(&sb, "\"type\":");
    sb_append_json_string(&sb, tok_name(g_tokens[i].type));
    sb_append(&sb, ",\"value\":");
    sb_append_json_string(&sb, g_tokens[i].value ? g_tokens[i].value : "");
    sb_appendf(&sb, ",\"line\":%d,\"col\":%d", g_tokens[i].line, g_tokens[i].col);
    sb_append(&sb, "}");
  }
  sb_append(&sb, "],");

  sb_append(&sb, "\"ast\":null,");
  sb_append(&sb, "\"trace\":[]");
  sb_append(&sb, "}");

  return sb.data; /* caller owns */
}

const char* compile_json(const char* source) {
  wasm_reset_buffers();
  wasm_set_source(source ? source : "");

  /* Flex parse from memory */
  YY_BUFFER_STATE buf = yy_scan_string(source ? source : "");
  yylineno = 1;

  int parse_rc = yyparse();

  if (buf) yy_delete_buffer(buf);
  yyrestart(NULL);

  /* mark end token for UI parity */
  wasm_record_token(TOK_FIN, "", yylineno > 0 ? yylineno : 1, 1);

  int ok = (parse_rc == 0 && g_errors_len == 0);
  char* out = build_json(ok);
  if (!out) {
    const char* fallback = "{\"ok\":false,\"errors\":[{\"type\":\"SEMANTIC\",\"message\":\"OOM\",\"line\":1,\"col\":1}],\"tokens\":[],\"ast\":null,\"trace\":[]}";
    char* f = (char*)malloc(strlen(fallback) + 1);
    if (!f) return NULL;
    strcpy(f, fallback);
    return f;
  }
  return out;
}