<div align="center">

# ⚡ THE OG TECHUILAGUY

### A Complete Computing Civilization — From First Principles to a Brighter Tomorrow

*One repository. One evolving ecosystem.*

![Status](https://img.shields.io/badge/status-early--development-orange?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-blue?style=for-the-badge)
![Made with](https://img.shields.io/badge/made%20with-first%20principles-purple?style=for-the-badge)
![PRs](https://img.shields.io/badge/PRs-welcome-brightgreen?style=for-the-badge)

`mathematics` → `hardware` → `compilers` → `os` → `networking` → `security` → `distributed systems` → `databases` → `cloud` → `ai` → `quantum` → `space`

</div>

---

## 🧭 Mission

Most engineers stop at *"use the compiler," "use the OS," "use the cloud."*
**THE OG TECHUILAGUY** asks what's underneath, and tries to build it.

```
Consumer of abstractions → Builder → Designer → Researcher → Ecosystem
```

## 🏗️ Civilization Stack

```
┌─────────────────────────────────────────────┐
│ Applications • Ecosystem • Fintech • Twin   │
│ AI • Robotics • Vision • Graphics • HPC      │
│ Quantum • GPU • Autonomous Computing         │
│ Distributed Systems • Blockchain • Cloud     │
│ Networking • Cryptography • Security         │
│ Operating Systems • Compilers • Languages    │
│ CPU • ISA • Hardware • Digital Logic         │
│ Mathematics • Algorithms • Foundations       │
└─────────────────────────────────────────────┘
```

## ⚙️ OGLang — the first deep system

A systems language, built alongside its own compiler. Memory safety and PQC
integration are design targets, not yet implemented — see `spec/OG_LANG_SPEC_V0.1.md`
and, for the honest current state of the memory model specifically,
[`docs/ADR/0001-oglang-memory-model.md`](docs/ADR/0001-oglang-memory-model.md)
(raw, C-like pointers today: real and tested as a *mechanism*, with no
bounds checking, use-after-return detection, or borrow checking — not
"memory safety" in the sense the PRD ultimately targets).

```rust
fn fact(n: i32) -> i32 {
    if (n <= 1) {
        return 1;
    }

    return n * fact(n - 1);
}

fn main() -> i32 {
    return fact(5);
}
```

```
Lexer → Parser → AST → Type Checker → IR → Liveness/Interference →
Register Allocation (with real stack spilling) → x86-64 Codegen
(branches, loops, calls, register-constrained division) →
Assembler → Linker → Native ELF → CPU
```

**Status: PROTOTYPE.** Verified end-to-end: `ogc` compiles multi-function
programs — including the recursive one above — to a linked x86-64 ELF
executable that a Linux process loader actually runs. Implemented and
tested: integer arithmetic with correct operand-clobber handling,
register-constrained division (`idivl`/`cdq`), unary minus, comparisons,
`if`/`else` and `while` control flow, mutable-variable assignment, and
function calls
with any number of integer arguments (the first 4 in registers per a
restricted System V AMD64 subset, the rest caller-cleanup stack-passed)
under a calling convention that keeps a caller's live values correct
across nested and repeated calls. The register allocator does real
Chaitin-style graph coloring with spilling: a program with more
simultaneously-live values than the 4 available registers compiles and
runs correctly, with the excess spilled to an `rbp`-relative stack frame
instead of failing to compile.
Pointers are also real: `&x` takes the address of a local or parameter,
forcing it into a stable stack slot instead of a register (the register
allocator is told exactly which values are address-taken and excludes
them from coloring entirely, rather than hoping graph coloring happens
to spill them); `*p` reads and `*p = v` writes through the resulting
`ptr` value. Pointers are handled as genuine 64-bit addresses in codegen
(`leaq`, 64-bit spill slots) even though every other OGLang value is
32-bit — a stack address routinely lives above the 4 GiB boundary on a
real 64-bit process, and the first version of this feature computed and
spilled addresses through 32-bit registers/slots, silently truncating
them into garbage (an immediate segfault on the first real test, caught
by that test, not by inspection).

Fixed-size arrays build directly on the pointer machinery: `let arr:
i32[N];` reserves N *contiguous* stack slots (a new register-allocator
capability — a plain forced spill only guarantees each value gets some
slot, not that a whole group's slots are adjacent and in index order),
and `arr[i]`/`arr[i] = v` compute the element's address as element 0's
address minus `i * 8`, reusing the exact same `AddressOfI32`/`LoadI32`/
`StoreI32` opcodes pointers already use. The one genuinely new piece —
that address arithmetic — hit the *same* 64-bit-truncation bug class a
second time: the natural first implementation subtracted the byte
offset using the generic 32-bit `SubI32` codegen, silently truncating
the real 64-bit address `AddressOfI32` had just correctly computed.
Fixed with a dedicated `PtrSubI32` opcode that does the subtraction in
a 64-bit register, caught by the first real array test, not by
inspection.

`03-compiler/oglang/tests/e2e_test.sh` runs seventeen programs
end-to-end and checks their real process exit codes, including cases
specifically chosen to fail under a naive calling convention, an
unconstrained division lowering, superficial/fake spilling, or
32-bit-truncated pointers — seven real bugs were caught this way across
this compiler's development (not by inspection): a naive calling
convention corrupting operands, a division codegen typo, three distinct
clobber hazards across parameter unpacking and argument marshaling, and
two separate instances of the same 64-bit-pointer-truncation bug class
(one in the pointer feature itself, one in array element addressing).
Frontend and codegen invariants are additionally covered by
`03-compiler/oglang/tests/unit_test.sh` (72 assertions). The type checker
also verifies every function returns on
all paths (an `if` without an `else`, or a function ending in a bare
`while` loop, is rejected — a loop may run zero times).
Not yet implemented: generics, traits, ownership/borrowing, `for` loops,
structs, enums, modules, and any type other than `i32`/`ptr`.

## 🖥️ Techuilaguy OS — second deep system

An x86 (32-bit) freestanding kernel prototype in `22-os/`.

**Status: PROTOTYPE.** Boots under QEMU (Multiboot-compliant), reaches
kernel entry, brings up a physical frame allocator, a real IDT with CPU
exception handlers, the PIC, and a syscall-ABI/VFS/security foundation.

The scheduler runs real preemptive multitasking: hardware IRQ0 handling
(PIC EOI, raw tick counting) is architecturally separate from software
scheduling policy (round-robin selection, task lifecycle) — both the
100 Hz timer and a software `int $0x81` yield vector drive the same
policy function, which returns the kernel stack pointer to resume,
letting `isr_common` context-switch by simply loading a different task's
saved trap frame. Task lifecycle (Ready/Running/Blocked/Dead) is tracked
per-task by a permanently unique pid, not by table-slot index, which is
what makes resurrecting a dead task structurally impossible rather than
merely avoided by convention: a dead task's slot can be reused by a new
task with a new pid, and any code still holding the old pid (e.g. a stale
`scheduler_unblock` call) can no longer reach it.

All of this is checked by `22-os/tests/boot_test.sh`, which boots the
kernel headlessly and asserts on real serial console output from the
kernel's own scheduler self-test in `kernel.cpp`: a task that runs a fixed
number of times and exits, a supervisor task that then proves the exited
task is never scheduled again, and a third task that reuses the dead
task's slot while a stale-pid unblock call is made against it and is
proven not to disturb the new occupant.

A real PS/2 keyboard driver reads scancodes from IRQ1 and translates them
to ASCII (US QWERTY, unshifted). `22-os/tests/keyboard_test.sh` boots the
kernel and injects real scancodes through QEMU's monitor to verify the
hardware-facing path end-to-end; the translation table itself has no
hardware I/O and is separately unit-tested with a hosted compiler
(`22-os/tests/keyboard_translation_test.sh`).

**Real ring-3 userspace**: a from-scratch GDT + TSS (there was no GDT at
all before this — `isr_common` and the scheduler both hardcoded `0x18`
as "the kernel data selector", an unverified assumption inherited from
the bootloader's own default), a syscall entry (`int $0x80`, a
dedicated DPL-3 IDT gate) reaching a real dispatcher (`SYS_WRITE`,
`SYS_YIELD`, `SYS_EXIT` implemented), and genuine privilege enforcement:
a real flat-machine-code ring-3 program (`22-os/userland/hello.S`)
reaches the kernel only through syscalls and survives an invalid
syscall number without crashing, while a second one
(`22-os/userland/evil.S`) executes a privileged instruction (`cli`)
directly from CPL 3 and is verified to fault (#GP) and be terminated in
isolation — the kernel and every other task keep running, and the
faulting program's own code after that point is verified to never
execute.

**Real paging-based memory isolation**: a 32-bit identity-mapped page
directory/table set is built and `CR0.PG` enabled; every page starts
supervisor-only, and a user task is granted access to only the exact
two pages (code, stack) allocated for it. This is a stronger guarantee
than the instruction-level isolation above — without it, a flat
0..4 GiB segment limit gave ring-3 code full read/write access to *all*
physical memory, including the kernel's own. A third program
(`22-os/userland/kernel_peek.S`) directly reads the kernel's own load
address from ring 3 and is verified to fault with #PF and be terminated
the same way. No per-process address spaces, filesystem, or networking
are implemented yet — see `22-os/README.md`.

## 🧩 Domains

| # | Domain | # | Domain |
|---|---|---|---|
| 01 | Foundations | 15 | Search |
| 02 | Hardware | 16 | Browser |
| 03 | Compiler | 17 | Developer Platform |
| 04 | OS | 18 | Robotics |
| 05 | Networking | 19 | Computer Vision |
| 06 | Cryptography | 20 | Graphics |
| 07 | Cybersecurity | 21 | Gaming |
| 08 | Distributed Systems | 22 | Fintech |
| 09 | Blockchain | 23 | Scientific Computing |
| 10 | Storage & Databases | 24 | Futuristic Computing |
| 11 | Cloud Infrastructure | 25 | Research |
| 12 | AI/ML | 26 | Documentation |
| 13 | GPU/HPC | 27 | Space Systems |
| 14 | Quantum | | |

## 🗺️ Roadmap

- [x] Repository + OGLang v0.1 spec
- [x] Lexer → Parser → AST → Type Checker → IR → Register Allocation → x86-64 → linked native ELF executable, verified by an end-to-end test
- [x] Register-constrained division, `if`/`else` control flow, multi-function programs, calls with any number of arguments (register + stack-passed), a correct calling convention across nested/recursive calls
- [x] `while` loops, mutable-variable assignment, real register-allocator spilling (Chaitin-style graph coloring, `rbp`-relative stack slots), all-paths-return checking
- [x] Real pointers: `&`/`*` (address-of, load, store) as genuine 64-bit addresses, with address-taken locals forced into stable stack slots
- [x] Fixed-size arrays: `i32[N]` with contiguous-slot allocation and pointer-arithmetic-based indexing
- [ ] `for` loops, structs, enums, modules, types other than `i32`/`ptr`
- [ ] Ownership, borrowing, generics, traits, safe concurrency
- [x] **Techuilaguy OS** — boots under QEMU: IDT, PIC/IRQ, syscall ABI foundation, VFS foundation, security foundation
- [x] Techuilaguy OS — real preemptive scheduler: round-robin, task lifecycle (Ready/Running/Blocked/Dead), pid-based anti-resurrection, hardware IRQ separated from scheduling policy (verified by an automated boot test running a real lifecycle scenario)
- [x] Techuilaguy OS — PS/2 keyboard driver (verified against real injected scancodes via QEMU's monitor, not just a unit-tested translation table)
- [x] Techuilaguy OS — GDT + TSS + ring-3 userspace + syscall entry (verified against a real ring-3 program and a real privilege-violation fault, both against actual boot behavior)
- [x] Techuilaguy OS — real paging + per-page memory isolation (verified against a real ring-3 program reading unmapped kernel memory and faulting)
- [ ] Techuilaguy OS — per-process address spaces, kernel heap, storage/network drivers, filesystem
- [ ] Techuilaguy L1, Storage, Cloud, AI, Quantum, Space Systems

See [`PROJECT_STATE.md`](./PROJECT_STATE.md) for the full, honestly-labeled
per-layer status of every PRD requirement, not just this summary.

## 🔐 Principles

**Security** → threat model → implementation → tests → fuzzing → review
**Crypto** → classical → lattice → PQC → hybrid → crypto-agility
**Everything** → measured, verified, documented, reproducible — no claims without benchmarks.

## 📚 Structure

```
THE-OG-TECHUILAGUY/
├── 01-foundations/ … 27-space-systems/
├── docs/ · website/
├── README.md · ARCHITECTURE.md · ROADMAP.md
└── CONTRIBUTING.md · SECURITY.md · LICENSE
```

## 🤝 Contributing

Code, docs, tests, benchmarks, research, and security reviews are all welcome.
See `CONTRIBUTING.md` · `SECURITY.md` · `CODE_OF_CONDUCT.md`.

---

<div align="center">

**UNDERSTAND → BUILD → CONNECT → VERIFY → MEASURE → IMPROVE → SHARE**

*From first principles to a brighter tomorrow.*

⭐ Star this repo if you believe computing should be understood, not just used.

</div>
