#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "wasm_bridge.h"
#include "botscript.tab.h"

/* flex/bison */
extern int yyparse(void);
extern int yylineno;
typedef void* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char* str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern void yyrestart(FILE* input_file);

/* ---------------- string builder ---------------- */
typedef struct {
  char* data;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf* sb) {
  sb->cap = 2048;
  sb->len = 0;
  sb->data = (char*)malloc(sb->cap);
  if (sb->data) sb->data[0] = '\0';
}
static void sb_ensure(StrBuf* sb, size_t extra) {
  if (sb->len + extra + 1 <= sb->cap) return;
  size_t nc = sb->cap;
  while (sb->len + extra + 1 > nc) nc *= 2;
  char* nd = (char*)realloc(sb->data, nc);
  if (!nd) return;
  sb->data = nd; sb->cap = nc;
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
  char tmp[4096];
  int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if ((size_t)n < sizeof(tmp)) { sb_append(sb, tmp); return; }
  char* big = (char*)malloc((size_t)n + 1);
  if (!big) return;
  va_start(ap, fmt);
  vsnprintf(big, (size_t)n + 1, fmt, ap);
  va_end(ap);
  sb_append(sb, big);
  free(big);
}
static void sb_json_str(StrBuf* sb, const char* s) {
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
        else { char c[2]={(char)*p,0}; sb_append(sb,c); }
    }
  }
  sb_append(sb, "\"");
}

/* ---------------- token/error buffers ---------------- */
typedef struct {
  WasmTokenType type;
  char* value;
  int line;
  int col;
} WasmToken;
typedef struct {
  char* type;
  char* message;
  int line;
  int col;
} WasmError;

static WasmToken* g_tokens = NULL; static int g_tlen = 0; static int g_tcap = 0;
static WasmError* g_errors = NULL; static int g_elen = 0; static int g_ecap = 0;
static AstNode* g_root = NULL;

static char* xstrdup(const char* s){ if(!s)s=""; size_t n=strlen(s); char* d=(char*)malloc(n+1); if(!d) return NULL; memcpy(d,s,n+1); return d; }

static const char* tok_name(WasmTokenType t){
  switch(t){
    case TOK_ENTIER:return "ENTIER";
    case TOK_REEL:return "REEL";
    case TOK_IDENT:return "IDENT";
    case TOK_MOTCLE:return "MOTCLE";
    case TOK_OP_ARTHM:return "OP_ARTHM";
    case TOK_OP_REL:return "OP_REL";
    case TOK_AFFECT:return "AFFECT";
    case TOK_PUNCT:return "PUNCT";
    case TOK_CHAINE:return "CHAINE";
    case TOK_FIN:return "FIN";
    case TOK_ERROR:return "ERROR";
    default:return "ERROR";
  }
}

void wasm_set_source(const char* src){ (void)src; }

void wasm_reset_buffers(void){
  for(int i=0;i<g_tlen;i++) free(g_tokens[i].value);
  free(g_tokens); g_tokens=NULL; g_tlen=0; g_tcap=0;

  for(int i=0;i<g_elen;i++){ free(g_errors[i].type); free(g_errors[i].message); }
  free(g_errors); g_errors=NULL; g_elen=0; g_ecap=0;

  g_root = NULL;
}

void wasm_record_token(WasmTokenType type, const char* value, int line, int col){
  if(g_tlen==g_tcap){ int nc=g_tcap?g_tcap*2:64; WasmToken* nt=(WasmToken*)realloc(g_tokens,nc*sizeof(WasmToken)); if(!nt)return; g_tokens=nt; g_tcap=nc; }
  g_tokens[g_tlen].type=type;
  g_tokens[g_tlen].value=xstrdup(value);
  g_tokens[g_tlen].line=line;
  g_tokens[g_tlen].col=col;
  g_tlen++;
}
void wasm_record_error(const char* type, const char* message, int line, int col){
  if(g_elen==g_ecap){ int nc=g_ecap?g_ecap*2:16; WasmError* ne=(WasmError*)realloc(g_errors,nc*sizeof(WasmError)); if(!ne)return; g_errors=ne; g_ecap=nc; }
  g_errors[g_elen].type=xstrdup(type?type:"SYNTACTIC");
  g_errors[g_elen].message=xstrdup(message?message:"Erreur");
  g_errors[g_elen].line=line;
  g_errors[g_elen].col=col;
  g_elen++;
}
void yyerror(const char *s){
  wasm_record_error("SYNTACTIC", s?s:"Syntax error", yylineno>0?yylineno:1, 1);
}

