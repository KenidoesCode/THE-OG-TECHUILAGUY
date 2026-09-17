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
| **const/mut pointer distinction** (`ptr` vs `constptr`, compile-time only, no borrow checking) | TESTED | `tests/programs/const_ptr.og`; `tests/unit_tests.cpp` — widening (`ptr`→`constptr`) allowed both ways it can occur (variables, call args), writes through `constptr` and narrowing (`constptr`→`ptr`) rejected; see `docs/ADR/0001-oglang-memory-model.md` amendment for exactly what this does/does not guarantee |
| **Fixed-size arrays**: `i32[N]`, contiguous-slot allocation, pointer-arithmetic indexing | TESTED | `tests/programs/arrays.og`, `arrays_with_calls.og` |
| **Struct types**: `struct Name { field: type, ... }`, zero-initialized locals, field read/write reusing array address arithmetic with compile-time-constant offsets (no bounds check needed) | TESTED | `tests/programs/struct_fields.og`, `struct_with_calls.og`; `tests/unit_tests.cpp` — parsing, field-name/type/struct-type-existence rejection, duplicate-field rejection, struct-as-function-parameter rejection (no calling convention for aggregates yet), well-typed round trip; fields restricted to `i32`/`ptr`/`constptr` (no nested structs, no struct-typed arrays) |
| **Runtime array bounds checking**: out-of-range/negative index traps (exit 101) | TESTED | `tests/programs/array_out_of_bounds.og`, `array_negative_index.og` |
| Memory-safety model | DESIGNED | [`docs/ADR/0001-oglang-memory-model.md`](docs/ADR/0001-oglang-memory-model.md) — explicitly documents the current model as raw/unsafe (C-like), records which mechanisms are tested (pointer aliasing, spilled-pointer correctness, array read/write, bounds checking, const/mut pointers) versus which safety properties are *not* enforced (no use-after-return detection, no aliasing discipline beyond const/mut, no borrow checking), and records candidate next steps. Full borrow-checking remains PLANNED, not started. |
| Enums, modules | PLANNED | not started |
| Atomics, volatile, MMIO, inline-asm boundary | PLANNED | not started |
| Types other than `i32`/`ptr`/`constptr`/user-declared structs | PLANNED | not started |
| Freestanding/kernel-target compilation | PLANNED | OGLang only targets a hosted Linux ELF process today; the OS kernel itself is still C++/asm |

**Test suite:** 75 unit assertions, 19 end-to-end programs, all passing
from a clean build (`03-compiler/oglang/tests/unit_test.sh` and
`e2e_test.sh`). 7 real bugs were caught by these tests during
development (a calling-convention operand-clobber bug, a division
codegen typo, three distinct parameter/argument marshaling clobber
hazards, and the same 64-bit-pointer-truncation bug class appearing
twice independently — once in the pointer feature, once in array
element addressing), not by inspection.

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
| **Real paging with genuine per-process address spaces** | TESTED | every user task gets its own page directory + private page table (`paging_create_address_space`/`paging_map_user_page`), CR3-switched per context switch (`paging_switch_address_space`); `userland/kernel_peek.S` reads the kernel's shared-region load address from CPL 3 and `userland/neighbor_peek.S` reads one page past its own private region (never mapped in any task) — both verified to fault #PF (14) and be killed in isolation, kernel/other tasks unaffected |
| PS/2 keyboard driver | TESTED | `tests/keyboard_test.sh` injects real scancodes via QEMU's monitor; translation table separately unit-tested (`keyboard_translation_test.sh`) |
| Syscall argument validation | PROTOTYPE | unrecognized syscall numbers are rejected; argument *values* (e.g. an out-of-range SYS_WRITE character, a bad pointer for a future SYS_READ) are not yet validated |
| Address-space/page reclaim on task exit | TESTED | reclaim is lazy — deferred to `reclaimSlot`, run just before a dead task's table slot is reused, not at the moment of exit/fault — chosen to avoid freeing the currently-loaded CR3's own directory page mid-fault-handler; recycled stack pages are explicitly zeroed first so no process ever observes another's leftover stack contents; private page tables/directory are freed, the shared kernel region is never touched |
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
