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

check "unary minus on a literal, a variable, and double negation" \
    "tests/programs/unary_minus.og" 5

check "pointer aliasing: mutating *p is visible reading the variable by name" \
    "tests/programs/pointer_aliasing.og" 1

check "a pointer forced to spill is reloaded as a full 64-bit value, not truncated" \
    "tests/programs/pointer_spill.og" 122

check "constptr: a mutable ptr widens into a constptr parameter/variable and reads correctly through it" \
    "tests/programs/const_ptr.og" 42

check "structs: write and read back both fields of a local struct variable" \
    "tests/programs/struct_fields.og" 42

check "structs combined with register pressure and function calls" \
    "tests/programs/struct_with_calls.og" 39

check "enums: variant access resolves to the correct declaration-order ordinal" \
    "tests/programs/enum_variants.og" 42

check "arrays: write through one loop, read back through a separate loop" \
    "tests/programs/arrays.og" 100

check "arrays combined with register pressure and function calls" \
    "tests/programs/arrays_with_calls.og" 22

check "out-of-range array index traps (exit 101) instead of reading past the array" \
    "tests/programs/array_out_of_bounds.og" 101

check "negative array index traps (exit 101) via the same unsigned bounds check" \
    "tests/programs/array_negative_index.og" 101

check "inline asm: a fixed-register (%eax) result is read correctly alongside other live locals" \
    "tests/programs/inline_asm.og" 65

check "inline asm under register pressure: spilled and register-allocated values both survive it" \
    "tests/programs/inline_asm_register_pressure.og" 136

# --- Multi-file modules ---
#
# One source file is one module, named after its filename (without
# extension); `import other;` makes `other`'s functions/structs/enums
# reachable only as `other.symbol`. See
# docs/ADR/0002-oglang-modules.md for the full design. These tests
# compile and link a REAL multi-file program into one native binary,
# the same way check() does for a single file — not just "the parser
# accepts the syntax."

checkMulti() {
    local desc="$1"
    shift
    local expected="${@: -1}"
    local files=("${@:1:$#-1}")

    ./ogc "${files[@]}" >/tmp/ogc_e2e_out.txt 2>&1
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

# A negative multi-file test: compilation (not execution) must fail,
# and stderr must contain a specific substring — proving the failure
# is the *intended* diagnostic, not compilation succeeding by accident
# or failing for an unrelated reason.
checkMultiFails() {
    local desc="$1"
    local expectedSubstring="$2"
    shift 2
    local files=("$@")

    ./ogc "${files[@]}" >/tmp/ogc_e2e_out.txt 2>&1
    if [ $? -eq 0 ]; then
        echo "[FAIL] $desc — compilation unexpectedly succeeded"
        FAIL=1
        return
    fi

    if grep -qF "$expectedSubstring" /tmp/ogc_e2e_out.txt; then
        echo "[PASS] $desc"
    else
        echo "[FAIL] $desc — expected diagnostic containing '$expectedSubstring', got:"
        cat /tmp/ogc_e2e_out.txt
        FAIL=1
    fi
}

checkMulti "modules: cross-module function call, struct type, and enum access, all in one linked binary" \
    "tests/programs/modules/module_main.og" "tests/programs/modules/colors.og" 25

checkMultiFails "modules: importing an unknown module is a compile error" \
    "Unknown module in import: 'doesnotexist'" \
    "tests/programs/modules/bad_import.og" "tests/programs/modules/colors.og"

checkMultiFails "modules: calling an unknown symbol on a real imported module is a compile error" \
    "Call to undefined function: colors.nonexistent" \
    "tests/programs/modules/bad_symbol.og" "tests/programs/modules/colors.og"

checkMultiFails "modules: a circular import (A imports B, B imports A) is rejected, not silently accepted" \
    "Circular module import detected" \
    "tests/programs/modules/cycle_a.og" "tests/programs/modules/cycle_b.og"

exit $FAIL
