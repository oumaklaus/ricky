#ifndef RICKY_H
#define RICKY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Max temporary slots reserved per stack frame for expression evaluation. */
#define RICKY_TEMP_SLOTS 128

/* ---------- Tokens ---------- */
typedef enum {
    TK_INT,      /* integer literal        */
    TK_STR,      /* string literal         */
    TK_IDENT,    /* identifier             */
    TK_KEYWORD,  /* fn let if else while print return */
    TK_PUNCT,    /* operator / punctuation */
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    long ival;    /* TK_INT */
    char *text;   /* TK_IDENT/TK_KEYWORD/TK_STR/TK_PUNCT lexeme (NUL-terminated) */
    int len;
    int line;
};

/* ---------- AST ---------- */
typedef enum {
    /* expressions */
    ND_NUM, ND_VAR, ND_ASSIGN, ND_CALL,
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_GT, ND_GE,
    ND_AND, ND_OR, ND_NOT, ND_NEG,
    /* statements */
    ND_EXPR_STMT, ND_LET, ND_IF, ND_WHILE, ND_BLOCK,
    ND_PRINT_INT, ND_PRINT_STR, ND_RETURN,
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *next;        /* next statement / next arg in a list */

    Node *lhs, *rhs;   /* binary / unary operands            */
    Node *cond, *then, *els, *body; /* control flow          */

    long ival;         /* ND_NUM                             */
    int var_offset;    /* ND_VAR/ND_ASSIGN/ND_LET: rbp offset */
    char *name;        /* ND_VAR/ND_ASSIGN/ND_CALL/ND_LET     */
    int strid;         /* ND_PRINT_STR: index into str table  */

    Node *args;        /* ND_CALL arg list                   */
    int nargs;

    int line;
};

/* ---------- Functions ---------- */
typedef struct Function Function;
struct Function {
    Function *next;
    char *name;
    int nparams;
    char *params[6];
    Node *body;        /* ND_BLOCK */
    int nlocals;       /* params + lets */
    int frame_size;    /* bytes, 16-aligned */
};

/* ---------- Program ---------- */
typedef struct {
    Function *funcs;
    char **strs;
    int nstrs;
} Program;

/* lexer.c */
Token *tokenize(const char *src);

/* parser.c */
Program *parse(Token *tok);

/* codegen.c */
void codegen(Program *prog, FILE *out);

/* main.c */
void error_at_line(int line, const char *fmt, ...);
extern const char *g_filename;

#endif /* RICKY_H */
