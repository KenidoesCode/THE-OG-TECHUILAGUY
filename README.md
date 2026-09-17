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

Array indexing is also bounds-checked at run time: `arr[i]` compiles a
single *unsigned* comparison (`index >= size`) before computing the
address, which traps (`exit(101)`) on both a too-large index and a
negative one (a negative `i32` reinterpreted as unsigned is huge, so
one check catches both) instead of silently computing and using an
out-of-bounds address.

Pointers also come in a second, read-only flavor: `constptr` is a
compile-time-only, no-runtime-cost distinction from `ptr` — a `ptr`
value may always widen into a `constptr` (variable, parameter, or
return type), but not the reverse, and `*p = v` is rejected by the type
checker wherever `p`'s declared type is `constptr`, before IR lowering
or codegen ever run (both types lower through the identical
`AddressOfI32`/`LoadI32`/`StoreI32` path and compile to the same raw
address). This is deliberately not called "ownership" or "borrowing"
anywhere: it says nothing about a `ptr` alias to the same storage
writing through it while a `constptr` view exists elsewhere, and
nothing about lifetimes. See
[`docs/ADR/0001-oglang-memory-model.md`](docs/ADR/0001-oglang-memory-model.md)
for the honest, complete accounting of what OGLang's memory model does
and does not guarantee — bounds checking and the const/mut pointer
distinction are real; ownership, borrowing, and use-after-return
detection are not.

OGLang also has struct types: `struct Point { x: i32, y: i32 }` at the
top level, `let p: Point;` for a zero-initialized local (structs have
no literal-initializer syntax, same as arrays), and `p.x`/`p.x = v;`
for field read/write. Field access reuses arrays' exact address
arithmetic and stack layout — each struct-typed local is one more
`arrayGroups` entry (one fresh stack slot per field, allocated
contiguously by the register allocator), and `p.x` lowers to field 0's
address minus a byte offset via the same `AddressOfI32`/`PtrSubI32`
opcodes array indexing uses, except the offset is a `fieldIndex * 8`
constant resolved once by the type checker rather than a runtime
expression — so there is no `MulI32`, and since an unknown field name
is a compile error rather than a possible runtime value, no bounds
check either. Struct types are deliberately restricted for now: fields
must be `i32`/`ptr`/`constptr` (no nested structs, no struct-typed
arrays), and a struct cannot be used as a function parameter or return
type (there is no calling convention yet for passing or returning a
multi-field aggregate) — both rejected explicitly by the type checker
rather than left to silently miscompile.

OGLang also has enums, but deliberately as narrow a feature as the
name suggests: `enum Color { Red, Green, Blue }` at the top level
declares named `i32` constants, not a distinct nominal type — there is
no enum-typed variable, no storage, and no exhaustiveness or pattern
matching. `Color.Red` reuses the identical dot syntax as struct field
access (`FieldAccessExpr`, unchanged in the parser) and resolves
entirely at compile time to its declaration-order ordinal (`0`, `1`,
`2`, ...); when a struct variable and an enum type share a name, the
struct variable takes priority, so real field access is never misread
as an enum lookup. Because there's no storage at all, a variant access
lowers straight to a `ConstI32` immediate — no `AddressOfI32`, no
`PtrSubI32`, no memory access whatsoever, the concrete way this is
lighter-weight than struct field access.

`03-compiler/oglang/tests/e2e_test.sh` runs twenty-three programs
end-to-end and checks their real process exit codes, including cases
specifically chosen to fail under a naive calling convention, an
unconstrained division lowering, superficial/fake spilling,
32-bit-truncated pointers, or missing bounds checks — seven real bugs
were caught this way across this compiler's development (not by
inspection): a naive calling convention corrupting operands, a
division codegen typo, three distinct clobber hazards across parameter
unpacking and argument marshaling, and
two separate instances of the same 64-bit-pointer-truncation bug class
(one in the pointer feature itself, one in array element addressing).
Frontend and codegen invariants are additionally covered by
`03-compiler/oglang/tests/unit_test.sh` (121 assertions). The type checker
also verifies every function returns on
all paths (an `if` without an `else`, or a function ending in a bare
`while` loop, is rejected — a loop may run zero times).
Not yet implemented: generics, traits, ownership/borrowing, `for` loops,
modules, nested/struct-typed-array fields, struct function
parameters/returns, and any type other than `i32`/`ptr`/`constptr`.

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

**Real paging with genuine per-process address spaces**: a shared
32-bit identity-mapped kernel region is built and `CR0.PG` enabled, and
every user task additionally gets its **own page directory and its own
private page table** mapping its code/stack pages at a fixed virtual
address that's identical across tasks — what differs per task is which
physical pages that address actually translates to. `CR3` is switched
on every context switch. No other task's directory has any translation
for another task's private region at all, which is what makes one
process structurally unable to reach another's memory, not merely
denied by a permission bit within one shared directory (the design
this replaced, and a strictly weaker guarantee — under it, any process
could in principle reach any other's pages by guessing their physical
addresses). Two ring-3 programs verify this against real boot behavior:
`22-os/userland/kernel_peek.S` reads the kernel's own load address (in
the *shared* region) and `22-os/userland/neighbor_peek.S` reads one
page past its own granted private region (never mapped in *any* task's
address space); both are confirmed to fault with #PF and be terminated
in isolation, with the kernel and every other task — including the
ongoing scheduler lifecycle test — kept running throughout. No kernel
heap, filesystem, or networking are implemented yet — see
`22-os/README.md`.

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
- [x] Runtime array bounds checking (out-of-range and negative indices both trap; see [ADR 0001](docs/ADR/0001-oglang-memory-model.md))
- [x] `const`/`mut` pointer distinction (`constptr`/`ptr`), compile-time only, no runtime cost — a `ptr` widens into a `constptr`, writes through a `constptr` are rejected by the type checker; not ownership or borrowing (see [ADR 0001](docs/ADR/0001-oglang-memory-model.md))
- [x] Struct types: `struct Name { field: type, ... }`, zero-initialized locals, field read/write (`p.x`, `p.x = v;`) reusing array address arithmetic with compile-time-constant field offsets (no bounds check needed); fields restricted to `i32`/`ptr`/`constptr`, structs not yet supported as function parameters/return types
- [x] Enum types: `enum Name { Variant, ... }`, `Name.Variant` reusing struct field-access dot syntax, resolved entirely at compile time to a declaration-order ordinal — named `i32` constants, not a distinct nominal type; no storage, no exhaustiveness/pattern matching
- [ ] `for` loops, modules, nested/struct-typed-array fields, struct function parameters/returns
- [ ] A real borrow-checking pass, generics, traits, safe concurrency
- [x] **Techuilaguy OS** — boots under QEMU: IDT, PIC/IRQ, syscall ABI foundation, VFS foundation, security foundation
- [x] Techuilaguy OS — real preemptive scheduler: round-robin, task lifecycle (Ready/Running/Blocked/Dead), pid-based anti-resurrection, hardware IRQ separated from scheduling policy (verified by an automated boot test running a real lifecycle scenario)
- [x] Techuilaguy OS — PS/2 keyboard driver (verified against real injected scancodes via QEMU's monitor, not just a unit-tested translation table)
- [x] Techuilaguy OS — GDT + TSS + ring-3 userspace + syscall entry (verified against a real ring-3 program and a real privilege-violation fault, both against actual boot behavior)
- [x] Techuilaguy OS — real paging with genuine per-process address spaces (own page directory/table per task, CR3 switched per context switch; verified against a real ring-3 program reading unmapped kernel memory and a second reading past its own private region, both faulting and killed in isolation)
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
