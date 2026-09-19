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
#
# Environment note: every test script here builds a native binary
# inside its own `tests/` directory. On Windows, running this from a
# WSL2 shell against a Windows-mounted path (`/mnt/c/...`) can hit
# DrvFs 9p permission quirks where a directory reports as writable but
# a linker still can't create the output file in it (or, per some
# reports, an ancestor directory ends up owned by `root` with no
# group/other write bit). This script preflight-checks each test
# directory for real write access and reports that condition as an
# ENVIRONMENT ERROR distinct from an actual code/test failure — see
# `docs/VERIFICATION.md` for the full explanation and the recommended
# fix (clone/copy the repo onto a native Linux filesystem, e.g.
# `$HOME/...` or `/tmp/...`, and run from there).

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TOTAL_PASS=0
TOTAL_FAIL=0
ENV_ERROR_COUNT=0
declare -a LAYER_NAMES
declare -a LAYER_RESULTS
OVERALL_OK=1

# Toolchain preflight: everything here is g++/bash based. Fail fast
# with an unambiguous diagnostic rather than letting every suite
# report a confusing "command not found" individually.
if ! command -v g++ >/dev/null 2>&1; then
    cat <<'EOF'
TOOLCHAIN ERROR:
Required compiler was not found.

Expected command: g++ (a C++20-capable GNU/Clang-compatible g++)

This is a toolchain problem, not a code defect: none of the test
suites below can build without a working g++ on PATH.

Recommended solution:
  - On Debian/Ubuntu (incl. WSL2):  sudo apt install g++
  - On Fedora:                      sudo dnf install gcc-c++
  - On macOS:                       xcode-select --install
  - On native Windows without WSL:  install a Linux environment
    (WSL2 + Ubuntu) and run from there — this project's test scripts
    assume a POSIX shell + GNU toolchain and are not adapted for MSVC.

See docs/VERIFICATION.md for the full supported-environment writeup.
EOF
    exit 4
fi

# Verifies that `dir` (relative to $ROOT) can actually have a file
# created and removed in it — not just that its permission bits look
# writable. This is the check that catches the DrvFs/WSL condition
# described above, where a stat() of the directory can look normal
# while an actual open()-for-write in it still fails.
check_writable_dir() {
    local dir="$1"
    local probe="$dir/.verify_write_probe.$$"
    if ( : > "$probe" ) 2>/dev/null; then
        rm -f "$probe"
        return 0
    fi
    return 1
}

# Prints the standard ENVIRONMENT ERROR diagnostic for a directory
# that failed the writability preflight check.
print_environment_error() {
    local dir="$1"
    cat <<EOF
ENVIRONMENT ERROR:
The repository is located on a filesystem where build artifacts
cannot be created reliably.

Detected path:
$dir

This is NOT a code or test failure — the preflight check confirmed
the test binary for this suite cannot even be written to disk, so the
compiler/linker was not invoked and no code was exercised.

Recommended solution:
Run verification from a native Linux filesystem, for example:

  ~/projects/THE-OG-TECHUILAGUY

or clone the repository into:

  /tmp/og-techuilaguy-verify

See docs/VERIFICATION.md for the full explanation (this is a known
WSL2 + Windows-mounted-path (/mnt/c) DrvFs behavior, not specific to
any one machine or username).
EOF
}

