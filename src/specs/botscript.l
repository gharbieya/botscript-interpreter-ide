%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "botscript.tab.h"

/* yyerror is defined in wasm_runtime.c */
void yyerror(const char *s);
%}

%option noyywrap
%option yylineno

%%

[ \t\r\n]+              { /* Ignorer les espaces */ }
"//".*                  { /* Ignorer les commentaires */ }
"#".*                   { /* Ignorer les commentaires */ }

"let"                   { return LET; }
"repeat"                { return REPEAT; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"forward"               { return FORWARD; }
"turn"                  { return TURN; }
"color"                 { return COLOR; }
"penDown"               { return PENDOWN; }
"penUp"                 { return PENUP; }

[0-9]+\.[0-9]+          { yylval.reel = atof(yytext); return REEL; }
[0-9]+                  { yylval.entier = atoi(yytext); return ENTIER; }
[a-zA-Z_][a-zA-Z0-9_]*  { yylval.chaine = strdup(yytext); return IDENT; }
\"[^\"]*\"              { yylval.chaine = strdup(yytext + 1); yylval.chaine[strlen(yylval.chaine)-1] = '\0'; return CHAINE; }

"+"                     { return '+'; }
"-"                     { return '-'; }
"*"                     { return '*'; }
"/"                     { return '/'; }
"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
">="                    { return GE; }
"<"                     { return '<'; }
">"                     { return '>'; }
"="                     { return '='; }

"{"                     { return '{'; }
"}"                     { return '}'; }
"("                     { return '('; }
")"                     { return ')'; }
";"                     { return ';'; }
","                     { return ','; }

. {
    yyerror("Erreur lexicale: caractère inattendu");
    return 0;
}

%%