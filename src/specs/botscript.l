%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "botscript.tab.h"
#include "wasm_bridge.h"

/* yylineno is enabled by %option yylineno */
extern int yylineno;

/* local column tracking for lexer */
static int yycol = 1;

static void bump_col(const char* txt) {
  yycol += (int)strlen(txt);
}

static void tok_keyword(const char* kw, int token) {
  wasm_record_token(TOK_MOTCLE, kw, yylineno, yycol);
  bump_col(yytext);
  yylval.chaine = strdup(yytext);
  /* yylval used only when parser needs it */
}

static void tok_simple(WasmTokenType t) {
  wasm_record_token(t, yytext, yylineno, yycol);
  bump_col(yytext);
}
%}

%option noyywrap
%option yylineno

%%

[ \t\r]+                { bump_col(yytext); }
\n                      { yylineno++; yycol = 1; }

"//".*                  { bump_col(yytext); }
"#".*                   { bump_col(yytext); }

"let"                   { tok_keyword("let", LET); return LET; }
"repeat"                { tok_keyword("repeat", REPEAT); return REPEAT; }
"if"                    { tok_keyword("if", IF); return IF; }
"else"                  { tok_keyword("else", ELSE); return ELSE; }
"while"                 { tok_keyword("while", WHILE); return WHILE; }
"forward"               { tok_keyword("forward", FORWARD); return FORWARD; }
"turn"                  { tok_keyword("turn", TURN); return TURN; }
"color"                 { tok_keyword("color", COLOR); return COLOR; }
"penDown"               { tok_keyword("penDown", PENDOWN); return PENDOWN; }
"penUp"                 { tok_keyword("penUp", PENUP); return PENUP; }

[0-9]+\.[0-9]+          {
                          yylval.reel = atof(yytext);
                          wasm_record_token(TOK_REEL, yytext, yylineno, yycol);
                          bump_col(yytext);
                          return REEL;
                        }

[0-9]+                  {
                          yylval.entier = atoi(yytext);
                          wasm_record_token(TOK_ENTIER, yytext, yylineno, yycol);
                          bump_col(yytext);
                          return ENTIER;
                        }

[a-zA-Z_][a-zA-Z0-9_]*  {
                          yylval.chaine = strdup(yytext);
                          wasm_record_token(TOK_IDENT, yytext, yylineno, yycol);
                          bump_col(yytext);
                          return IDENT;
                        }

\"[^\"]*\"              {
                          char* s = strdup(yytext + 1);
                          s[strlen(s)-1] = '\0';
                          yylval.chaine = strdup(s);
                          wasm_record_token(TOK_CHAINE, s, yylineno, yycol);
                          free(s);
                          bump_col(yytext);
                          return CHAINE;
                        }

"=="                    { tok_simple(TOK_OP_REL); return EQ; }
"!="                    { tok_simple(TOK_OP_REL); return NE; }
"<="                    { tok_simple(TOK_OP_REL); return LE; }
">="                    { tok_simple(TOK_OP_REL); return GE; }

"<"                     { tok_simple(TOK_OP_REL); return '<'; }
">"                     { tok_simple(TOK_OP_REL); return '>'; }
"="                     { tok_simple(TOK_AFFECT); return '='; }

"+"                     { tok_simple(TOK_OP_ARTHM); return '+'; }
"-"                     { tok_simple(TOK_OP_ARTHM); return '-'; }
"*"                     { tok_simple(TOK_OP_ARTHM); return '*'; }
"/"                     { tok_simple(TOK_OP_ARTHM); return '/'; }

"{"                     { tok_simple(TOK_PUNCT); return '{'; }
"}"                     { tok_simple(TOK_PUNCT); return '}'; }
"("                     { tok_simple(TOK_PUNCT); return '('; }
")"                     { tok_simple(TOK_PUNCT); return ')'; }
";"                     { tok_simple(TOK_PUNCT); return ';'; }
","                     { tok_simple(TOK_PUNCT); return ','; }

.                       {
                          char msg[128];
                          snprintf(msg, sizeof(msg), "Caractère inattendu: '%s'", yytext);
                          wasm_record_error("LEXICAL", msg, yylineno, yycol);
                          wasm_record_token(TOK_ERROR, yytext, yylineno, yycol);
                          bump_col(yytext);
                          return 0;
                        }

%%