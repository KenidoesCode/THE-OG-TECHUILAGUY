#!/usr/bin/env bash
# End-to-end pipeline test: source -> lexer -> parser -> semantic -> IR ->
# register allocation -> x86-64 codegen -> assembler -> linker -> native
# ELF executable -> process execution -> exit code.
#
# This is the only test in the tree that verifies the compiler produces a
# binary that actually runs, as opposed to inspecting intermediate output.
# Several of the programs below are specifically chosen to fail if the
# calling convention or register-constrained division is implemented
# incorrectly, not just to exercise the happy path.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

FAIL=0

check() {
    local desc="$1"
    local source_file="$2"
    local expected="$3"

    ./ogc "$source_file" >/tmp/ogc_e2e_out.txt 2>&1
    if [ $? -ne 0 ]; then
        echo "[FAIL] $desc — compilation failed"
        cat /tmp/ogc_e2e_out.txt
        FAIL=1
        return
    fi

    ./main
    local actual=$?

    if [ "$actual" -eq "$expected" ]; then
        echo "[PASS] $desc — expected $expected, got $actual"
    else
        echo "[FAIL] $desc — expected $expected, got $actual"
        FAIL=1
    fi
}

check "arithmetic precedence (10 + 20 * 3)" \
    "main.og" 70

check "register-constrained integer division (84 / 2)" \
    "tests/programs/division.og" 42

check "if/else control-flow lowering and codegen" \
    "tests/programs/if_else.og" 1

check "recursive function calls; a live value must survive the recursive call" \
    "tests/programs/factorial.og" 120

check "3-argument calling convention (ABI register marshaling)" \
    "tests/programs/multi_arg.og" 42

check "a caller-saved value must survive two separate calls that reuse it" \
    "tests/programs/call_preserves_live_value.og" 21

check "while-loop lowering and back-edge codegen (sum 0..9)" \
    "tests/programs/loop_sum.og" 45

check "nested while loops with independently reset inner counters" \
    "tests/programs/nested_loop.og" 9

check "register pressure beyond the 4-register pool forces a real spill" \
    "tests/programs/register_pressure.og" 21

check "spilled values survive a loop, a branch, and a function call" \
    "tests/programs/spill_stress.og" 32

check "more than 4 arguments requires stack-passed parameters, order-sensitive" \
    "tests/programs/many_args.og" 85

check "spilled loop-mutated values passed as mixed register/stack call arguments" \
    "tests/programs/spill_and_stack_args.og" 76

exit $FAIL
