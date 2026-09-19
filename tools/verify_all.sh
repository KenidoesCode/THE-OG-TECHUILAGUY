#!/usr/bin/env bash
# THE OG TECHUILAGUY — universal test runner.
#
# Runs every hosted (no special hardware/emulator required) test
# script across the whole repository and reports real, actually-
# observed PASS/FAIL counts per layer — never fabricated numbers. A
# layer with no entry in the table below has no test suite yet; this
# script does not print a row for a layer it didn't actually run
# anything for.
#
# Requires: g++ (C++20), bash. Some OGLang compiler tests additionally
# invoke `as`/`ld` (GNU binutils) to produce and run real native
# binaries. QEMU-based OS boot tests (boot_test.sh, keyboard_test.sh)
# are intentionally NOT run here — they need qemu-system-i386 and a
# real boot cycle, take much longer, and already skip themselves
# gracefully (see 22-os/tests/boot_test.sh) when qemu isn't available;
# run them directly (`bash 22-os/tests/boot_test.sh`) when you have
# QEMU. This script covers everything else.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TOTAL_PASS=0
TOTAL_FAIL=0
declare -a LAYER_NAMES
declare -a LAYER_RESULTS
OVERALL_OK=1

# Runs one test script, parses its [PASS]/[FAIL] line counts from
# actual output, and records the result. `label` is a human-readable
# name for the summary table; `script` is the path (relative to repo
# root) to execute.
run_suite() {
    local label="$1"
    local script="$2"

    if [ ! -f "$script" ]; then
        echo "  (skipping '$label': $script not found)"
        return
    fi

    local output
    output="$(bash "$script" 2>&1)"
    local exit_code=$?

    local pass_count
    local fail_count
    pass_count="$(printf '%s\n' "$output" | grep -c '^\[PASS\]' || true)"
    fail_count="$(printf '%s\n' "$output" | grep -c '^\[FAIL\]' || true)"

    TOTAL_PASS=$((TOTAL_PASS + pass_count))
    TOTAL_FAIL=$((TOTAL_FAIL + fail_count))

    local status
    if [ "$exit_code" -eq 0 ] && [ "$fail_count" -eq 0 ]; then
        status="PASS"
    else
        status="FAIL"
        OVERALL_OK=0
        echo "----- $label: FAILING OUTPUT -----"
        printf '%s\n' "$output" | tail -30
        echo "----------------------------------"
    fi

    LAYER_NAMES+=("$label")
    LAYER_RESULTS+=("$status ($pass_count passed, $fail_count failed)")
}

echo "THE OG TECHUILAGUY — universal test run"
echo "========================================"
echo

echo "Layer 3/4 — OGLang / Compiler"
run_suite "OGLang unit tests" "03-compiler/oglang/tests/unit_test.sh"

# e2e_test.sh expects an already-built ./ogc binary in place (it is
# not itself a build script) — build it here, the same command used
# throughout this project's own development workflow.
(
    cd "$ROOT/03-compiler/oglang" && \
    g++ -std=c++20 -Wall -Wextra -O2 \
        main.cpp lexer/lexer.cpp parser/parser.cpp types/type_checker.cpp \
        ir/lower.cpp analysis/liveness.cpp analysis/interference.cpp \
        codegen/register_allocator.cpp codegen/x86_64.cpp \
        -I. -o ogc
) > /tmp/ogc_build.log 2>&1
if [ $? -ne 0 ]; then
    echo "  (could not build ogc — see /tmp/ogc_build.log; skipping OGLang end-to-end tests)"
    cat /tmp/ogc_build.log
    OVERALL_OK=0
else
    run_suite "OGLang end-to-end tests" "03-compiler/oglang/tests/e2e_test.sh"
    rm -f "$ROOT/03-compiler/oglang/ogc" "$ROOT/03-compiler/oglang/main" \
          "$ROOT/03-compiler/oglang/main.o" "$ROOT/03-compiler/oglang/main.s"
fi
echo

echo "Layer 5 — OS (hosted-only subset; run boot_test.sh/keyboard_test.sh separately with QEMU)"
run_suite "OS kernel heap"          "22-os/tests/heap_test.sh"
run_suite "OS ELF loader"           "22-os/tests/elf_test.sh"
run_suite "OS keyboard translation" "22-os/tests/keyboard_translation_test.sh"
echo

echo "Layer 6 — Networking"
run_suite "Network protocol codecs" "06-networking/tests/protocols_test.sh"
echo

echo "Layer 7 — Distributed Systems"
run_suite "RPC + fault injection"     "07-distributed-systems/tests/rpc_test.sh"
run_suite "Raft leader election"      "07-distributed-systems/tests/raft_test.sh"
run_suite "Authenticated RPC envelopes" "07-distributed-systems/tests/auth_test.sh"
echo

echo "Layer 8 — Storage"
run_suite "WAL + KV store" "08-storage/tests/storage_test.sh"
echo

echo "Layer 10 — Cryptography"
run_suite "SHA-256"       "10-cryptography/tests/sha256_test.sh"
run_suite "HMAC-SHA256"   "10-cryptography/tests/hmac_test.sh"
echo

echo "Layer 11 — Formal Verification"
run_suite "Property-based tests (cross-layer)" "11-verification/tests/property_tests.sh"
echo

echo "Layer 13 — Developer Ecosystem"
run_suite "OGGit object store" "13-developer-ecosystem/tests/oggit_object_store_test.sh"
run_suite "OGGit refs/HEAD/history" "13-developer-ecosystem/tests/oggit_refs_test.sh"
echo

echo "Layer 15 — Quantum"
run_suite "Quantum state-vector simulator" "15-quantum/tests/qsim_test.sh"
echo

echo "========================================"
echo "SUMMARY"
echo "========================================"
for i in "${!LAYER_NAMES[@]}"; do
    printf "  %-42s %s\n" "${LAYER_NAMES[$i]}" "${LAYER_RESULTS[$i]}"
done
echo
echo "TOTAL: $TOTAL_PASS assertions passed, $TOTAL_FAIL failed"
echo

echo "Layers with NO test suite in this runner yet (not started, or"
echo "hardware/emulator-dependent and run separately):"
echo "  Layers 0-2 (physical foundations / math-CS / digital logic) — not started as standalone code"
echo "  Layer 9 (Security) — not started as its own subsystem"
echo "  Layer 12 (Cloud/edge) — not started"
echo "  Layers 14, 16-21 (AI/ML, Vision, Robotics, Graphics, Scientific Computing, Finance, Space, VLEO) — not started"
echo "  22-os/tests/boot_test.sh, keyboard_test.sh — QEMU-dependent, run separately"
echo

# Each test script builds its own hosted test binary in place (e.g.
# tests/sha256_test_bin) — clean them all up so a verify_all.sh run
# never leaves generated binaries in the working tree.
find "$ROOT" -type f -name '*_bin' -delete
rm -f "$ROOT/03-compiler/oglang/main.s" "$ROOT/03-compiler/oglang/main.o"

if [ "$OVERALL_OK" -eq 1 ]; then
    echo "RESULT: ALL RUN SUITES PASSED"
    exit 0
else
    echo "RESULT: AT LEAST ONE SUITE FAILED — see failing output above"
    exit 1
fi
