#include "ricky.h"

static Token *tk;  /* current token */

/* ---- string table (collected during parse) ---- */
static char **g_strs;
static int g_nstrs, g_strcap;

static int add_string(char *s) {
    if (g_nstrs == g_strcap) {
        g_strcap = g_strcap ? g_strcap * 2 : 8;
        g_strs = realloc(g_strs, g_strcap * sizeof(char *));
    }
    g_strs[g_nstrs] = s;
    return g_nstrs++;
}

/* ---- token helpers ---- */
static bool is_punct(Token *t, const char *s) {
    return t->kind == TK_PUNCT && (int)strlen(s) == t->len && strncmp(t->text, s, t->len) == 0;
}
static bool is_kw(Token *t, const char *s) {
    return t->kind == TK_KEYWORD && strcmp(t->text, s) == 0;
}
static bool equal(const char *s) { return is_punct(tk, s) || is_kw(tk, s); }
static bool consume(const char *s) {
    if (equal(s)) { tk = tk->next; return true; }
    return false;
}
static void expect(const char *s) {
    if (!equal(s)) error_at_line(tk->line, "expected '%s'", s);
    tk = tk->next;
}

/* ---- node constructors ---- */
static Node *new_node(NodeKind k) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = k;
    n->line = tk->line;
    return n;
}
static Node *new_binary(NodeKind k, Node *l, Node *r) {
    Node *n = new_node(k); n->lhs = l; n->rhs = r; return n;
}
static Node *new_unary(NodeKind k, Node *e) {
    Node *n = new_node(k); n->lhs = e; return n;
}

/* ---- expression grammar (low to high precedence) ---- */
static Node *expr(void);
static Node *stmt(void);

static Node *primary(void) {
    if (consume("(")) { Node *n = expr(); expect(")"); return n; }

    if (tk->kind == TK_INT) {
        Node *n = new_node(ND_NUM); n->ival = tk->ival; tk = tk->next; return n;
    }
    if (tk->kind == TK_IDENT) {
        char *name = tk->text; int line = tk->line;
        tk = tk->next;
        if (consume("(")) {                 /* function call */
            Node *n = new_node(ND_CALL);
            n->name = name; n->line = line;
            Node ah = {0}; Node *ac = &ah; int cnt = 0;
            if (!equal(")")) {
                do { ac = ac->next = expr(); cnt++; } while (consume(","));
            }
            expect(")");
            n->args = ah.next; n->nargs = cnt;
            return n;
        }
        Node *n = new_node(ND_VAR); n->name = name; n->line = line; return n;
    }
    if (tk->kind == TK_STR)
        error_at_line(tk->line, "string literals are only allowed in 'print'");
    error_at_line(tk->line, "expected an expression");
    return NULL;
}

static Node *unary(void) {
    if (consume("-")) return new_unary(ND_NEG, unary());
    if (consume("!")) return new_unary(ND_NOT, unary());
    return primary();
}
static Node *mul(void) {
    Node *n = unary();
    for (;;) {
        if (consume("*")) n = new_binary(ND_MUL, n, unary());
        else if (consume("/")) n = new_binary(ND_DIV, n, unary());
        else if (consume("%")) n = new_binary(ND_MOD, n, unary());
        else return n;
    }
}
static Node *add(void) {
    Node *n = mul();
    for (;;) {
        if (consume("+")) n = new_binary(ND_ADD, n, mul());
        else if (consume("-")) n = new_binary(ND_SUB, n, mul());
        else return n;
    }
}
static Node *relational(void) {
    Node *n = add();
    for (;;) {
        if (consume("<")) n = new_binary(ND_LT, n, add());
        else if (consume("<=")) n = new_binary(ND_LE, n, add());
        else if (consume(">")) n = new_binary(ND_GT, n, add());
        else if (consume(">=")) n = new_binary(ND_GE, n, add());
        else return n;
    }
}
static Node *equality(void) {
    Node *n = relational();
    for (;;) {
        if (consume("==")) n = new_binary(ND_EQ, n, relational());
        else if (consume("!=")) n = new_binary(ND_NE, n, relational());
        else return n;
    }
}
static Node *logand(void) {
    Node *n = equality();
    while (consume("&&")) n = new_binary(ND_AND, n, equality());
    return n;
}
static Node *logor(void) {
    Node *n = logand();
    while (consume("||")) n = new_binary(ND_OR, n, logand());
    return n;
}
static Node *assign(void) {
    Node *n = logor();
    if (consume("=")) {
        if (n->kind != ND_VAR) error_at_line(n->line, "invalid assignment target");
        Node *a = new_node(ND_ASSIGN);
        a->name = n->name;
        a->rhs = assign();
        return a;
    }
    return n;
}
static Node *expr(void) { return assign(); }
/* ---- statements ---- */
static Node *block(void) {
    expect("{");
    Node head = {0}; Node *c = &head;
    while (!equal("}")) {
        if (tk->kind == TK_EOF) error_at_line(tk->line, "unexpected end of file, expected '}'");
        c = c->next = stmt();
    }
    expect("}");
    Node *n = new_node(ND_BLOCK);
    n->body = head.next;
    return n;
}

