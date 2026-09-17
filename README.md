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
integration are design targets, not yet implemented — see `spec/OG_LANG_SPEC_V0.1.md`.

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
Register Allocation → x86-64 Codegen (branches, calls, register-
constrained division) → Assembler → Linker → Native ELF → CPU
```

**Status: PROTOTYPE.** Verified end-to-end: `ogc` compiles multi-function
programs — including the recursive one above — to a linked x86-64 ELF
executable that a Linux process loader actually runs. Implemented and
tested: integer arithmetic with correct operand-clobber handling,
register-constrained division (`idivl`/`cdq`), comparisons, `if`/`else`
control flow, and function calls with up to 4 integer arguments under a
stack-mediated calling convention that keeps a caller's live values correct
across nested and repeated calls. `03-compiler/oglang/tests/e2e_test.sh`
runs six programs end-to-end and checks their real process exit codes,
including cases specifically chosen to fail under a naive calling
convention or an unconstrained division lowering (both bugs were caught
this way during development, not by inspection). Frontend and codegen
invariants are additionally covered by `03-compiler/oglang/tests/unit_test.sh`.
Not yet implemented: generics, traits, ownership/borrowing, loops, more than
4 parameters, and any type other than `i32`.

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
proven not to disturb the new occupant. No userspace, drivers beyond the
timer/PIC, or filesystem are implemented yet — see `22-os/README.md`.

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
- [x] Register-constrained division, `if`/`else` control flow, multi-function programs, calls with up to 4 arguments, a correct calling convention across nested/recursive calls
- [ ] Loops, more than 4 parameters, types other than `i32`
- [ ] Ownership, borrowing, generics, traits, safe concurrency
- [x] **Techuilaguy OS** — boots under QEMU: IDT, PIC/IRQ, syscall ABI foundation, VFS foundation, security foundation
- [x] Techuilaguy OS — real preemptive scheduler: round-robin, task lifecycle (Ready/Running/Blocked/Dead), pid-based anti-resurrection, hardware IRQ separated from scheduling policy (verified by an automated boot test running a real lifecycle scenario)
- [ ] Techuilaguy OS — userspace, drivers beyond timer/PIC, filesystem, networking
- [ ] Techuilaguy L1, Storage, Cloud, AI, Quantum, Space Systems

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