# Runs one test script, parses its [PASS]/[FAIL] line counts from
# actual output, and records the result. `label` is a human-readable
# name for the summary table; `script` is the path (relative to repo
# root) to execute. Before running anything, checks that the script's
# own test directory can actually accept a new file — if not, this is
# classified as an ENVIRONMENT ERROR, never as a code/test failure.
run_suite() {
    local label="$1"
    local script="$2"

    if [ ! -f "$script" ]; then
        echo "  (skipping '$label': $script not found)"
        return
    fi

    local test_dir
    test_dir="$(dirname "$script")"
    if ! check_writable_dir "$test_dir"; then
        echo "----- $label: ENVIRONMENT ERROR -----"
        print_environment_error "$test_dir"
        echo "----------------------------------"
        ENV_ERROR_COUNT=$((ENV_ERROR_COUNT + 1))
        OVERALL_OK=0
        LAYER_NAMES+=("$label")
        LAYER_RESULTS+=("ENVIRONMENT ERROR (build artifacts could not be created in $test_dir)")
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
    elif [ "$fail_count" -gt 0 ]; then
        # The suite built and ran; at least one real assertion failed.
        status="FAIL — CODE/TEST FAILURE"
        OVERALL_OK=0
        echo "----- $label: FAILING OUTPUT -----"
        printf '%s\n' "$output" | tail -30
        echo "----------------------------------"
    else
        # Nonzero exit with zero parsed [FAIL] assertions: the suite
        # never got far enough to run its assertions at all (build/
        # link/toolchain error), as opposed to running and failing.
        status="FAIL — BUILD/TOOLCHAIN FAILURE"
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
if ! check_writable_dir "$ROOT/03-compiler/oglang"; then
    echo "----- OGLang end-to-end tests: ENVIRONMENT ERROR -----"
    print_environment_error "$ROOT/03-compiler/oglang"
    echo "----------------------------------"
    ENV_ERROR_COUNT=$((ENV_ERROR_COUNT + 1))
    OVERALL_OK=0
    LAYER_NAMES+=("OGLang end-to-end tests")
    LAYER_RESULTS+=("ENVIRONMENT ERROR (build artifacts could not be created in 03-compiler/oglang)")
else
    OGC_BUILD_LOG="$(mktemp)"
    (
        cd "$ROOT/03-compiler/oglang" && \
        g++ -std=c++20 -Wall -Wextra -O2 \
            main.cpp lexer/lexer.cpp parser/parser.cpp types/type_checker.cpp \
            ir/lower.cpp analysis/liveness.cpp analysis/interference.cpp \
            codegen/register_allocator.cpp codegen/x86_64.cpp \
            -I. -o ogc
    ) > "$OGC_BUILD_LOG" 2>&1
    if [ $? -ne 0 ]; then
        echo "----- OGLang end-to-end tests: FAILING OUTPUT (FAIL — BUILD/TOOLCHAIN FAILURE) -----"
        echo "  (could not build ogc)"
        cat "$OGC_BUILD_LOG"
        echo "----------------------------------"
        OVERALL_OK=0
        LAYER_NAMES+=("OGLang end-to-end tests")
        LAYER_RESULTS+=("FAIL — BUILD/TOOLCHAIN FAILURE (0 passed, 0 failed)")
    else
        run_suite "OGLang end-to-end tests" "03-compiler/oglang/tests/e2e_test.sh"
        rm -f "$ROOT/03-compiler/oglang/ogc" "$ROOT/03-compiler/oglang/main" \
              "$ROOT/03-compiler/oglang/main.o" "$ROOT/03-compiler/oglang/main.s"
    fi
    rm -f "$OGC_BUILD_LOG"
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
run_suite "OGGit index/staging" "13-developer-ecosystem/tests/oggit_index_test.sh"
run_suite "OGGit checkout" "13-developer-ecosystem/tests/oggit_checkout_test.sh"
run_suite "OGGit diff" "13-developer-ecosystem/tests/oggit_diff_test.sh"
run_suite "OGGit merge" "13-developer-ecosystem/tests/oggit_merge_test.sh"
run_suite "OGForge server foundation" "13-developer-ecosystem/tests/forge_server_test.sh"
echo

echo "Layer 15 — Quantum"
run_suite "Quantum state-vector simulator" "15-quantum/tests/qsim_test.sh"
echo

echo "Layer 18 — Scientific Computing"
run_suite "Vec3 + RK4 numerics" "18-scientific-computing/tests/sci_test.sh"
echo

echo "Layer 20 — Space Systems"
run_suite "Two-body orbital propagation" "20-space-systems/tests/orbital_test.sh"
echo

echo "Layer 23 — Techuilaguy Blockchain L1 (new domain, not in the original PRD numbering — see docs/ADR/0021-techuilaguy-blockchain-l1.md)"
run_suite "L1 accounts/transactions/blocks/persistence" "23-blockchain/tests/l1_test.sh"
echo

echo "Layer 24 — Techuilaguy NetLab (network simulator foundation; part of Layer 6's FR-NET-3 scope — see docs/ADR/0022-techuilaguy-netlab-foundation.md)"
run_suite "NetLab topology + L2 switch simulation" "24-network-simulator/tests/netlab_test.sh"
run_suite "NetLab IPv4/ARP/routing + first mission" "24-network-simulator/tests/ip_routing_test.sh"
run_suite "NetLab packet inspector" "24-network-simulator/tests/packet_inspector_test.sh"
run_suite "NetLab timeline session (step/rewind/replay)" "24-network-simulator/tests/simulation_session_test.sh"
run_suite "NetLab browser-foundation facade (pre-WASM API)" "24-network-simulator/tests/facade_test.sh"
echo

echo "Layer 14 — AI/ML (Tensor + scalar autodiff foundation)"
run_suite "Tensor + autodiff + linear regression training" "14-ai/tests/ai_test.sh"
echo

echo "Layer 17 — Graphics (deterministic software rasterizer foundation)"
run_suite "Software rasterizer (Mat4/FrameBuffer/rasterizer/scene)" "17-graphics/tests/graphics_test.sh"
run_suite "ECS / game-engine foundation (integrated with the rasterizer)" "17-graphics/tests/ecs_test.sh"
echo

echo "Layer 16 — Robotics & autonomy (planar-arm kinematics + P-control foundation)"
run_suite "Planar-arm forward kinematics + proportional control" "16-robotics/tests/robotics_test.sh"
echo

echo "========================================"
echo "SUMMARY"
echo "========================================"
for i in "${!LAYER_NAMES[@]}"; do
    printf "  %-42s %s\n" "${LAYER_NAMES[$i]}" "${LAYER_RESULTS[$i]}"
done
echo
echo "TOTAL: $TOTAL_PASS assertions passed, $TOTAL_FAIL failed"
if [ "$ENV_ERROR_COUNT" -gt 0 ]; then
    echo "ENVIRONMENT ERRORS: $ENV_ERROR_COUNT suite(s) could not even build — see docs/VERIFICATION.md"
fi
echo

echo "Layers with NO test suite in this runner yet (not started, or"
echo "hardware/emulator-dependent and run separately):"
echo "  Layers 0-2 (physical foundations / math-CS / digital logic) — not started as standalone code"
echo "  Layer 9 (Security) — not started as its own subsystem"
echo "  Layer 12 (Cloud/edge) — not started"
echo "  Layers 19, 21 (Finance, VLEO) — not started"
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
elif [ "$ENV_ERROR_COUNT" -gt 0 ] && [ "$TOTAL_FAIL" -eq 0 ]; then
    echo "RESULT: VERIFICATION INCOMPLETE — ENVIRONMENT ERROR (no code/test failures observed;"
    echo "        $ENV_ERROR_COUNT suite(s) could not build artifacts in this environment)"
    exit 3
else
    echo "RESULT: AT LEAST ONE SUITE FAILED — see failing output above"
    exit 1
fi
