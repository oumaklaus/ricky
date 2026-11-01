#include "ricky.h"

static FILE *o;
static int label_count;
static Function *cur_fn;
static int cur_depth;   /* current temporary-stack depth */

static const char *argregs[6] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

static int new_label(void) { return label_count++; }

/* byte offset of temporary slot `depth`, relative to %rbp (below the locals) */
static int temp_offset(int depth) {
    return -(8 * cur_fn->nlocals + 8 * (depth + 1));
}
static void push_tmp(void) {
    if (cur_depth >= RICKY_TEMP_SLOTS) error_at_line(0, "expression too deeply nested");
    fprintf(o, "  mov %%rax, %d(%%rbp)\n", temp_offset(cur_depth));
    cur_depth++;
}
static void pop_tmp(const char *reg) {
    cur_depth--;
    fprintf(o, "  mov %d(%%rbp), %s\n", temp_offset(cur_depth), reg);
}

static void gen_expr(Node *n);

/* binary arithmetic / comparison: lhs in %rdi, rhs in %rax */
static void gen_binary(Node *n) {
    gen_expr(n->lhs);
    push_tmp();
    gen_expr(n->rhs);
    pop_tmp("%rdi");   /* %rdi = lhs, %rax = rhs */

    switch (n->kind) {
    case ND_ADD: fprintf(o, "  add %%rdi, %%rax\n"); break;
    case ND_SUB: fprintf(o, "  sub %%rax, %%rdi\n  mov %%rdi, %%rax\n"); break;
    case ND_MUL: fprintf(o, "  imul %%rdi, %%rax\n"); break;
    case ND_DIV: fprintf(o, "  mov %%rax, %%rcx\n  mov %%rdi, %%rax\n  cqo\n  idiv %%rcx\n"); break;
    case ND_MOD: fprintf(o, "  mov %%rax, %%rcx\n  mov %%rdi, %%rax\n  cqo\n  idiv %%rcx\n  mov %%rdx, %%rax\n"); break;
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_GT: case ND_GE: {
        const char *set = "sete";
        if (n->kind == ND_NE) set = "setne";
        else if (n->kind == ND_LT) set = "setl";
        else if (n->kind == ND_LE) set = "setle";
        else if (n->kind == ND_GT) set = "setg";
        else if (n->kind == ND_GE) set = "setge";
        fprintf(o, "  cmp %%rax, %%rdi\n  %s %%al\n  movzbq %%al, %%rax\n", set);
        break;
    }
    default: error_at_line(0, "codegen: bad binary operator");
    }
}
static void gen_expr(Node *n) {
    switch (n->kind) {
    case ND_NUM:
        fprintf(o, "  mov $%ld, %%rax\n", n->ival);
        return;
    case ND_VAR:
        fprintf(o, "  mov %d(%%rbp), %%rax\n", n->var_offset);
        return;
    case ND_ASSIGN:
        gen_expr(n->rhs);
        fprintf(o, "  mov %%rax, %d(%%rbp)\n", n->var_offset);
        return;
    case ND_NEG:
        gen_expr(n->lhs);
        fprintf(o, "  neg %%rax\n");
        return;
    case ND_NOT:
        gen_expr(n->lhs);
        fprintf(o, "  cmp $0, %%rax\n  sete %%al\n  movzbq %%al, %%rax\n");
        return;
    case ND_AND: {
        int l = new_label();
        gen_expr(n->lhs);
        fprintf(o, "  cmp $0, %%rax\n  je .L.false.%d\n", l);
        gen_expr(n->rhs);
        fprintf(o, "  cmp $0, %%rax\n  je .L.false.%d\n", l);
        fprintf(o, "  mov $1, %%rax\n  jmp .L.end.%d\n", l);
        fprintf(o, ".L.false.%d:\n  mov $0, %%rax\n.L.end.%d:\n", l, l);
        return;
    }
    case ND_OR: {
        int l = new_label();
        gen_expr(n->lhs);
        fprintf(o, "  cmp $0, %%rax\n  jne .L.true.%d\n", l);
        gen_expr(n->rhs);
        fprintf(o, "  cmp $0, %%rax\n  jne .L.true.%d\n", l);
        fprintf(o, "  mov $0, %%rax\n  jmp .L.end.%d\n", l);
        fprintf(o, ".L.true.%d:\n  mov $1, %%rax\n.L.end.%d:\n", l, l);
        return;
    }
    case ND_CALL: {
        for (Node *a = n->args; a; a = a->next) { gen_expr(a); push_tmp(); }
        for (int i = n->nargs - 1; i >= 0; i--) pop_tmp(argregs[i]);
        if (strcmp(n->name, "main") == 0) fprintf(o, "  call main\n");
        else fprintf(o, "  call kf_%s\n", n->name);
        return;
    }
    default:
        gen_binary(n);
        return;
    }
}
static void gen_stmt(Node *n) {
    switch (n->kind) {
    case ND_EXPR_STMT:
        gen_expr(n->lhs);
        return;
    case ND_LET:
        gen_expr(n->rhs);
        fprintf(o, "  mov %%rax, %d(%%rbp)\n", n->var_offset);
        return;
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next) gen_stmt(s);
        return;
    case ND_IF: {
        int l = new_label();
        gen_expr(n->cond);
        fprintf(o, "  cmp $0, %%rax\n  je .L.else.%d\n", l);
        gen_stmt(n->then);
        fprintf(o, "  jmp .L.ifend.%d\n.L.else.%d:\n", l, l);
        if (n->els) gen_stmt(n->els);
        fprintf(o, ".L.ifend.%d:\n", l);
        return;
    }
    case ND_WHILE: {
        int l = new_label();
        fprintf(o, ".L.begin.%d:\n", l);
        gen_expr(n->cond);
        fprintf(o, "  cmp $0, %%rax\n  je .L.wend.%d\n", l);
        gen_stmt(n->body);
        fprintf(o, "  jmp .L.begin.%d\n.L.wend.%d:\n", l, l);
        return;
    }
    case ND_PRINT_INT:
        gen_expr(n->lhs);
        fprintf(o, "  mov %%rax, %%rsi\n  lea .L.fmt(%%rip), %%rdi\n  mov $0, %%eax\n  call printf@PLT\n");
        return;
    case ND_PRINT_STR:
        fprintf(o, "  lea .L.str.%d(%%rip), %%rdi\n  call puts@PLT\n", n->strid);
        return;
    case ND_RETURN:
        if (n->lhs) gen_expr(n->lhs);
        else fprintf(o, "  mov $0, %%rax\n");
        fprintf(o, "  jmp .L.return.%s\n", cur_fn->name);
        return;
    default:
        error_at_line(0, "codegen: unexpected statement");
    }
}
static void gen_function(Function *f) {
    cur_fn = f;
    cur_depth = 0;

    const char *lbl = (strcmp(f->name, "main") == 0) ? "main" : NULL;
    char buf[256];
    if (!lbl) { snprintf(buf, sizeof buf, "kf_%s", f->name); lbl = buf; }

    if (strcmp(f->name, "main") == 0) fprintf(o, "  .globl main\n");
    fprintf(o, "%s:\n", lbl);
    fprintf(o, "  push %%rbp\n  mov %%rsp, %%rbp\n  sub $%d, %%rsp\n", f->frame_size);

    for (int i = 0; i < f->nparams; i++)
        fprintf(o, "  mov %s, %d(%%rbp)\n", argregs[i], -8 * (i + 1));

    for (Node *s = f->body->body; s; s = s->next) gen_stmt(s);

    fprintf(o, "  mov $0, %%rax\n");                 /* default return value */
    fprintf(o, ".L.return.%s:\n", f->name);
    fprintf(o, "  leave\n  ret\n");
}

void codegen(Program *prog, FILE *out) {
    o = out;
    label_count = 0;

    fprintf(o, "  .text\n");
    for (Function *f = prog->funcs; f; f = f->next) gen_function(f);

    fprintf(o, "  .section .rodata\n");
    fprintf(o, ".L.fmt:\n  .string \"%%ld\\n\"\n");
    for (int i = 0; i < prog->nstrs; i++)
        fprintf(o, ".L.str.%d:\n  .string \"%s\"\n", i, prog->strs[i]);
}
