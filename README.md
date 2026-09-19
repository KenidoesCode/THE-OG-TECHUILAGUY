<div align="center">

# ⚡ THE OG TECHUILAGUY

### A Complete Computing Civilization — From First Principles to a Brighter Tomorrow

**One repository. One evolving ecosystem.**

[![Status](https://img.shields.io/badge/status-early%20development-orange?style=for-the-badge)](PROJECT_STATE.md)
[![License](https://img.shields.io/badge/license-MIT-blue?style=for-the-badge)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-783%20passing-brightgreen?style=for-the-badge)](tools/verify_all.sh)
[![Layers](https://img.shields.io/badge/verified%20layers-11-purple?style=for-the-badge)](PROJECT_STATE.md)

<br>

`MATHEMATICS`
→ `HARDWARE`
→ `LANGUAGES`
→ `COMPILERS`
→ `OS`
→ `NETWORKING`
→ `SECURITY`
→ `DISTRIBUTED SYSTEMS`
→ `STORAGE`
→ `AI`
→ `QUANTUM`
→ `SPACE`

</div>

---

## 🧭 What is THE OG TECHUILAGUY?

**THE OG TECHUILAGUY** is a single open-source repository exploring what it would take to build a complete computing ecosystem from first principles.

Instead of treating the modern stack as a collection of black boxes, the project works downward:

> **Understand the abstraction → rebuild it → test it → connect it → measure it → improve it.**

The long-term vision spans everything from mathematics and digital logic to operating systems, distributed infrastructure, cryptography, AI, quantum computing, robotics, scientific computing, and space systems.

This is not a collection of unrelated demos.

It is one evolving dependency graph.

```text
                    ┌─────────────────────────┐
                    │     Applications        │
                    │  AI • Robotics • Vision │
                    │ Graphics • Fintech • HPC│
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ Distributed Computing   │
                    │ Cloud • Storage • DB    │
                    │ Blockchain • Search     │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ Systems & Infrastructure │
                    │ Network • Security • OS │
                    │ Compiler • OGLang        │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ Computing Foundations   │
                    │ Hardware • ISA • Logic  │
                    │ Algorithms • Mathematics│
                    └─────────────────────────┘
```

---

# 🚀 Current State

This repository is **actively under development**.

The important distinction is:

> **Implemented means implemented. Tested means tested. Planned means planned.**

No subsystem is considered complete merely because a directory or API exists.

### Current verified snapshot

| Metric                                  |                Current state |
| ---------------------------------------- | ---------------------------: |
| Verified assertions                     |                      **783** |
| Test suites                             |                       **32** |
| PRD layers with verified implementation |                    **11 + blockchain** |
| Latest commit                           |                    `ce6a0d9` |
| Working tree                            |                    **Clean** |
| Main branch                             | **Synchronized with GitHub** |

Run everything yourself:

```bash
bash tools/verify_all.sh
```

The runner aggregates the real results from the project's implemented
test suites. It also preflight-checks that each suite's build
directory can actually accept new files, and reports a filesystem
write problem (a known WSL2 `/mnt/c` DrvFs quirk on Windows) as a
distinct environment error rather than a code failure — see
[`docs/VERIFICATION.md`](docs/VERIFICATION.md) for the supported
environments and how to read every failure mode the runner reports.

For the complete status of every layer and requirement:

→ **[PROJECT_STATE.md](PROJECT_STATE.md)**

---

# 🧠 The Core Philosophy

### 01 — First Principles

Understand what the abstraction actually does before rebuilding it.

### 02 — Real Implementations

Prefer working mechanisms over placeholders and impressive-looking interfaces.

### 03 — Verification

Every meaningful claim should have tests, measurements, or reproducible evidence.

### 04 — Honest Scope

A prototype is a prototype.

A simulator is a simulator.

A research direction is a research direction.

Nothing is called production-ready without the evidence to support it.

### 05 — Integration

Subsystems should become dependencies of one another rather than isolated projects.

### 06 — Open Source

The work, architecture, experiments, failures, and lessons should remain inspectable.

---

# 🔥 What Actually Exists?

## 🦾 OGLang + Compiler

A from-scratch systems-language experiment with its own compiler pipeline.

```text
Source
  ↓
Lexer
  ↓
Parser / AST
  ↓
Type Checker
  ↓
IR
  ↓
Liveness Analysis
  ↓
Interference Graph
  ↓
Register Allocation
  ↓
x86-64 Code Generation
  ↓
Assembler
  ↓
Linker
  ↓
Native ELF
```

Implemented and tested include:

* integer expressions
* comparisons
* `if` / `else`
* `while`
* mutable variables
* functions
* recursion
* multi-argument calls
* register + stack argument passing
* register allocation
* real stack spilling
* pointers
* fixed-size arrays
* runtime bounds checking
* `ptr` / `constptr`
* structs
* enums
* multi-file modules
* inline assembly boundary

The compiler currently targets a restricted x86-64 userspace environment.

**Important:** OGLang's eventual memory-safety goals are not yet fully implemented. Ownership, borrowing, use-after-return detection, atomics, volatile, MMIO, and other systems-language requirements remain future work.

→ [Memory model ADR](docs/ADR/0001-oglang-memory-model.md)

→ [Modules ADR](docs/ADR/0002-oglang-modules.md)

→ [Inline assembly ADR](docs/ADR/0003-oglang-inline-asm.md)

---

# 🖥️ Techuilaguy OS

A from-scratch 32-bit x86 freestanding operating-system prototype.

Current work includes:

* Multiboot boot
* physical page allocation
* IDT
* CPU exception handling
* PIC / IRQ handling
* PIT timer
* preemptive round-robin scheduling
* task lifecycle management
* GDT
* TSS
* ring-3 userspace
* syscall entry
* privilege enforcement
* paging
* per-process address spaces
* kernel heap
* ELF32 loading
* PS/2 keyboard input

The system is tested through both hosted tests and real QEMU boot scenarios.

→ [ELF loader ADR](docs/ADR/0004-elf-loader.md)

---

# 🌐 Networking

A standards-based protocol foundation currently covering:

* Ethernet
* ARP
* IPv4
* ICMP
* UDP
* RFC 1071 checksums

The current implementation is intentionally scoped below a full network stack.

Not yet implemented:

* NIC drivers
* TCP
* sockets API
* TLS
* QUIC
* HTTP

→ [Networking ADR](docs/ADR/0005-networking-protocol-layer.md)

---

# 🔐 Cryptography

Current cryptographic primitives include:

* SHA-256
* HMAC-SHA256

They are implemented from scratch and tested against standard known-answer vectors.

Future work includes:

* AEAD
* KDFs
* PKI
* protocol integration
* post-quantum cryptography
* hybrid cryptographic protocols
* crypto-agility

→ [Cryptography ADR](docs/ADR/0006-cryptography-hashing.md)

→ [HMAC ADR](docs/ADR/0011-hmac.md)

---

# ⚡ Distributed Systems

Current foundations include:

* RPC
* deterministic network simulation
* seeded fault injection
* message drop
* duplication
* reordering
* delay
* partitions and healing
* Raft leader election
* authenticated RPC envelopes

The distributed-systems work currently focuses on deterministic simulation and verification rather than pretending to be a production cluster.

→ [RPC ADR](docs/ADR/0008-distributed-rpc.md)

→ [Raft ADR](docs/ADR/0009-raft-leader-election.md)

→ [Authenticated RPC ADR](docs/ADR/0012-authenticated-rpc.md)

---

# 💾 Storage

Current storage foundations include:

* CRC-32 checked WAL
* durable key-value storage
* serialization
* crash/corruption testing

Future layers include:

* block devices
* filesystems
* transactions
* MVCC
* replication
* query processing
* vector storage

→ [Storage ADR](docs/ADR/0010-storage-wal.md)

---

# 🧬 Developer Infrastructure

### OGGit

A content-addressed Git-like storage foundation.

Current work includes:

* blobs
* trees
* commits
* SHA-256 object addressing
* refs
* `HEAD`
* first-parent history
* branch references
* symbolic and detached HEAD
* index / staging area (real hierarchical tree construction from staged files)
* checkout (materializing a tree back onto a real filesystem)
* diff (file-level comparison between any two trees)
* merge (full-ancestry-DAG merge-base discovery + three-way tree merge)

Future:

```text
OGGit
  ↓
OGForge
  ↓
OGJudge
  ↓
OGRegistry
  ↓
OGCI
```

→ [OGGit ADR](docs/ADR/0007-oggit-object-store.md)

→ [OGGit refs ADR](docs/ADR/0014-oggit-refs.md)

→ [OGGit index ADR](docs/ADR/0017-oggit-index.md)

→ [OGGit checkout ADR](docs/ADR/0018-oggit-checkout.md)

→ [OGGit diff ADR](docs/ADR/0019-oggit-diff.md)

→ [OGGit merge ADR](docs/ADR/0020-oggit-merge.md)

---

# 🏢 OGForge

A real server-logic foundation for a multi-repository developer
platform, built directly on OGGit — not a static frontend mock.

Current implementation:

* multiple named repositories, each a real OGGit repository on disk
* password-based authentication (explicitly not production-grade — see the ADR)
* authenticated repository creation, object push, and branch moves
* public repository browsing (list branches, read objects)
* real on-disk persistence surviving a process restart

Not yet implemented: any network transport (every operation today is
a direct in-process call), a real KDF-based credential store, private
repositories/permissions, issues, pull requests, code review, CI, and
a package registry.

→ [OGForge server foundation ADR](docs/ADR/0023-ogforge-server-foundation.md)

---

# 🧠 AI — Tensor + Autodiff Foundation

A real numeric `Tensor` plus a real, separate reverse-mode **scalar**
autodiff engine — used together to actually train a linear regression
model to convergence, not merely define the data structures.

Current implementation:

* `Tensor`: shape, flat storage, elementwise add/subtract/multiply, 2D matrix multiplication, shape-mismatch rejection (no broadcasting)
* `autodiff::Value`: a real reverse-mode scalar autodiff engine (+, −, ×, topological-sort-based `backward()`), cross-checked against independently-computed numerical (finite-difference) gradients
* a linear regression demo that trains on a fixed `y = 2x + 3` dataset and converges to `weight ≈ 2`, `bias ≈ 3` within a documented tolerance, then performs inference on an unseen input

**Important:** this is explicitly not Tensor-level autodiff, not a
neural network, and not a general-purpose ML framework — see the ADR
for exactly what is and isn't built yet.

→ [AI Tensor + autodiff foundation ADR](docs/ADR/0024-ai-tensor-autodiff-foundation.md)

---

# 🎨 Graphics — Deterministic Software Rasterizer

A real CPU software rendering pipeline — no GPU or window dependency,
a deliberate choice (this project's hosted verification runner has no
guaranteed GPU/display), not a fallback.

Current implementation:

* `Mat4` transforms (translation, scale, rotation, perspective projection, a real lookAt camera), operating on the scientific-computing layer's existing `Vec3` — a genuine cross-layer integration
* a real barycentric-coordinate triangle rasterizer with per-pixel depth testing (correct occlusion regardless of draw order, not painter's-algorithm layering)
* an end-to-end scene renderer: world-space triangles + a camera → framebuffer pixels, verified byte-for-byte deterministic across repeated renders

Not yet implemented: GPU execution, windowing/interactive input,
shaders/textures/lighting, anti-aliasing, a scene graph, asset loading,
or near/far/side-plane clipping.

Prepared as the rendering foundation future systems (NetLab's visual
topology editor, Space Systems visualization, a robotics simulator, a
game engine) can build on — none of that integration exists yet.

→ [Graphics software rasterizer foundation ADR](docs/ADR/0025-graphics-software-rasterizer-foundation.md)

## 🎮 ECS / Game Engine

A real Entity-Component-System, genuinely integrated with the
rasterizer above — its render system builds a scene from live ECS
state and calls the existing `renderScene` unmodified, not a second
rendering path.

Current implementation:

* generation-checked entity handles (a destroyed-and-recycled id's old handle correctly stays "not alive")
* per-type component storage: `Transform`, `Velocity`, `MeshComponent`, `CameraComponent`
* a movement system (deterministic position integration) and a render system (live ECS state → the real software rasterizer)

Not yet implemented: archetype/chunked storage, arbitrary 3D rotation,
a parent/child transform hierarchy, physics/collision, scripting,
serialization, or an interactive loop.

→ [ECS / game-engine foundation ADR](docs/ADR/0026-ecs-game-engine-foundation.md)

---

# 🦾 Robotics

Real forward kinematics for a 2-link planar arm and a real
proportional joint controller — a genuine closed-loop control
simulation, verified against an independent closed-form analytic
solution, not just "got closer to target."

Current implementation:

* standard closed-form 2-link planar forward kinematics, built on the scientific-computing layer's `Vec3`
* a proportional (P-only) joint controller
* a simulated control loop verified against the exact analytic `error(0)·(1-gain·dt)^N` solution

Not yet implemented: inverse kinematics, 3D/arbitrary-link kinematics,
dynamics, sensors/SLAM, PID, collision/path planning, or a
visualization integration with Graphics/ECS.

→ [Robotics planar-arm foundation ADR](docs/ADR/0027-robotics-planar-arm-foundation.md)

---

# 🧪 Verification

Verification is treated as a first-class layer rather than an afterthought.

Current work includes a seeded property-based testing framework applied across multiple subsystems.

Examples include:

* network serialization
* HMAC behavior
* WAL exactness
* ELF parsing robustness
* network parser robustness

Future work includes:

* shrinking
* model checking
* coverage-guided fuzzing
* stronger formal methods
* proof-oriented verification

→ [Verification ADR](docs/ADR/0013-property-testing.md)

---

# ⚛️ Quantum

A classical state-vector simulator currently supports:

* X
* Z
* H
* CNOT
* measurement
* Bell states
* GHZ states
* amplitude verification
* correlation verification

The simulator is intentionally a classical research tool.

Not yet implemented:

* quantum algorithms library
* noise models
* error correction
* hardware integration

→ [Quantum ADR](docs/ADR/0015-quantum-simulator.md)

---

# 🧮 Scientific Computing

Layer 18 currently provides the numerical foundation used by the space-systems work:

* `Vec3`
* vector arithmetic
* dot products
* cross products
* norms
* normalization
* classical RK4 integration

The numerical methods are tested against analytical solutions.

---

# 🛰️ Space Systems

Layer 20 currently begins with a real dependency on Layer 18.

```text
Scientific Computing
        │
        ├── Vec3
        └── RK4
             │
             ▼
      Two-Body Dynamics
             │
             ▼
      Orbital Propagation
```

Current implementation:

* Newtonian two-body gravity
* Cartesian position/velocity state
* Keplerian circular-orbit period
* specific orbital energy
* numerical orbital propagation

The orbital tests caught and fixed a real numerical integration bug involving truncated final time steps.

Not yet implemented:

* J2 perturbations
* atmospheric drag
* third-body effects
* thrust
* spacecraft model
* ADCS
* flight software
* communications
* telemetry
* telecommand
* digital twin
* mission simulator

→ [Space Systems README](20-space-systems/README.md)

→ [Scientific Computing + Orbital Mechanics ADR](docs/ADR/0016-scientific-computing-linalg.md)

---

# ⛓️ Techuilaguy Blockchain (L1)

A new domain, added directly per project direction rather than
appearing in `PRD.md`'s original layer numbering — tracked honestly as
such in [`PROJECT_STATE.md`](PROJECT_STATE.md) rather than folded into
an unrelated existing layer.

Current implementation is a real, single-node, persistent chain:

* an account model (balance + nonce)
* canonical, deterministic transaction serialization and hashing
* a real state-transition function (nonce-based replay/double-spend defense, balance checks)
* a block structure that chains to the real previous block's hash
* real on-disk persistence, built directly on the existing storage layer's WAL-backed KV store — a genuine cross-layer integration, not a new bespoke format

**Important:** transactions are not cryptographically signed yet —
there is no asymmetric-key/identity integration, so `Chain`/`Ledger`
never verify who is authorized to spend from an address. `stateRoot`/
`txRoot` are whole-content hashes, not Merkle roots. There is no P2P,
no consensus, no multiple nodes, no VM, and no smart contracts.

→ [Techuilaguy Blockchain L1 ADR](docs/ADR/0021-techuilaguy-blockchain-l1.md)

---

# 🌐 Techuilaguy NetLab

An original (not Cisco-derived) educational network simulation
engine — now a first-class product frontier, not an isolated
subsystem. The first complete learning experience exists: a student
can build `PC1 → Switch → Router → Switch → PC2`, configure real IPv4
addresses, and simulate a real ping that visibly moves through
Ethernet → ARP → IPv4 → routing → Ethernet → destination, with every
step recorded in an inspectable packet timeline.

Current implementation:

* a topology graph of Host, Switch, and Router nodes connected by links
* real simulated Ethernet frames, built directly on `06-networking`'s actual wire-format codec
* genuine (simplified) L2 switch behavior: MAC learning + flooding, not a hardcoded routing table
* a router modeled as a real L2 broadcast-domain boundary, performing real single-hop L3 forwarding between its interfaces' subnets
* real ARP resolution and ICMP echo (ping) simulation, using `06-networking`'s existing ARP/IPv4/ICMP codecs — no protocol reimplementation
* an inspectable, byte-for-byte-deterministic packet timeline (every ARP/IP event, with the real frame bytes attached)
* the first gamified mission, "Connect Two Networks" — a real objective/validation/failure/completion evaluator, not a static pass/fail stub
* a simulation core with no OS/UI dependency, designed (not yet built) to eventually compile to WebAssembly for a browser front end
* a packet inspector that decodes a real captured frame's actual header fields (Ethernet/ARP/IPv4/ICMP) into human-readable summaries, using only `06-networking`'s existing parse functions
* a timeline session supporting step forward, step backward (rewind), jump-to, and reset over an already-computed simulation timeline — the UI-independent core a future timeline UI would drive (Phase 1 of the NetLab product build)

Not yet implemented: multi-hop routing, DHCP/DNS/TCP/UDP, a
persistent ARP cache, latency/loss/bandwidth simulation, a visual
topology editor, an actual browser/WebAssembly build, additional
missions, and grading/XP/skill-tree/classroom mode.

→ [Techuilaguy NetLab foundation ADR](docs/ADR/0022-techuilaguy-netlab-foundation.md)

→ [NetLab IPv4/ARP/routing + first mission ADR](docs/ADR/0028-netlab-ip-arp-routing-mission.md)

→ [NetLab packet inspector + timeline session ADR](docs/ADR/0029-netlab-packet-inspector-and-timeline-session.md)

---

# 🗺️ The Civilization Roadmap

The long-term architecture spans:

```text
01  Foundations
02  Hardware
03  Compiler
04  Operating System
05  Networking
06  Cryptography
07  Cybersecurity
08  Distributed Systems
09  Blockchain
10  Storage & Databases
11  Verification
12  Cloud Infrastructure
13  Developer Ecosystem
14  AI / ML
15  Quantum
16  Robotics
17  Graphics / Simulation
18  Scientific Computing
19  Finance / Data
20  Space Systems
21  Quantum-Secure VLEO Research
22  Applications / Ecosystem
```

The repository does **not** claim that all of these layers are complete.

The dependency graph advances as real implementations become possible.

---

# 📊 Verified Progress

The current universal verification runner covers implemented subsystems across the active layers.

```text
632 assertions
25 test suites
0 failures
```

The current verified suites include:

```text
OGLang unit tests
OGLang end-to-end tests
OS kernel heap
OS ELF loader
OS keyboard translation
Network protocol codecs
RPC + fault injection
Raft leader election
Authenticated RPC envelopes
WAL + KV store
SHA-256
HMAC-SHA256
Property-based testing
OGGit object store
OGGit refs / HEAD / history
OGGit index / staging
OGGit checkout
OGGit diff
OGGit merge
OGForge server foundation
Quantum state-vector simulator
Vec3 + RK4 numerics
Two-body orbital propagation
L1 accounts/transactions/blocks/persistence
NetLab topology + L2 switch simulation
NetLab IPv4/ARP/routing + first mission
Tensor + autodiff + linear regression training
Software rasterizer (Mat4/FrameBuffer/rasterizer/scene)
ECS / game-engine foundation (integrated with the rasterizer)
Planar-arm forward kinematics + proportional control
```

Run them:

```bash
bash tools/verify_all.sh
```

---

# 🏗️ Repository Structure

```text
THE-OG-TECHUILAGUY/
│
├── 01-foundations/
│   └── machine/
│
├── 03-compiler/
│   └── oglang/
│
├── 06-networking/
│
├── 07-distributed-systems/
│
├── 08-storage/
│
├── 10-cryptography/
│
├── 11-verification/
│
├── 13-developer-ecosystem/
│   ├── oggit/
│   └── forge/
│
├── 14-ai/               Tensor + scalar autodiff + linear regression training
│
├── 15-quantum/
│
├── 16-robotics/          Planar-arm forward kinematics + proportional control
│
├── 17-graphics/          Deterministic software rasterizer + ECS/game-engine foundation
│
├── 18-scientific-computing/
│
├── 20-space-systems/
│
├── 22-os/
│
├── 23-blockchain/       Techuilaguy L1 (accounts, transactions, blocks, persistence)
│
├── 24-network-simulator/  Techuilaguy NetLab (topology + L2 switch simulation engine)
│
├── docs/
│   ├── ADR/
│   └── VERIFICATION.md
│
├── tools/
│   └── verify_all.sh
│
├── PRD.md
├── PROJECT_STATE.md
├── LICENSE
└── README.md
```

Each numbered directory represents a layer of the larger computing ecosystem.

---

# 📚 Architecture Decisions

Important design decisions are documented as ADRs.

```text
docs/ADR/
├── 0001  OGLang memory model
├── 0002  OGLang modules
├── 0003  Inline assembly
├── 0004  ELF loader
├── 0005  Networking protocol layer
├── 0006  Cryptographic hashing
├── 0007  OGGit object store
├── 0008  Distributed RPC
├── 0009  Raft leader election
├── 0010  Storage WAL
├── 0011  HMAC-SHA256
├── 0012  Authenticated RPC
├── 0013  Property-based testing
├── 0014  OGGit refs
├── 0015  Quantum simulator
├── 0016  Scientific computing + orbital mechanics
├── 0017  OGGit index (staging area)
├── 0018  OGGit checkout (tree materialization)
├── 0019  OGGit diff (tree comparison)
├── 0020  OGGit merge (ancestry DAG + three-way merge)
├── 0021  Techuilaguy Blockchain L1
├── 0022  Techuilaguy NetLab foundation
├── 0023  OGForge server foundation
├── 0024  AI Tensor + autodiff foundation
├── 0025  Graphics software rasterizer foundation
├── 0026  ECS / game-engine foundation
├── 0027  Robotics planar-arm foundation
├── 0028  NetLab IPv4/ARP/routing + first mission
└── 0029  NetLab packet inspector + timeline session
NetLab packet inspector
NetLab timeline session (step/rewind/replay)
```

The ADRs are the detailed technical record.

The README is the map.

---

# 🧭 Project State

For the most accurate view of what is:

* planned
* designed
* prototyped
* implemented
* tested
* benchmarked
* integrated
* hardware-tested
* validated

see:

### → [PROJECT_STATE.md](PROJECT_STATE.md)

That document is intentionally more detailed than this README.

---

# 🤝 Contributing

Contributions are welcome across:

* code
* tests
* documentation
* benchmarks
* research
* security reviews
* architecture
* mathematical verification
* hardware experiments

The core rule is simple:

> **If you claim it works, show the evidence.**

Good contributions should include the implementation, tests, documentation, and scope of what remains.

---

# ⭐ The Long-Term Vision

THE OG TECHUILAGUY is ultimately an experiment in building a computing civilization from the bottom up.

Not just:

```text
"build an app"
```

but:

```text
Mathematics
    ↓
Algorithms
    ↓
Logic
    ↓
Hardware
    ↓
ISA
    ↓
Language
    ↓
Compiler
    ↓
Operating System
    ↓
Networking
    ↓
Distributed Systems
    ↓
Storage
    ↓
Security
    ↓
AI / Quantum / Robotics
    ↓
Scientific Computing
    ↓
Space Systems
    ↓
A larger open computing ecosystem
```

The destination is deliberately ambitious.

The work is deliberately incremental.

Every layer must earn its place.

---

<div align="center">

## UNDERSTAND → BUILD → CONNECT → VERIFY → MEASURE → IMPROVE → SHARE

### From first principles to a brighter tomorrow.

⭐ **Star the repository if you want to follow the build.**

</div>
