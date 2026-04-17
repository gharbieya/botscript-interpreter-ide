// This file is the bridge between the WASM module and UI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yyparse(void);
extern int yylineno;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);

static char g_last_error[4096];

void yyerror(const char *s) {
  snprintf(g_last_error, sizeof(g_last_error),
           "Erreur syntaxique à la ligne %d: %s", yylineno, s);
}

static char *json_escape(const char *in) {
  size_t n = 0;
  for (const char *p = in; *p; p++) {
    if (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') n++;
    n++;
  }
  char *out = (char *)malloc(n + 1);
  char *q = out;

  for (const char *p = in; *p; p++) {
    switch (*p) {
      case '"':  *q++='\\'; *q++='"'; break;
      case '\\': *q++='\\'; *q++='\\'; break;
      case '\n': *q++='\\'; *q++='n'; break;
      case '\r': *q++='\\'; *q++='r'; break;
      case '\t': *q++='\\'; *q++='t'; break;
      default: *q++=*p; break;
    }
  }
  *q = 0;
  return out;
}

// Exported function called from JS.
// Returns malloc'd JSON char*. JS must free() it.
char *compile_json(const char *source) {
  g_last_error[0] = 0;
  yylineno = 1;

  YY_BUFFER_STATE buf = yy_scan_string(source);
  int rc = yyparse();
  yy_delete_buffer(buf);

  const int ok = (rc == 0 && g_last_error[0] == 0);

  char *errEsc = json_escape(g_last_error[0] ? g_last_error : "");
  char tmp[8192];

  if (ok) {
    snprintf(tmp, sizeof(tmp),
             "{\"ok\":true,\"errors\":[]}");
  } else {
    snprintf(tmp, sizeof(tmp),
             "{\"ok\":false,\"errors\":[{\"type\":\"SYNTACTIC\",\"message\":\"%s\",\"line\":%d,\"col\":1}]}",
             errEsc, yylineno);
  }

  free(errEsc);

  char *out = (char *)malloc(strlen(tmp) + 1);
  strcpy(out, tmp);
  return out;
}