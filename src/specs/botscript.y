/* IMPORTANT:
   - No main() here. WASM calls compile_json() in wasm_runtime.c
*/

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex();
extern int yylineno;
void yyerror(const char *s);
%}

%union {
    int entier;
    double reel;
    char *chaine;
}

/* Tokens */
%token <entier> ENTIER
%token <reel> REEL
%token <chaine> IDENT CHAINE
%token LET REPEAT IF ELSE WHILE FORWARD TURN COLOR PENDOWN PENUP
%token EQ NE LE GE

/* We define expression semantic value as double to avoid union conflicts */
%type <reel> expression

%left '+' '-'
%left '*' '/'
%nonassoc '<' '>' EQ NE LE GE

%%

program:
    statements
    ;

statements:
    statement
    | statements statement
    ;

statement:
    var_decl
    | assignment
    | repeat_loop
    | while_loop
    | if_stmt
    | command
    ;

var_decl:
    LET IDENT '=' expression ';'
    ;

assignment:
    IDENT '=' expression ';'
    ;

repeat_loop:
    REPEAT expression '{' statements '}'
    ;

while_loop:
    WHILE '(' expression ')' '{' statements '}'
    ;

if_stmt:
    IF '(' expression ')' '{' statements '}'
    | IF '(' expression ')' '{' statements '}' ELSE '{' statements '}'
    ;

command:
    FORWARD '(' expression ')' ';'
    | TURN '(' expression ')' ';'
    | COLOR '(' CHAINE ')' ';'
    | PENDOWN '(' ')' ';'
    | PENUP '(' ')' ';'
    ;

expression:
    ENTIER                    { $$ = (double)$1; }
    | REEL                    { $$ = $1; }
    | IDENT                   { $$ = 0.0; /* TODO: semantic lookup */ }
    | expression '+' expression { $$ = $1 + $3; }
    | expression '-' expression { $$ = $1 - $3; }
    | expression '*' expression { $$ = $1 * $3; }
    | expression '/' expression { $$ = $1 / $3; }
    | '(' expression ')'      { $$ = $2; }
    | expression '<' expression { $$ = ($1 < $3) ? 1.0 : 0.0; }
    | expression '>' expression { $$ = ($1 > $3) ? 1.0 : 0.0; }
    | expression EQ expression  { $$ = ($1 == $3) ? 1.0 : 0.0; }
    | expression NE expression  { $$ = ($1 != $3) ? 1.0 : 0.0; }
    | expression LE expression  { $$ = ($1 <= $3) ? 1.0 : 0.0; }
    | expression GE expression  { $$ = ($1 >= $3) ? 1.0 : 0.0; }
    ;

%%