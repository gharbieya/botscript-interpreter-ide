%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wasm_bridge.h"

extern int yylex();
extern int yylineno;
void yyerror(const char *s);
%}

%define parse.error verbose

%code requires {
  typedef struct AstNode AstNode;
}

%union {
    int entier;
    double reel;
    char *chaine;

    AstNode* node;
    struct {
      AstNode** items;
      int len;
    } nodelist;
}

/* Tokens */
%token <entier> ENTIER
%token <reel> REEL
%token <chaine> IDENT CHAINE
%token LET REPEAT IF ELSE WHILE FORWARD TURN COLOR PENDOWN PENUP
%token EQ NE LE GE

/* AST non-terminals */
%type <node> program statement var_decl assignment repeat_loop while_loop if_stmt command expression
%type <nodelist> statements stmt_block arg_list_opt arg_list

%left '+' '-'
%left '*' '/'
%nonassoc '<' '>' EQ NE LE GE

%%

program:
    statements
    {
      $$ = ast_make_program($1.items, $1.len);
      wasm_set_ast_root($$);
    }
    ;

statements:
    statement
    {
      $$.items = ast_list_new($1, &$$.len);
    }
    | statements statement
    {
      $$.items = ast_list_push($1.items, &$1.len, $2);
      $$.len = $1.len;
    }
    ;

stmt_block:
    statements { $$ = $1; }
    | /* empty */
    {
      $$.items = NULL;
      $$.len = 0;
    }
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
    {
      $$ = ast_make_var_decl($2, $4);
      free($2);
    }
    ;

assignment:
    IDENT '=' expression ';'
    {
      $$ = ast_make_assignment($1, $3);
      free($1);
    }
    ;

repeat_loop:
    REPEAT expression '{' stmt_block '}'
    {
      $$ = ast_make_repeat($2, $4.items, $4.len);
    }
    ;

while_loop:
    WHILE '(' expression ')' '{' stmt_block '}'
    {
      $$ = ast_make_while($3, $6.items, $6.len);
    }
    ;

if_stmt:
    IF '(' expression ')' '{' stmt_block '}'
    {
      $$ = ast_make_if($3, $6.items, $6.len, NULL, 0);
    }
    | IF '(' expression ')' '{' stmt_block '}' ELSE '{' stmt_block '}'
    {
      $$ = ast_make_if($3, $6.items, $6.len, $10.items, $10.len);
    }
    ;

arg_list_opt:
    /* empty */
    {
      $$.items = NULL;
      $$.len = 0;
    }
    | arg_list
    {
      $$ = $1;
    }
    ;

arg_list:
    expression
    {
      $$.items = ast_list_new($1, &$$.len);
    }
    | arg_list ',' expression
    {
      $$.items = ast_list_push($1.items, &$1.len, $3);
      $$.len = $1.len;
    }
    ;

command:
    FORWARD '(' arg_list_opt ')' ';'
    {
      $$ = ast_make_command("forward", $3.items, $3.len);
    }
    | TURN '(' arg_list_opt ')' ';'
    {
      $$ = ast_make_command("turn", $3.items, $3.len);
    }
    | COLOR '(' arg_list_opt ')' ';'
    {
      $$ = ast_make_command("color", $3.items, $3.len);
    }
    | PENDOWN '(' ')' ';'
    {
      $$ = ast_make_command("penDown", NULL, 0);
    }
    | PENUP '(' ')' ';'
    {
      $$ = ast_make_command("penUp", NULL, 0);
    }
    ;

expression:
    ENTIER
    {
      $$ = ast_make_literal_number((double)$1);
    }
    | REEL
    {
      $$ = ast_make_literal_number($1);
    }
    | CHAINE
    {
      $$ = ast_make_literal_string($1);
      free($1);
    }
    | IDENT
    {
      $$ = ast_make_identifier($1);
      free($1);
    }
    | expression '+' expression
    {
      $$ = ast_make_binary($1, "+", $3);
    }
    | expression '-' expression
    {
      $$ = ast_make_binary($1, "-", $3);
    }
    | expression '*' expression
    {
      $$ = ast_make_binary($1, "*", $3);
    }
    | expression '/' expression
    {
      $$ = ast_make_binary($1, "/", $3);
    }
    | expression '<' expression
    {
      $$ = ast_make_binary($1, "<", $3);
    }
    | expression '>' expression
    {
      $$ = ast_make_binary($1, ">", $3);
    }
    | expression EQ expression
    {
      $$ = ast_make_binary($1, "==", $3);
    }
    | expression NE expression
    {
      $$ = ast_make_binary($1, "!=", $3);
    }
    | expression LE expression
    {
      $$ = ast_make_binary($1, "<=", $3);
    }
    | expression GE expression
    {
      $$ = ast_make_binary($1, ">=", $3);
    }
    | '(' expression ')'
    {
      $$ = $2;
    }
    ;

%%