static Node *stmt(void) {
    if (equal("{")) return block();

    if (consume("let")) {
        if (tk->kind != TK_IDENT) error_at_line(tk->line, "expected variable name after 'let'");
        Node *n = new_node(ND_LET);
        n->name = tk->text; tk = tk->next;
        expect("=");
        n->rhs = expr();
        expect(";");
        return n;
    }
    if (consume("if")) {
        Node *n = new_node(ND_IF);
        expect("("); n->cond = expr(); expect(")");
        n->then = stmt();
        if (consume("else")) n->els = stmt();
        return n;
    }
    if (consume("while")) {
        Node *n = new_node(ND_WHILE);
        expect("("); n->cond = expr(); expect(")");
        n->body = stmt();
        return n;
    }
    if (consume("print")) {
        Node *n;
        if (tk->kind == TK_STR) {
            n = new_node(ND_PRINT_STR);
            n->strid = add_string(tk->text);
            tk = tk->next;
        } else {
            n = new_node(ND_PRINT_INT);
            n->lhs = expr();
        }
        expect(";");
        return n;
    }
    if (consume("return")) {
        Node *n = new_node(ND_RETURN);
        if (!equal(";")) n->lhs = expr();
        expect(";");
        return n;
    }
    Node *n = new_node(ND_EXPR_STMT);
    n->lhs = expr();
    expect(";");
    return n;
}
static Function *function(void) {
    expect("fn");
    if (tk->kind != TK_IDENT) error_at_line(tk->line, "expected function name");
    Function *f = calloc(1, sizeof(Function));
    f->name = tk->text; tk = tk->next;
    expect("(");
    if (!equal(")")) {
        do {
            if (tk->kind != TK_IDENT) error_at_line(tk->line, "expected parameter name");
            if (f->nparams >= 6) error_at_line(tk->line, "too many parameters (max 6)");
            f->params[f->nparams++] = tk->text;
            tk = tk->next;
        } while (consume(","));
    }
    expect(")");
    f->body = block();
    return f;
}

/* ---- name resolution: assign stack offsets, validate vars & calls ---- */
typedef struct { char *name; int offset; } Sym;
static Sym syms[256];
static int nsyms, cur_nlocals;
static Function *g_funcs;

static Function *find_func(const char *name) {
    for (Function *f = g_funcs; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    return NULL;
}
static bool lookup(const char *name, int *off) {
    for (int i = 0; i < nsyms; i++)
        if (strcmp(syms[i].name, name) == 0) { *off = syms[i].offset; return true; }
    return false;
}
static int add_local(const char *name, int line) {
    for (int i = 0; i < nsyms; i++)
        if (strcmp(syms[i].name, name) == 0) error_at_line(line, "redeclaration of '%s'", name);
    if (nsyms >= 256) error_at_line(line, "too many local variables");
    int off = -8 * (nsyms + 1);
    syms[nsyms].name = (char *)name;
    syms[nsyms].offset = off;
    nsyms++;
    cur_nlocals++;
    return off;
}
static void resolve_expr(Node *n) {
    if (!n) return;
    switch (n->kind) {
    case ND_NUM:
        return;
    case ND_VAR:
        if (!lookup(n->name, &n->var_offset))
            error_at_line(n->line, "undefined variable '%s'", n->name);
        return;
    case ND_ASSIGN:
        if (!lookup(n->name, &n->var_offset))
            error_at_line(n->line, "assignment to undefined variable '%s'", n->name);
        resolve_expr(n->rhs);
        return;
    case ND_CALL: {
        Function *f = find_func(n->name);
        if (!f) error_at_line(n->line, "call to undefined function '%s'", n->name);
        if (f->nparams != n->nargs)
            error_at_line(n->line, "function '%s' expects %d argument(s) but got %d",
                          n->name, f->nparams, n->nargs);
        for (Node *a = n->args; a; a = a->next) resolve_expr(a);
        return;
    }
    case ND_NOT: case ND_NEG:
        resolve_expr(n->lhs);
        return;
    default:
        resolve_expr(n->lhs);
        resolve_expr(n->rhs);
        return;
    }
}

static void resolve_stmt(Node *n) {
    if (!n) return;
    switch (n->kind) {
    case ND_LET:
        resolve_expr(n->rhs);
        n->var_offset = add_local(n->name, n->line);
        return;
    case ND_IF:
        resolve_expr(n->cond); resolve_stmt(n->then); resolve_stmt(n->els); return;
    case ND_WHILE:
        resolve_expr(n->cond); resolve_stmt(n->body); return;
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next) resolve_stmt(s);
        return;
    case ND_PRINT_INT: case ND_RETURN: case ND_EXPR_STMT:
        resolve_expr(n->lhs); return;
    default:
        return;
    }
}
static void resolve_function(Function *f) {
    nsyms = 0;
    cur_nlocals = 0;
    for (int i = 0; i < f->nparams; i++) add_local(f->params[i], 0);
    for (Node *s = f->body->body; s; s = s->next) resolve_stmt(s);
    f->nlocals = cur_nlocals;
    int bytes = 8 * cur_nlocals + 8 * RICKY_TEMP_SLOTS;
    f->frame_size = (bytes + 15) & ~15;
}

Program *parse(Token *tok) {
    tk = tok;
    Function head = {0}; Function *c = &head;
    while (tk->kind != TK_EOF) {
        if (!equal("fn")) error_at_line(tk->line, "expected 'fn' at top level");
        c = c->next = function();
    }
    g_funcs = head.next;

    for (Function *f = g_funcs; f; f = f->next) resolve_function(f);
    if (!find_func("main")) error_at_line(0, "no 'main' function defined");

    Program *prog = calloc(1, sizeof(Program));
    prog->funcs = g_funcs;
    prog->strs = g_strs;
    prog->nstrs = g_nstrs;
    return prog;
}
