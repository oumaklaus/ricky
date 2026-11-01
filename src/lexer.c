#include "ricky.h"
#include <ctype.h>

static const char *kw[] = {
    "fn", "let", "if", "else", "while", "print", "return", NULL
};

static bool is_keyword(const char *s, int len) {
    for (int i = 0; kw[i]; i++)
        if ((int)strlen(kw[i]) == len && strncmp(kw[i], s, len) == 0)
            return true;
    return false;
}

static Token *new_token(TokenKind kind, int line) {
    Token *t = calloc(1, sizeof(Token));
    t->kind = kind;
    t->line = line;
    return t;
}

static char *dup_range(const char *s, int len) {
    char *p = malloc(len + 1);
    memcpy(p, s, len);
    p[len] = 0;
    return p;
}

Token *tokenize(const char *p) {
    Token head = {0};
    Token *cur = &head;
    int line = 1;

    while (*p) {
        if (*p == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)*p)) { p++; continue; }

        /* comments: # ... or // ... to end of line */
        if (*p == '#' || (p[0] == '/' && p[1] == '/')) {
            while (*p && *p != '\n') p++;
            continue;
        }

        /* integer literal */
        if (isdigit((unsigned char)*p)) {
            long v = 0;
            while (isdigit((unsigned char)*p)) { v = v * 10 + (*p - '0'); p++; }
            Token *t = new_token(TK_INT, line);
            t->ival = v;
            cur = cur->next = t;
            continue;
        }

        /* identifier / keyword */
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            int len = (int)(p - start);
            Token *t = new_token(is_keyword(start, len) ? TK_KEYWORD : TK_IDENT, line);
            t->text = dup_range(start, len);
            t->len = len;
            cur = cur->next = t;
            continue;
        }

        /* string literal (escapes are preserved verbatim for the assembler) */
        if (*p == '"') {
            p++;
            char buf[1024];
            int bl = 0;
            while (*p && *p != '"') {
                if (bl >= 1000) error_at_line(line, "string literal too long");
                if (*p == '\\' && p[1]) { buf[bl++] = *p++; buf[bl++] = *p++; }
                else buf[bl++] = *p++;
            }
            if (*p != '"') error_at_line(line, "unterminated string literal");
            p++;
            buf[bl] = 0;
            Token *t = new_token(TK_STR, line);
            t->text = dup_range(buf, bl);
            t->len = bl;
            cur = cur->next = t;
            continue;
        }

        /* two-character operators */
        static const char *puncts2[] = { "==", "!=", "<=", ">=", "&&", "||", NULL };
        bool matched = false;
        for (int i = 0; puncts2[i]; i++) {
            if (strncmp(p, puncts2[i], 2) == 0) {
                Token *t = new_token(TK_PUNCT, line);
                t->text = dup_range(p, 2);
                t->len = 2;
                cur = cur->next = t;
                p += 2;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        /* single-character operators / punctuation */
        if (strchr("+-*/%()<>{};,=!", *p)) {
            Token *t = new_token(TK_PUNCT, line);
            t->text = dup_range(p, 1);
            t->len = 1;
            cur = cur->next = t;
            p++;
            continue;
        }

        error_at_line(line, "unexpected character '%c'", *p);
    }

    cur->next = new_token(TK_EOF, line);
    return head.next;
}
