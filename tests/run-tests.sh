#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

passed=0

pass() {
    passed=$((passed + 1))
    printf 'ok %d - %s\n' "$passed" "$1"
}

compile_and_compare() {
    name=$1
    source=$2
    expected=$3

    "$ROOT/ricky" "$source" -o "$TMP/$name"
    "$TMP/$name" > "$TMP/$name.actual"
    printf '%s' "$expected" > "$TMP/$name.expected"
    diff -u "$TMP/$name.expected" "$TMP/$name.actual"
    pass "$name output"
}

compile_and_compare hello "$ROOT/examples/hello.ricky" 'Hello, Ricky!
'

compile_and_compare fib "$ROOT/examples/fib.ricky" '0
1
1
2
3
5
8
13
21
34
55
'

compile_and_compare factorial "$ROOT/examples/factorial.ricky" 'Factorials:
1
2
6
24
120
720
5040
40320
362880
3628800
'

compile_and_compare fizzbuzz "$ROOT/examples/fizzbuzz.ricky" '1
2
Fizz
4
Buzz
Fizz
7
8
Fizz
Buzz
11
Fizz
13
14
FizzBuzz
'

compile_and_compare semantics "$ROOT/tests/programs/semantics.ricky" '14
20
2
1
-2
-1
1
0
0
0
1
0
21
'

"$ROOT/ricky" "$ROOT/examples/hello.ricky" -S -o "$TMP/hello.s"
grep -Fq '.globl main' "$TMP/hello.s"
grep -Fq 'call puts@PLT' "$TMP/hello.s"
pass 'assembly output mode'

cat > "$TMP/undefined.ricky" <<'EOF'
fn main() {
    print missing;
}
EOF

if "$ROOT/ricky" "$TMP/undefined.ricky" -o "$TMP/undefined" > "$TMP/error.stdout" 2> "$TMP/error.stderr"; then
    echo 'not ok - undefined variables must be rejected' >&2
    exit 1
fi
grep -Fq "undefined variable 'missing'" "$TMP/error.stderr"
pass 'undefined-variable diagnostic'

cat > "$TMP/no-main.ricky" <<'EOF'
fn helper() {
    return 0;
}
EOF

if "$ROOT/ricky" "$TMP/no-main.ricky" -o "$TMP/no-main" > "$TMP/no-main.stdout" 2> "$TMP/no-main.stderr"; then
    echo 'not ok - programs without main must be rejected' >&2
    exit 1
fi
grep -Fq "no 'main' function defined" "$TMP/no-main.stderr"
pass 'missing-main diagnostic'

printf '1..%d\n' "$passed"