/* ---------------- AST builders ---------------- */
static AstNode* ast_new(AstType t){
  AstNode* n=(AstNode*)calloc(1,sizeof(AstNode));
  if(!n) return NULL;
  n->type=t; n->line = yylineno>0?yylineno:1; n->col=1;
  return n;
}
void wasm_set_ast_root(AstNode* root){ g_root = root; }

AstNode** ast_list_new(AstNode* first, int* out_len){
  AstNode** arr=(AstNode**)malloc(sizeof(AstNode*));
  if(!arr){ *out_len=0; return NULL; }
  arr[0]=first; *out_len=1; return arr;
}
AstNode** ast_list_push(AstNode** list, int* len, AstNode* item){
  int n=*len+1;
  AstNode** nl=(AstNode**)realloc(list,(size_t)n*sizeof(AstNode*));
  if(!nl) return list;
  nl[n-1]=item; *len=n; return nl;
}

AstNode* ast_make_program(AstNode** body, int body_len){ AstNode* n=ast_new(AST_PROGRAM); n->as.program.body=body; n->as.program.body_len=body_len; return n; }
AstNode* ast_make_var_decl(const char* name, AstNode* value){ AstNode* n=ast_new(AST_VAR_DECL); n->as.var_decl.name=xstrdup(name); n->as.var_decl.value=value; return n; }
AstNode* ast_make_assignment(const char* name, AstNode* value){ AstNode* n=ast_new(AST_ASSIGNMENT); n->as.assignment.name=xstrdup(name); n->as.assignment.value=value; return n; }
AstNode* ast_make_repeat(AstNode* count, AstNode** body, int body_len){ AstNode* n=ast_new(AST_REPEAT); n->as.repeat_stmt.count=count; n->as.repeat_stmt.body=body; n->as.repeat_stmt.body_len=body_len; return n; }
AstNode* ast_make_if(AstNode* cond, AstNode** then_body, int then_len, AstNode** else_body, int else_len){ AstNode* n=ast_new(AST_IF); n->as.if_stmt.condition=cond; n->as.if_stmt.then_body=then_body; n->as.if_stmt.then_len=then_len; n->as.if_stmt.else_body=else_body; n->as.if_stmt.else_len=else_len; return n; }
AstNode* ast_make_while(AstNode* cond, AstNode** body, int body_len){ AstNode* n=ast_new(AST_WHILE); n->as.while_stmt.condition=cond; n->as.while_stmt.body=body; n->as.while_stmt.body_len=body_len; return n; }
AstNode* ast_make_command(const char* name, AstNode** args, int args_len){ AstNode* n=ast_new(AST_COMMAND); n->as.command.name=xstrdup(name); n->as.command.args=args; n->as.command.args_len=args_len; return n; }
AstNode* ast_make_binary(AstNode* left, const char* op, AstNode* right){ AstNode* n=ast_new(AST_BINARY_EXPR); n->as.binary.left=left; n->as.binary.op=xstrdup(op); n->as.binary.right=right; return n; }
AstNode* ast_make_literal_number(double v){ AstNode* n=ast_new(AST_LITERAL); char buf[64]; snprintf(buf,sizeof(buf),"%.15g",v); n->as.literal.literal_type=xstrdup("number"); n->as.literal.value=xstrdup(buf); return n; }
AstNode* ast_make_literal_string(const char* s){ AstNode* n=ast_new(AST_LITERAL); n->as.literal.literal_type=xstrdup("string"); n->as.literal.value=xstrdup(s?s:""); return n; }
AstNode* ast_make_identifier(const char* name){ AstNode* n=ast_new(AST_IDENTIFIER); n->as.ident.name=xstrdup(name); return n; }

