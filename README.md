# Ricky

Ricky is a small, statically-compiled programming language with a compiler
written from scratch in C. The compiler lexes and parses Ricky source, then
emits x86-64 assembly (AT&T syntax) which is assembled and linked into a native
executable using the system C toolchain.

It's deliberately simple — one data type (64-bit signed integers), functions,
control flow, and printing — but it's a *real* compiler: no interpreter, no VM.
Your `.ricky` file becomes a standalone ELF binary.

## Build

```sh
make
```

This produces the `ricky` compiler in the project root. Requirements: a C
compiler (`cc`/`gcc`), an assembler (`as`), and `make` — all standard on Linux.

## Usage

```sh
./ricky program.ricky -o program   # compile to a native executable
./program                          # run it

./ricky program.ricky -S           # print the generated assembly to stdout
./ricky program.ricky -S -o out.s  # write the assembly to a file
```

Run every bundled example at once:

```sh
make examples
```

## Testing

Run the complete automated suite with:

```sh
make test
```

The suite performs clean end-to-end compiler checks: it compiles and runs all
bundled examples, verifies arithmetic and control-flow semantics, checks
assembly output, and confirms that invalid programs produce useful errors.
GitHub Actions runs the same command for every push and pull request.

## Language tour

```
# Comments start with '#' or '//'.

fn add(a, b) {           # functions take up to 6 parameters
    return a + b;
}

fn main() {              # execution starts at main; its return value
    let x = 10;          # is the process exit code
    let total = 0;

    while (x > 0) {
        total = total + x;
        x = x - 1;
    }

    if (total % 2 == 0) {
        print "even sum";
    } else {
        print "odd sum";
    }

    print total;             # prints an integer, followed by a newline
    print add(2, 3);         # calls are expressions
    return 0;
}
```

### What's in the language

- **Values:** 64-bit signed integers only.
- **Variables:** `let name = expr;` declares a function-local variable;
  `name = expr;` reassigns it. Variables are function-scoped.
- **Operators** (highest precedence first):
  - unary `-` `!`
  - `*` `/` `%`
  - `+` `-`
  - `<` `<=` `>` `>=`
  - `==` `!=`
  - `&&`  (short-circuit)
  - `||`  (short-circuit)
  - `=`   (assignment)
- **Control flow:** `if (cond) { ... } else { ... }` and `while (cond) { ... }`.
  Any non-zero value is "true". `else if` chains work naturally.
- **Functions:** `fn name(params) { ... }`, with `return expr;`. Recursion is
  supported. A function that runs off the end returns `0`.
- **Printing:** `print expr;` prints an integer with a trailing newline;
  `print "text";` prints a string literal (supports `\n`, `\t`, `\\`, `\"`).
- **Entry point:** a `main` function is required.

### Grammar

```
program    = function* ;
function   = "fn" ident "(" (ident ("," ident)*)? ")" block ;
block      = "{" statement* "}" ;
statement  = block
           | "let" ident "=" expr ";"
           | "if" "(" expr ")" statement ("else" statement)?
           | "while" "(" expr ")" statement
           | "print" (string | expr) ";"
           | "return" expr? ";"
           | expr ";" ;
expr       = assign ;
assign     = logor ("=" assign)? ;
logor      = logand ("||" logand)* ;
logand     = equality ("&&" equality)* ;
equality   = relational (("==" | "!=") relational)* ;
relational = add (("<" | "<=" | ">" | ">=") add)* ;
add        = mul (("+" | "-") mul)* ;
mul        = unary (("*" | "/" | "%") unary)* ;
unary      = ("-" | "!") unary | primary ;
primary    = integer | ident | ident "(" args? ")" | "(" expr ")" ;
```

## How the compiler works

The compiler is a classic four-stage pipeline:

| Stage      | File              | Job                                                            |
|------------|-------------------|---------------------------------------------------------------|
| Lexer      | `src/lexer.c`     | Turns source text into a linked list of tokens.               |
| Parser     | `src/parser.c`    | Recursive-descent parse into an AST; resolves names, assigns each local a stack slot, and validates calls. |
| Code gen   | `src/codegen.c`   | Walks the AST and emits x86-64 assembly.                       |
| Driver     | `src/main.c`      | Reads files, drives the pipeline, invokes `cc` to assemble and link. |

Code generation uses a simple stack-machine model: each expression leaves its
result in `%rax`, and intermediate values are spilled to a fixed region of the
current stack frame rather than via `push`/`pop`. Because `%rsp` stays fixed and
16-byte aligned for the whole function body, every `call` is correctly aligned
per the System V AMD64 ABI. Function parameters follow the standard calling
convention (`%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`). `print` is lowered to
calls to libc's `printf`/`puts`.

## Project layout

```text
.
├── src/                 Compiler implementation
├── examples/            Example Ricky programs
├── tests/               End-to-end tests and test programs
├── .github/workflows/   GitHub Actions configuration
├── Makefile             Build, examples, tests, and cleanup
└── README.md            Language and project documentation
```

## Examples

- `examples/hello.ricky` — hello world
- `examples/fib.ricky` — recursive Fibonacci
- `examples/factorial.ricky` — iterative factorial
- `examples/fizzbuzz.ricky` — FizzBuzz

## Limitations

Ricky is intentionally minimal. There are no strings as values (only as `print`
literals), no arrays, structs, globals, or floating point; functions take at
most 6 parameters; and every local declaration requires an initializer. The
compiler currently targets Linux on x86-64. These are natural next steps rather
than fundamental obstacles.

## Contributing

Bug reports and small improvements are welcome. Read
[`CONTRIBUTING.md`](CONTRIBUTING.md) before submitting a change.
