# PROJECT_STATE.md

**Purpose:** an honest, evidence-based record of what is actually built,
tested, and integrated in THE OG TECHUILAGUY, versus what is planned.
Updated as work lands, not retroactively inflated. Maturity labels
follow the PRD's model exactly:

```
PLANNED > DESIGNED > PROTOTYPE > IMPLEMENTED > TESTED > BENCHMARKED
> INTEGRATED > HARDWARE-IN-LOOP > VALIDATED > PRODUCTION-READY
```

A layer with no entry below has not been started. That is not a
placeholder — it is the honest state. Nothing here is claimed complete
because a directory, README, or interface exists.

---

## Layer 3 — OGLang

**Status: TESTED.** A real lexer/parser/type-checker/IR/codegen
pipeline compiles OGLang source to a linked x86-64 ELF executable that
a Linux process loader actually runs.

| Capability | Status | Evidence |
|---|---|---|
| Lexer, parser, AST | TESTED | `03-compiler/oglang/tests/unit_test.sh` |
| Type checker (whole-program, forward/recursive calls, all-paths-return) | TESTED | same |
| IR lowering | TESTED | same |
| Register allocation (Chaitin-style graph coloring) | TESTED | same |
| **Real stack spilling** (forced and coloring-driven) | TESTED | `tests/programs/register_pressure.og`, `spill_stress.og` |
| x86-64 codegen: arithmetic, division (`idivl`/`cdq`), comparisons | TESTED | `tests/e2e_test.sh` |
| Control flow: `if`/`else`, `while` | TESTED | `tests/programs/if_else.og`, `loop_sum.og`, `nested_loop.og` |
| Functions, calls, recursion, ≤4-register + stack-passed args | TESTED | `tests/programs/factorial.og`, `many_args.og` |
| Mutable assignment, unary minus, `//` comments | TESTED | `tests/unit_test.sh` |
| **Pointers**: `&`, `*` read/write, address-taken locals forced to stack | TESTED | `tests/programs/pointer_aliasing.og`, `pointer_spill.og` |
| Memory-safety model | PLANNED | no ADR yet; only raw pointers, no ownership/borrowing |
| Arrays, structs, enums, modules | PLANNED | not started |
| Atomics, volatile, MMIO, inline-asm boundary | PLANNED | not started |
| Types other than `i32`/`ptr` | PLANNED | not started |
| Freestanding/kernel-target compilation | PLANNED | OGLang only targets a hosted Linux ELF process today; the OS kernel itself is still C++/asm |

**Test suite:** 65 unit assertions, 15 end-to-end programs, all passing
from a clean build (`03-compiler/oglang/tests/unit_test.sh` and
`e2e_test.sh`). 5 real bugs were caught by these tests during
development (a calling-convention operand-clobber bug, a division
codegen typo, three distinct parameter/argument marshaling clobber
hazards, and a pointer-truncation bug), not by inspection.

**Explicitly NOT done:** self-hosting (the compiler that compiles
OGLang is itself C++, not OGLang), a written memory-safety-model ADR,
CFG/constant-propagation/DCE as an explicit IR pass (codegen is direct
from a linear IR today), fuzzing, differential testing, golden tests,
crash minimization, an i386 bootstrap backend, cross-compilation.

---

## Layer 5 — Techuilaguy OS

**Status: TESTED (per-subsystem; see table).** Boots under QEMU
(Multiboot), and its own kernel-side self-tests are checked against
real serial output and, where CPU-privilege behavior matters, real
QEMU-emulated hardware — not simulated or asserted by inspection.