/* ---------------- AST -> JSON ---------------- */
static void ast_to_json(StrBuf* sb, AstNode* n){
  if(!n){ sb_append(sb,"null"); return; }

  switch(n->type){
    case AST_PROGRAM:
      sb_append(sb,"{\"type\":\"Program\",\"body\":[");
      for(int i=0;i<n->as.program.body_len;i++){ if(i) sb_append(sb,","); ast_to_json(sb,n->as.program.body[i]); }
      sb_append(sb,"]}");
      break;

    case AST_VAR_DECL:
      sb_append(sb,"{\"type\":\"VarDecl\",\"name\":");
      sb_json_str(sb,n->as.var_decl.name?n->as.var_decl.name:"");
      sb_append(sb,",\"value\":");
      ast_to_json(sb,n->as.var_decl.value);
      sb_append(sb,"}");
      break;

    case AST_ASSIGNMENT:
      sb_append(sb,"{\"type\":\"Assignment\",\"name\":");
      sb_json_str(sb,n->as.assignment.name?n->as.assignment.name:"");
      sb_append(sb,",\"value\":");
      ast_to_json(sb,n->as.assignment.value);
      sb_append(sb,"}");
      break;

    case AST_REPEAT:
      sb_append(sb,"{\"type\":\"Repeat\",\"count\":");
      ast_to_json(sb,n->as.repeat_stmt.count);
      sb_append(sb,",\"body\":[");
      for(int i=0;i<n->as.repeat_stmt.body_len;i++){ if(i) sb_append(sb,","); ast_to_json(sb,n->as.repeat_stmt.body[i]); }
      sb_append(sb,"]}");
      break;

    case AST_IF:
      sb_append(sb,"{\"type\":\"If\",\"condition\":");
      ast_to_json(sb,n->as.if_stmt.condition);
      sb_append(sb,",\"thenBody\":[");
      for(int i=0;i<n->as.if_stmt.then_len;i++){ if(i) sb_append(sb,","); ast_to_json(sb,n->as.if_stmt.then_body[i]); }
      sb_append(sb,"],\"elseBody\":");
      if(n->as.if_stmt.else_len>0){
        sb_append(sb,"[");
        for(int i=0;i<n->as.if_stmt.else_len;i++){ if(i) sb_append(sb,","); ast_to_json(sb,n->as.if_stmt.else_body[i]); }
        sb_append(sb,"]");
      } else {
        sb_append(sb,"null");
      }
      sb_append(sb,"}");
      break;

    case AST_WHILE:
      sb_append(sb,"{\"type\":\"While\",\"condition\":");
      ast_to_json(sb,n->as.while_stmt.condition);
      sb_append(sb,",\"body\":[");
      for(int i=0;i<n->as.while_stmt.body_len;i++){ if(i) sb_append(sb,","); ast_to_json(sb,n->as.while_stmt.body[i]); }
      sb_append(sb,"]}");
      break;

    case AST_COMMAND:
      sb_append(sb,"{\"type\":\"Command\",\"name\":");
      sb_json_str(sb,n->as.command.name?n->as.command.name:"");
      sb_append(sb,",\"args\":[");
      for(int i=0;i<n->as.command.args_len;i++){ if(i) sb_append(sb,","); ast_to_json(sb,n->as.command.args[i]); }
      sb_append(sb,"]}");
      break;

    case AST_BINARY_EXPR:
      sb_append(sb,"{\"type\":\"BinaryExpr\",\"left\":");
      ast_to_json(sb,n->as.binary.left);
      sb_append(sb,",\"operator\":");
      sb_json_str(sb,n->as.binary.op?n->as.binary.op:"");
      sb_append(sb,",\"right\":");
      ast_to_json(sb,n->as.binary.right);
      sb_append(sb,"}");
      break;

    case AST_LITERAL:
      sb_append(sb,"{\"type\":\"Literal\",\"value\":");
      if(n->as.literal.literal_type && strcmp(n->as.literal.literal_type,"number")==0){
        sb_append(sb,n->as.literal.value?n->as.literal.value:"0");
      } else {
        sb_json_str(sb,n->as.literal.value?n->as.literal.value:"");
      }
      sb_append(sb,"}");
      break;

    case AST_IDENTIFIER:
      sb_append(sb,"{\"type\":\"Identifier\",\"name\":");
      sb_json_str(sb,n->as.ident.name?n->as.ident.name:"");
      sb_append(sb,"}");
      break;
  }
}

