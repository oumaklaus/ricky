#define _POSIX_C_SOURCE 200809L
#include "ricky.h"
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

const char *g_filename = "<input>";

void error_at_line(int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (line > 0) fprintf(stderr, "%s:%d: error: ", g_filename, line);
    else          fprintf(stderr, "%s: error: ", g_filename);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { fprintf(stderr, "ricky: cannot open %s: %s\n", path, strerror(errno)); exit(1); }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    size_t rd = fread(buf, 1, sz, fp);
    buf[rd] = 0;
    fclose(fp);
    return buf;
}

int main(int argc, char **argv) {
    const char *input = NULL, *output = "a.out";
    bool emit_asm = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "-S") == 0) emit_asm = true;
        else if (argv[i][0] == '-') { fprintf(stderr, "ricky: unknown option %s\n", argv[i]); return 1; }
        else input = argv[i];
    }
    if (!input) {
        fprintf(stderr, "usage: ricky [-S] [-o output] input.ricky\n");
        fprintf(stderr, "  -S    emit assembly instead of an executable\n");
        return 1;
    }

    g_filename = input;
    char *src = read_file(input);
    Token *tok = tokenize(src);
    Program *prog = parse(tok);

    if (emit_asm) {
        FILE *out = (strcmp(output, "a.out") == 0) ? stdout : fopen(output, "w");
        if (!out) { perror("ricky: fopen"); return 1; }
        codegen(prog, out);
        if (out != stdout) fclose(out);
        return 0;
    }

    /* emit assembly to a temp file, then assemble + link with the C toolchain */
    char tmpl[] = "/tmp/ricky-XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) { perror("ricky: mkstemp"); return 1; }
    FILE *out = fdopen(fd, "w");
    codegen(prog, out);
    fclose(out);

    char cmd[2048];
    snprintf(cmd, sizeof cmd, "cc -x assembler \"%s\" -o \"%s\"", tmpl, output);
    int rc = system(cmd);
    unlink(tmpl);

    if (rc != 0) { fprintf(stderr, "ricky: assembly/link step failed\n"); return 1; }
    return 0;
}
