#!/usr/bin/env bash
# Boots the kernel under headless QEMU and asserts on real serial output,
# rather than only checking that a binary file exists. Requires
# qemu-system-i386 on PATH; skips (rather than fails) if unavailable, since
# this is a hardware-adjacent test and not every environment has an emulator.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

if ! command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "[SKIP] boot test — qemu-system-i386 not available in this environment"
    exit 0
fi

make clean >/dev/null
make >/dev/null 2>&1 || { echo "[FAIL] kernel failed to build"; exit 1; }

test -f techuilaguy-os || { echo "[FAIL] techuilaguy-os binary not produced"; exit 1; }

LOG="$(mktemp)"
timeout 10 qemu-system-i386 \
    -kernel techuilaguy-os \
    -serial file:"$LOG" \
    -display none \
    -no-reboot \
    -no-shutdown \
    >/dev/null 2>&1

FAIL=0

check() {
    if grep -q "$1" "$LOG"; then
        echo "[PASS] boot: $2"
    else
        echo "[FAIL] boot: $2"
        FAIL=1
    fi
}

check "TECHUILAGUY OS KERNEL"        "kernel banner printed over serial"
check "\[BOOT\] kernel entered"      "kernel entry reached"
check "\[MEM \] physical frame allocator online" "physical frame allocator initialized"
check "\[INT \] real IDT installed"  "IDT installed"
check "\[SCHED\] scheduler initialized" "scheduler initialized"
check "\[SYS \] syscall subsystem initialized" "syscall subsystem initialized"
check "\[VFS \] virtual filesystem initialized" "VFS initialized"
check "\[SEC \] capability security initialized" "security subsystem initialized"
check "\[PASS\] IRQ0 hardware path survived" "IRQ0 hardware interrupt path verified live"

if grep -qi "panic\|fault\|triple fault" "$LOG"; then
    echo "[FAIL] boot: no panic/fault detected"
    FAIL=1
fi

rm -f "$LOG"
exit $FAIL