/* ---------------- trace (simple AST-walk execution) ---------------- */
typedef struct { double x,y,angle; int penDown; char color[32]; } ExecState;
static void trace_push_state(StrBuf* tr, int* first, ExecState* s){
  if(!*first) sb_append(tr,","); *first=0;
  sb_appendf(tr,"{\"x\":%.6f,\"y\":%.6f,\"angle\":%.6f,\"penDown\":%s,\"color\":",s->x,s->y,s->angle,s->penDown?"true":"false");
  sb_json_str(tr,s->color);
  sb_append(tr,"}");
}
static double eval_expr(AstNode* n); /* fwd */

typedef struct { char* k; double v; } Var;
static Var* g_vars=NULL; static int g_vlen=0, g_vcap=0;
static void var_set(const char* k,double v){
  for(int i=0;i<g_vlen;i++) if(strcmp(g_vars[i].k,k)==0){ g_vars[i].v=v; return; }
  if(g_vlen==g_vcap){ int nc=g_vcap?g_vcap*2:16; Var* nv=(Var*)realloc(g_vars,nc*sizeof(Var)); if(!nv) return; g_vars=nv; g_vcap=nc; }
  g_vars[g_vlen].k=xstrdup(k); g_vars[g_vlen].v=v; g_vlen++;
}
static int var_get(const char* k,double* out){
  for(int i=0;i<g_vlen;i++) if(strcmp(g_vars[i].k,k)==0){ *out=g_vars[i].v; return 1; }
  return 0;
}
static double eval_expr(AstNode* n){
  if(!n) return 0.0;
  if(n->type==AST_LITERAL){
    if(n->as.literal.literal_type && strcmp(n->as.literal.literal_type,"number")==0) return atof(n->as.literal.value?n->as.literal.value:"0");
    return 0.0;
  }
  if(n->type==AST_IDENTIFIER){ double v=0; if(var_get(n->as.ident.name,&v)) return v; return 0.0; }
  if(n->type==AST_BINARY_EXPR){
    double l=eval_expr(n->as.binary.left), r=eval_expr(n->as.binary.right);
    const char* op=n->as.binary.op?n->as.binary.op:"";
    if(strcmp(op,"+")==0) return l+r;
    if(strcmp(op,"-")==0) return l-r;
    if(strcmp(op,"*")==0) return l*r;
    if(strcmp(op,"/")==0) return r!=0?r?l/r:0:0;
    if(strcmp(op,"<")==0) return l<r;
    if(strcmp(op,">")==0) return l>r;
    if(strcmp(op,"==")==0) return l==r;
    if(strcmp(op,"!=")==0) return l!=r;
    if(strcmp(op,"<=")==0) return l<=r;
    if(strcmp(op,">=")==0) return l>=r;
  }
  return 0.0;
}
static void exec_stmt(AstNode* n, ExecState* s, StrBuf* tr, int* first){
  if(!n) return;
  switch(n->type){
    case AST_VAR_DECL: var_set(n->as.var_decl.name, eval_expr(n->as.var_decl.value)); break;
    case AST_ASSIGNMENT: var_set(n->as.assignment.name, eval_expr(n->as.assignment.value)); break;
    case AST_REPEAT: {
      int c=(int)eval_expr(n->as.repeat_stmt.count);
      for(int i=0;i<c;i++) for(int j=0;j<n->as.repeat_stmt.body_len;j++) exec_stmt(n->as.repeat_stmt.body[j],s,tr,first);
      break;
    }
    case AST_IF: {
      if(eval_expr(n->as.if_stmt.condition)){
        for(int i=0;i<n->as.if_stmt.then_len;i++) exec_stmt(n->as.if_stmt.then_body[i],s,tr,first);
      } else {
        for(int i=0;i<n->as.if_stmt.else_len;i++) exec_stmt(n->as.if_stmt.else_body[i],s,tr,first);
      }
      break;
    }
    case AST_WHILE: {
      int guard=0;
      while(eval_expr(n->as.while_stmt.condition) && guard++<10000){
        for(int i=0;i<n->as.while_stmt.body_len;i++) exec_stmt(n->as.while_stmt.body[i],s,tr,first);
      }
      break;
    }
    case AST_COMMAND: {
      const char* nm=n->as.command.name?n->as.command.name:"";
      if(strcmp(nm,"forward")==0){
        double d = n->as.command.args_len>0 ? eval_expr(n->as.command.args[0]) : 0.0;
        double rad = s->angle * 3.14159265358979323846 / 180.0;
        s->x += cos(rad)*d;
        s->y += sin(rad)*d;
        trace_push_state(tr,first,s);
      } else if(strcmp(nm,"turn")==0){
        double a = n->as.command.args_len>0 ? eval_expr(n->as.command.args[0]) : 0.0;
        s->angle += a;
      } else if(strcmp(nm,"color")==0){
        if(n->as.command.args_len>0 && n->as.command.args[0] && n->as.command.args[0]->type==AST_LITERAL){
          const char* c=n->as.command.args[0]->as.literal.value;
          if(c){ strncpy(s->color,c,sizeof(s->color)-1); s->color[sizeof(s->color)-1]='\0'; }
        }
      } else if(strcmp(nm,"penDown")==0){
        s->penDown = 1;
      } else if(strcmp(nm,"penUp")==0){
        s->penDown = 0;
      }
      break;
    }
    default: break;
  }
}
static void build_trace_json(StrBuf* tr, AstNode* root){
  ExecState s = {250.0,250.0,0.0,1,"#3b82f6"};
  int first=1;
  sb_append(tr,"[");
  trace_push_state(tr,&first,&s);
  if(root && root->type==AST_PROGRAM){
    for(int i=0;i<root->as.program.body_len;i++) exec_stmt(root->as.program.body[i],&s,tr,&first);
  }
  sb_append(tr,"]");
}

