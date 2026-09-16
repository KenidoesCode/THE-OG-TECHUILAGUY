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
fn main() -> i32 {
    let x: i32 = 10 + 20 * 3;
    return x;
}
```

```
Lexer → Parser → AST → Type Checker → IR → Liveness/Interference →
Register Allocation → x86-64 Codegen → Assembler → Linker → Native ELF → CPU
```

**Status: PROTOTYPE.** Verified end-to-end: `ogc` compiles this program to a
linked x86-64 ELF executable that a Linux process loader actually runs,
exiting with code `70` (`10 + 20 * 3`) — checked by
`03-compiler/oglang/tests/e2e_test.sh`, not just inspected by hand. Frontend
and register allocation are also covered by assertion-based tests in
`03-compiler/oglang/tests/unit_test.sh`. Division, functions with arguments,
control flow codegen, and calling conventions are not yet implemented.

## 🖥️ Techuilaguy OS — second deep system

An x86 (32-bit) freestanding kernel prototype in `22-os/`.

**Status: PROTOTYPE.** Boots under QEMU (Multiboot-compliant), reaches
kernel entry, brings up a physical frame allocator, a real IDT with CPU
exception handlers, the PIC, a scheduler and syscall-ABI foundation, a VFS
foundation, and a capability-security foundation — and exercises a live
IRQ0 hardware interrupt path. All of this is checked by an automated test
(`22-os/tests/boot_test.sh`) that boots the kernel headlessly and asserts on
its serial console output, rather than only checking that a binary exists.
No userspace, drivers beyond the timer/PIC, or filesystem are implemented
yet — see `22-os/README.md`.

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
- [ ] Division, function calls/arguments, control-flow codegen, calling conventions
- [ ] Ownership, borrowing, generics, traits, safe concurrency
- [x] **Techuilaguy OS** — boots under QEMU: IDT, PIC/IRQ, scheduler foundation, syscall ABI foundation, VFS foundation, security foundation (verified by an automated boot test)
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
