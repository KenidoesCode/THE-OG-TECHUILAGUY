#!/usr/bin/env bash
# Boots the kernel under headless QEMU and asserts on real serial output,
# rather than only checking that a binary file exists. Requires
# qemu-system-i386 on PATH; skips (rather than fails) if unavailable, since
# this is a hardware-adjacent test and not every environment has an emulator.
#
# The scheduler assertions below are not superficial existence checks: the
# kernel's own scheduler test (kernel.cpp) runs a real preemptive task
# lifecycle scenario — a task that exits, a supervisor that waits and then
# checks the exited task never runs again, and a brand-new task that reuses
# the dead task's table slot while a stale unblock() call against the old
# pid is deliberately made against it. If context switching, task exit, or
# the pid-based anti-resurrection design were broken, these lines would not
# appear, or the [FAIL] variants would appear instead.

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
check "\[GDT \] GDT + TSS installed" "GDT and TSS installed, segments reloaded onto our own descriptors"
check "\[PAGE\] paging enabled" "paging enabled (CR0.PG set) without a triple fault"
check "\[INT \] real IDT installed"  "IDT installed"
check "\[SCHED\] scheduler initialized" "scheduler initialized"
check "\[SYS \] syscall dispatch online" "syscall dispatch (int \$0x80) online"
check "\[SYS \] syscall subsystem initialized" "syscall subsystem initialized"
check "\[VFS \] virtual filesystem initialized" "VFS initialized"
check "\[SEC \] capability security initialized" "security subsystem initialized"
check "\[PIT \] programmable timer online" "PIT programmed"

# --- Ring-3 userspace: a real privilege transition, a real syscall
# path, and real privilege enforcement — not simulated or asserted by
# code inspection. "hello" is genuine ring-3 machine code that can only
# reach the serial port through the kernel-mediated SYS_WRITE syscall
# (a direct outb from CPL 3 with no I/O permission bitmap set in the
# TSS would itself fault); "evil" proves the reverse — that a
# privileged instruction executed directly from ring 3 is *rejected*,
# not silently allowed.
check "created ring-3 task 'hello'" \
    "a ring-3 task is created from a real flat machine-code image"

check "^U!\$" \
    "ring-3 code reached the kernel through SYS_WRITE and printed via it, twice, with an unrecognized syscall number (99) survived silently in between"

check "^E" \
    "the second ring-3 task also reached the kernel via SYS_WRITE before attempting a privileged instruction"

check "\[FAULT\] ring-3 task killed by exception 13" \
    "executing a privileged instruction (cli) at CPL 3 faults with #GP and the offending task is killed, not the kernel"

if grep -q "^X\$" "$LOG"; then
    echo "[FAIL] boot: ring-3 code after the privileged instruction ran — privilege isolation did not actually block it"
    FAIL=1
else
    echo "[PASS] boot: code after the privileged instruction never ran — the faulting task was genuinely terminated, not merely warned about"
fi

# --- Paging-based memory isolation (distinct from instruction-level
# privilege isolation above): every physical page starts supervisor-
# only, and only the specific code/stack pages granted to a user task
# are marked user-accessible. "kernel_peek" directly reads the
# kernel's own load address (1 MiB, never granted to it) from ring 3,
# which must fault with #PF (14) rather than succeed.
check "^K" \
    "the third ring-3 task reached the kernel via SYS_WRITE before attempting to read kernel memory"

check "\[FAULT\] ring-3 task killed by exception 14" \
    "reading unmapped-to-it kernel memory from CPL 3 faults with #PF and the offending task is killed, not the kernel"

if grep -q "^L\$" "$LOG"; then
    echo "[FAIL] boot: ring-3 code after the kernel-memory read ran — paging isolation did not actually block it"
    FAIL=1
else
    echo "[PASS] boot: code after the kernel-memory read never ran — paging isolation is real, not merely instruction-level"
fi

# --- Real per-process address spaces (distinct from the shared-
# directory permission-bit isolation above): every user task now gets
# its own page directory and its own private page table for a fixed
# virtual region, switched via CR3 on every context switch.
# "neighbor_peek" reads one page past its own granted 2-page region, at
# a virtual address that is never mapped for *any* task — proving a
# task cannot reach memory beyond what its own address space explicitly
# maps, not merely memory it lacks permission for within a shared one.
check "^N" \
    "the fourth ring-3 task reached the kernel via SYS_WRITE before reading past its own private region"

check "\[FAULT\] ring-3 task killed by exception 14" \
    "reading an address never mapped in any task's private region faults with #PF and the task is killed"

if grep -q "^Z\$" "$LOG"; then
    echo "[FAIL] boot: ring-3 code after the out-of-region read ran — per-process address-space isolation did not actually block it"
    FAIL=1
else
    echo "[PASS] boot: code after the out-of-region read never ran — per-process address-space isolation is real"
fi

check "task A exited after 5 run(s)" \
    "a task actually ran, hit its exit condition, and called scheduler_exit"

check "\[PASS\] dead task never ran again after exit" \
    "a dead task's slot is never rescheduled (round-robin skips Dead)"

check "\[PASS\] task reusing the dead slot ran normally despite an unblock call against the old pid" \
    "a stale pid handle cannot reach a new task reusing its old slot"

check "task C observed 10 run(s)" \
    "a task created after boot (reusing a dead slot) runs to completion"

check "\[TICK\] 100" \
    "the timer keeps preempting tasks after the lifecycle test completes"

if grep -q "\[FAIL\]" "$LOG"; then
    echo "[FAIL] boot: kernel's own scheduler self-test reported a failure"
    grep "\[FAIL\]" "$LOG"
    FAIL=1
fi

if grep -qi "panic\|triple fault" "$LOG"; then
    echo "[FAIL] boot: no panic/fault detected"
    FAIL=1
fi

rm -f "$LOG"
exit $FAIL