/* ---------------- final JSON ---------------- */
const char* compile_json(const char* source){
  wasm_reset_buffers();
  wasm_set_source(source?source:"");

  YY_BUFFER_STATE buf = yy_scan_string(source?source:"");
  yylineno = 1;
  int rc = yyparse();
  if(buf) yy_delete_buffer(buf);
  yyrestart(NULL);

  wasm_record_token(TOK_FIN, "", yylineno>0?yylineno:1, 1);

  int ok = (rc==0 && g_elen==0 && g_root!=NULL);

  StrBuf out; sb_init(&out);
  sb_append(&out,"{");
  sb_appendf(&out,"\"ok\":%s,", ok?"true":"false");

  sb_append(&out,"\"errors\":[");
  for(int i=0;i<g_elen;i++){
    if(i) sb_append(&out,",");
    sb_append(&out,"{\"type\":"); sb_json_str(&out,g_errors[i].type?g_errors[i].type:"SYNTACTIC");
    sb_append(&out,",\"message\":"); sb_json_str(&out,g_errors[i].message?g_errors[i].message:"Erreur");
    sb_appendf(&out,",\"line\":%d,\"col\":%d}",g_errors[i].line,g_errors[i].col);
  }
  sb_append(&out,"],");

  sb_append(&out,"\"tokens\":[");
  for(int i=0;i<g_tlen;i++){
    if(i) sb_append(&out,",");
    sb_append(&out,"{\"type\":"); sb_json_str(&out,tok_name(g_tokens[i].type));
    sb_append(&out,",\"value\":"); sb_json_str(&out,g_tokens[i].value?g_tokens[i].value:"");
    sb_appendf(&out,",\"line\":%d,\"col\":%d}",g_tokens[i].line,g_tokens[i].col);
  }
  sb_append(&out,"],");

  sb_append(&out,"\"ast\":");
  ast_to_json(&out, g_root);
  sb_append(&out,",");

  StrBuf tr; sb_init(&tr);
  build_trace_json(&tr, g_root);
  sb_append(&out,"\"trace\":");
  sb_append(&out,tr.data?tr.data:"[]");

  sb_append(&out,"}");

  free(tr.data);
  return out.data; /* caller (JS) frees */
}