# THE OG TECHUILAGUY — Product Requirements Document (PRD)

**Status:** Living document · **Version:** 1.0 · **Owner:** KK (Kenidoescode)
**Repository:** https://github.com/KenidoesCode/THE-OG-TECHUILAGUY
**Companion docs:** [`ARCHITECTURE.md`](./ARCHITECTURE.md) · [`PLAN.md`](./PLAN.md) · [`PROJECT_STATE.md`](./PROJECT_STATE.md)

---

## 0. How to read this document

This PRD defines **what** THE OG TECHUILAGUY is, **why** it exists, **who** it serves, and **what "done" means** for each layer. It deliberately does *not* define implementation detail — that lives in `ARCHITECTURE.md` (the *how*) and `PLAN.md` (the *when* and *in what order*).

One rule governs every line below and is the reason this project can be trusted:

> **Nothing is claimed to work until code, a passing test, or a reproducible measurement demonstrates it.** Every capability carries an explicit maturity label. Marketing language is a bug.

---

## 1. Product vision

**One sentence:** THE OG TECHUILAGUY is a single, open-source monorepo that rebuilds a complete computing stack — from mathematics and digital logic, through a self-hosting language/compiler/OS, up to distributed systems, AI, cryptography, and space-systems simulation — as one coherent, integrated, verifiable civilization.

**The defining property is not that every component is finished. It is that every component has a documented, tested path toward integration into one system.**

This is explicitly a **learn-by-building** project of extreme depth. Its purpose is to understand every abstraction in modern computing from its underlying mechanism rather than as a black box — and to leave behind an artifact that others can inspect, build, and learn from.

### 1.1 What this is
- A **first-principles reconstruction** of the computing stack.
- A **single integrated repository** (one flagship, not many scattered projects).
- An **honest engineering record**: every claim is backed by evidence or labeled as a hypothesis.
- A **teaching artifact**: the *why* and *how* are documented alongside the *what*.

### 1.2 What this is NOT (non-goals)
- Not a claim that any subsystem is production-ready unless its acceptance criteria say so.
- Not a claim of absolute security, "unhackable" systems, or "quantum-proof" cryptography.
- Not a set of proprietary clones — it builds original architectures, and where it interoperates with standards it names them accurately.
- Not an operational spacecraft, radio transmitter, or launched mission. Space work is simulation and hardware-in-the-loop until proper authorization exists.
- Not a project that hides its bootstrap dependencies (C/C++, LLVM, NASM, QEMU, etc.). Those are named openly and progressively removed.

---

## 2. Problem statement & motivation

Modern computing is built on layers most engineers treat as opaque: the compiler is magic, the kernel is magic, the network stack is magic, the crypto library is magic, the satellite link is magic. Depth of understanding across the *full* stack is rare because no single project spans it.

**THE OG TECHUILAGUY exists to close that gap for its builder and its readers** by constructing the stack end-to-end, with every layer:
1. understood from first principles,
2. implemented in buildable code,
3. tested (including failure cases),
4. documented with its assumptions and limits,
5. and connected to the layers above and below it.

The measure of success is not scale for its own sake. It is **verifiable depth**: can each layer be built, tested, broken deliberately, and explained?

---

## 3. Target users & stakeholders

| User | Need | How the project serves it |
|---|---|---|
| **The builder (KK)** | Master the full computing stack; produce a portfolio-defining body of work | Structured, dependency-ordered milestones with acceptance criteria |
| **Engineering reviewers / employers** | Evidence of genuine, tested engineering skill | Reproducible builds, passing tests, honest maturity labels, ADRs |
| **Learners / open-source community** | A readable reference implementation of computing internals | Documentation-first subsystems, teaching notes, research logs |
| **Research collaborators** | A credible sandbox for PQC, distributed systems, and space-systems experiments | Clearly separated research branches with stated assumptions |

---

## 4. Guiding product principles

1. **Open source** — inspectable, reproducible, buildable by others wherever practical.
2. **First principles** — understand mechanisms, not just interfaces.
3. **Measurable engineering** — claims require code, tests, measurements, or a labeled research hypothesis.
4. **Security by design** — threat modeling, isolation, verification, and supply-chain integrity cross every layer.
5. **Honest maturity labeling** — every subsystem states exactly how mature it is (see §6).
6. **Modularity with one identity** — subsystems have explicit interfaces yet remain one project.
7. **Interoperability over reinvention-of-names** — standards are named as standards; original work is named as original.
8. **Progressive self-hosting** — bootstrap dependencies are named and removed over time.
9. **Teach as you build** — the human remains the architect; AI assists but never hides complexity.

