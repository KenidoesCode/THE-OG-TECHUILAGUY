#!/usr/bin/env bash
# End-to-end pipeline test: source -> lexer -> parser -> semantic -> IR ->
# register allocation -> x86-64 codegen -> assembler -> linker -> native
# ELF executable -> process execution -> exit code.
#
# This is the only test in the tree that verifies the compiler produces a
# binary that actually runs, as opposed to inspecting intermediate output.

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

check "arithmetic precedence (10 + 20 * 3)" "main.og" 70

exit $FAIL