| Capability | Status | Evidence |
|---|---|---|
| Multiboot boot, kernel entry, serial console | TESTED | `tests/boot_test.sh` |
| Physical frame allocator | TESTED | reserves through a real `_kernel_end` linker symbol (a `memory_alloc_page()` bug that would have handed back the kernel's own first byte was caught and fixed the first time this allocator got a real caller) |
| Real IDT, CPU exception handlers (vectors 0-31) | TESTED | boot log |
| PIC (remapped), PIT (100 Hz) | TESTED | boot log |
| **Real GDT + TSS** (previously none existed — `0x18` was a hardcoded, unverified assumption inherited from the bootloader) | TESTED | boot log; ring-3 code runs correctly under it |
| **Real preemptive scheduler**: round-robin, Ready/Running/Blocked/Dead lifecycle, pid-based anti-resurrection | TESTED | `tests/boot_test.sh` — a task that exits, a supervisor that proves it never runs again, a new task reusing its dead slot while a stale pid `unblock()` call is proven to miss it |
| **Real ring-3 userspace + syscalls** (`int $0x80`, DPL-3 gate, SYS_WRITE/YIELD/EXIT) | TESTED | `tests/boot_test.sh` — a real assembled ring-3 program reaches the kernel only through syscalls, survives an invalid syscall number |
| **Privilege isolation** (a ring-3 fault kills only that task) | TESTED | `userland/evil.S` executes `cli` at CPL 3; verified faulted, killed, kernel/other tasks kept running |
| **Real paging + per-page memory isolation** | TESTED | `userland/kernel_peek.S` reads the kernel's own load address from CPL 3; verified faults with #PF (14) and is killed, not merely instruction-privilege-isolated |
| PS/2 keyboard driver | TESTED | `tests/keyboard_test.sh` injects real scancodes via QEMU's monitor; translation table separately unit-tested (`keyboard_translation_test.sh`) |
| Syscall argument validation | PROTOTYPE | unrecognized syscall numbers are rejected; argument *values* (e.g. an out-of-range SYS_WRITE character, a bad pointer for a future SYS_READ) are not yet validated |
| Per-process address spaces | PLANNED | paging is one identity-mapped page directory shared by everything; every task sees the same address space, just with different per-page U/S permission |
| Kernel heap | PLANNED | only a physical bump/bitmap page allocator exists; no `malloc`-style kernel heap |
| VFS, security/capability subsystems | PROTOTYPE | `vfs_init()`/`security_init()` exist and print but implement no actual filesystem or capability enforcement yet |
| ELF loader, real process creation from disk, init, shell, utilities | PLANNED | userland programs today are hand-assembled flat binaries embedded into the kernel image at build time, not loaded from any filesystem |
| Filesystem, block device abstraction, file descriptors | PLANNED | not started |
| Drivers beyond timer/PIC/keyboard | PLANNED | not started |
| Documented OGLang kernel-migration stages | PLANNED | not started (OGLang cannot yet target freestanding code at all — see Layer 3) |

**Test suite:** `tests/boot_test.sh` (24 assertions against real QEMU
boot behavior), `tests/keyboard_test.sh` (real injected PS/2 input),
`tests/keyboard_translation_test.sh` (12 hosted unit assertions).

---

## Layers 6-21 (Networking, Distributed Systems, Storage, Security,
Cryptography/PQC, Formal Verification, Cloud, Developer Ecosystem,
AI/ML, Quantum, Robotics, Graphics, Scientific Computing, Finance,
Space Systems, VLEO research)

**Status: PLANNED.** No implementation exists for any of these layers.
This is stated plainly rather than represented by an empty directory,
a stub file, or aspirational documentation, per the project's own
"marketing language is a bug" rule. Each is a substantial,
independently multi-week-to-multi-year engineering effort; none will
be marked above PLANNED until real code, with real tests, against
real (or honestly-labeled simulated) behavior exists.

---

## Honest summary

- Two layers (OGLang, Techuilaguy OS) have real, tested, integrated
  substance, verified against actual compiled/booted behavior, not
  inspection.
- The compiler and OS are integrated with each other only at the
  build-tooling level (both use the same freestanding-capable
  toolchain conventions); OGLang cannot yet compile code that runs
  *inside* the kernel — the documented next step for that integration.
- Every other PRD layer is unstarted.
- No claim of "100% PRD completion" is made or will be made until
  every FR-* in the PRD has real, tested, integrated evidence exactly
  like the entries above — an estimate that, done to this project's
  own stated bar (real code, real tests, no fabricated benchmarks, no
  marketing language), is realistically years of further work, not a
  target reachable by continuing to iterate in isolated sessions.