---

## 5. Scope — the layer map

The product spans the following layers. Each is a **capability area**, not a promise of completion. Ordering reflects dependency, not priority of interest.

```
Layer 0   Physical foundations (physics, energy, materials, semiconductors)
Layer 1   Mathematics & CS foundations (algorithms, data structures, numerics, info theory)
Layer 2   Digital logic & hardware (gates → ALU → CPU → Techuilaguy ISA)
Layer 3   OGLang (the project's own systems programming language)
Layer 4   Techuilaguy Compiler (frontend → IR → backend → self-hosting)
Layer 5   Techuilaguy OS (boot → scheduler → memory → syscalls → userspace → drivers)
Layer 6   Networking (Ethernet → IP → TCP/UDP → TLS/QUIC → simulator)
Layer 7   Distributed systems (RPC → Raft → replication → fault injection)
Layer 8   Storage & databases (block → FS → KV → WAL/MVCC → query → vector)
Layer 9   Security (capabilities, sandboxing, fuzzing, supply-chain, secure boot)
Layer 10  Cryptography & PQC (AEAD, PKI, ML-KEM, ML-DSA, SLH-DSA, hybrids)
Layer 11  Formal verification & reliability (invariants, model checking, chaos)
Layer 12  Cloud / edge / infrastructure (containers, orchestration, service mesh)
Layer 13  Developer ecosystem (OGGit, OGForge, OGJudge, package registry)
Layer 14  AI/ML (Tensor, Runtime, Agents, Vision)
Layer 15  Quantum computing (simulator, gates, error correction research)
Layer 16  Robotics & autonomy (control, SLAM, planning, swarm)
Layer 17  Graphics & simulation (rasterizer, renderer, engine)
Layer 18  Scientific computing (numerics, HPC, simulation, visualization)
Layer 19  Finance & data platform (ledgers, risk, integrated intelligence)
Layer 20  Space systems (orbital sim → digital twin → flight SW → HIL)
Layer 21  Quantum-secure VLEO research program (integration capstone)
```

Full architectural detail for every layer is in `ARCHITECTURE.md`. Ordered milestones are in `PLAN.md`.

---

## 6. Maturity model (mandatory labeling)

Every subsystem, in every document and README, must declare exactly one current maturity level. This is the single most important discipline in the project.

```
PLANNED       → intent recorded, nothing built
DESIGNED      → interfaces/spec written, reviewed
PROTOTYPE     → runs, incomplete, not fully tested
IMPLEMENTED   → feature-complete for its milestone scope
TESTED        → unit + failure tests pass, reproducibly
BENCHMARKED   → performance measured under recorded conditions
INTEGRATED    → wired to adjacent layers via stable interfaces
HARDWARE-IN-LOOP → validated against real/representative hardware
VALIDATED     → meets all acceptance criteria; limits documented
PRODUCTION-READY → hardened for real use (rare; most layers never need this)
```

Additional evidence tags used inline: `FACT`, `SOURCE-CLAIM`, `EXPERIMENTAL-RESULT`, `ENGINEERING-ASSUMPTION`, `RESEARCH-HYPOTHESIS`.

For space systems specifically: `SIMULATION` → `ENGINEERING MODEL` → `QUALIFICATION MODEL` → `FLIGHT MODEL`. A simulated spacecraft is never described as a launched one.

---

## 7. Functional requirements by layer (acceptance criteria)

Below, each layer states its **minimum "done for now" bar**. These are gates, not ceilings.

### 7.1 OGLang (Layer 3)
- **FR-LANG-1:** Lexer tokenizes the defined grammar; lexer unit tests pass.
- **FR-LANG-2:** Parser produces a correct AST for valid programs; invalid syntax yields a useful, deterministic diagnostic.
- **FR-LANG-3:** Static type checker rejects type errors with clear messages.
- **FR-LANG-4:** A documented memory-safety model (ownership/borrowing or an explicitly designed alternative) with tested invariants.
- **FR-LANG-5:** Systems features (pointers, structs, enums, arrays, modules, atomics, volatile, MMIO, inline-asm boundary) each have a concrete systems use case and tests.
- **Acceptance:** freestanding "hello world" and a non-trivial kernel utility compile and run.

### 7.2 Techuilaguy Compiler (Layer 4)
- **FR-COMP-1:** Frontend (lexer→parser→AST→semantic) with comprehensive unit + malformed-input tests.
- **FR-COMP-2:** Typed IR with basic blocks, CFG, constant propagation, dead-code elimination.
- **FR-COMP-3:** Liveness/interference analysis and working register allocation.
- **FR-COMP-4:** x86-64 (and i386 for bootstrap OS) backend producing linkable objects.
- **FR-COMP-5:** Toolchain integration (assembler, object format, linker, driver) + cross-compilation.
- **FR-COMP-6:** Compiler-quality suite: fuzzing, differential testing, golden tests, crash minimization.
- **Acceptance (self-hosting milestone, tracked explicitly):** a reproducible bootstrap in which the compiler builds its defined compiler target, and the result passes the compiler test suite.

### 7.3 Techuilaguy OS (Layer 5)
- **FR-OS-1:** Boots under QEMU via a documented boot protocol; serial output works.
- **FR-OS-2:** IDT, CPU exceptions, PIC, PIT, IRQ handling verified by tests.
- **FR-OS-3:** Scheduler with Ready/Running/Blocked/Dead states, preemption, context switching; deterministic scheduler tests pass across repeated runs with no context corruption.
- **FR-OS-4:** GDT/TSS, ring-3 userspace, privilege transitions, page protection.
- **FR-OS-5:** Physical frame allocator, paging, virtual memory, kernel heap, per-process address spaces.
- **FR-OS-6:** Syscall ABI: entry, dispatch, argument validation, user/kernel isolation.
- **FR-OS-7:** ELF loader, process creation, init, shell, basic utilities.
- **FR-OS-8:** VFS + a filesystem + block abstraction + file descriptors.
- **FR-OS-9:** Drivers: timer, serial, keyboard, then storage/network progressively.
- **FR-OS-MIGRATION:** A documented, staged migration path moving kernel subsystems from C/asm to OGLang (Stages A–I in `PLAN.md`).

### 7.4 Networking (Layer 6)
- **FR-NET-1:** Ethernet/ARP/IPv4/ICMP/UDP/TCP with packet-level and protocol tests, plus fuzzing and failure injection.
- **FR-NET-2:** TLS and QUIC integration points; HTTP client/server.
- **FR-NET-3:** An **original** gamified network simulator for education (inspired by the pedagogical role of tools like Packet Tracer; no proprietary code or branding copied).

### 7.5 Distributed systems (Layer 7)
- **FR-DIST-1:** RPC + membership + leader election + Raft with **deterministic simulation** and fault injection (partitions, node loss, message loss/reorder/duplication, recovery).

### 7.6 Storage & databases (Layer 8)
- **FR-STORE-1:** Block layer → filesystem → KV engine → WAL → transactions → MVCC → query engine, each with tests; vector search as research.

### 7.7 Security (Layer 9) & Cryptography/PQC (Layer 10)
- **FR-SEC-1:** Every security-sensitive component ships a threat model (assets, trust boundaries, attack surface, properties, assumptions, tests, failure modes, limits).
- **FR-CRYPTO-1:** Every crypto primitive has known-answer tests, negative tests, serialization tests, and fuzzing where appropriate.
- **FR-PQC-1:** ML-KEM, ML-DSA, SLH-DSA implemented/studied against their **actual standards**, with hybrid classical+PQC designs, crypto-agility, migration tooling, interoperability, and benchmarking. No absolute "quantum-proof" claims; assumptions and threat models documented.

### 7.8 Formal verification & reliability (Layer 11)
- **FR-VERIFY-1:** Property testing / model checking / (selective) theorem proving applied to data structures, scheduler invariants, and protocol invariants. Full-civilization verification is explicitly *not* a prerequisite for progress.

### 7.9 Developer ecosystem (Layer 13)
- **FR-DEV-1:** OGGit (content-addressed objects, commits, branches, merge, remote sync), OGForge (repos, issues, PRs, CI, registry, permissions), OGJudge (sandboxed deterministic evaluation).

### 7.10 AI/ML (Layer 14)
- **FR-AI-1:** Techuilaguy Tensor (ops, numerical kernels, autodiff research), Runtime (inference, batching, scheduling), Agents (tools, planning, retrieval, permissions, evaluation, observability, safety controls), Vision (image processing, detection, multimodal). AI security and safety treated as first-class.

### 7.11 Space systems (Layer 20) & VLEO program (Layer 21)
- **FR-SPACE-1:** Orbital math + two-body/perturbation/drag propagation, numerically validated (Monte Carlo where relevant).
- **FR-SPACE-2:** Spacecraft digital twin (mass, power, thermal, attitude, propulsion, comms, flight computer, payload).
- **FR-SPACE-3:** Flight-computer simulator + flight software (task scheduler, telemetry, telecommand, fault management, safe modes).
- **FR-SPACE-4:** Communications simulation with honest link budgets (no blanket "laser is X% faster than fiber" claims — propagation medium, path length, hardware, modulation, latency, throughput analyzed separately).
- **FR-SPACE-5:** Security integration: PQC-authenticated telemetry/telecommand + key management; QKD treated strictly as a key-distribution research layer with stated assumptions, **not** a replacement for authenticated cryptography.
- **FR-SPACE-6:** VLEO altitude treated as an **optimization problem** (drag, lifetime, propulsion, power, thermal, atomic oxygen, radiation, coverage, latency) — never assumed universally superior; conclusions come from simulation.
- **FR-SPACE-7 (legal gate):** No transmission, spectrum use, spacecraft operation, or launch without appropriate authorization (research current IN-SPACe / ISRO / DoS / ITU requirements before any real step).

---

## 8. Non-functional requirements

| Category | Requirement |
|---|---|
| **Reproducibility** | Every buildable subsystem documents exact build + test commands; builds are reproducible or the variance is controlled and documented. |
| **Testability** | Each subsystem has tests appropriate to its abstraction level, including deliberate failure tests. |
| **Documentation** | Each subsystem records architecture, assumptions, and limitations. Significant decisions become ADRs in `docs/ADR/`. |
| **Traceability** | Every important research claim cites an authoritative source in the docs. |
| **Honesty** | Maturity labels are mandatory and accurate. Benchmarks are never invented; they record hardware, config, workload, iterations, and results. |
| **Integrity of the record** | Git is the permanent engineering record: meaningful commits, no destruction of working systems, no committing generated binaries unless required. |
| **Security posture** | Banned phrases ("unhackable", "perfectly secure", "quantum-proof forever", "military grade") unless directly quoting and attributing a source. |

---

## 9. Success metrics

The project is succeeding when, at any point in time, an outside engineer can:

1. **Clone and build** the currently-active subsystems using only the documented commands.
2. **Run the tests** and see them pass — including the failure-condition tests.
3. **Read `PROJECT_STATE.md`** and know exactly what works, what's broken, and what's next.
4. **Trust every claim**, because each is either demonstrated or explicitly labeled a hypothesis.
5. **Trace the dependency chain** from the current frontier back to foundations and forward to integration.

Vanity metrics (line counts, number of subsystems "started", stars) are explicitly **not** success metrics. Verified depth is.

---

## 10. Release / maturity strategy

There is no single "1.0 launch." Instead, the project advances an **active frontier** along the dependency graph, promoting one subsystem at a time through the maturity model. Research branches (PQC, AI, space) may proceed in parallel *without pretending* their outputs are integrated into the core stack.

The long-horizon capstones — in rough order — are:
1. **Self-hosting compiler** (Layer 4 endgame).
2. **OGLang-majority OS kernel** (Layer 5 / migration Stage F+).
3. **Integrated core stack** (language → compiler → OS → network → storage → cloud → forge).
4. **Quantum-secure VLEO research architecture**, fully simulated and HIL-validated (Layer 21).

---

## 11. Risks & mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| **Scope collapse** ("this is too large") | Paralysis | Never build everything at once; maintain a dependency-aware active frontier; decompose relentlessly. |
| **Claim inflation** | Loss of credibility — the fatal risk | Mandatory maturity labels; no invented benchmarks; banned marketing phrases; evidence-or-hypothesis rule. |
| **Duplicate/competing implementations** | Fragmentation | One coherent system; experimental variants live in clearly marked directories. |
| **Destroying working systems during refactors** | Regression, lost trust | Tests before refactor; incremental migration stages; git as permanent record. |
