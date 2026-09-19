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

**Reproducing this:** `bash tools/verify_all.sh` builds and runs every
hosted suite referenced below and prints the real, aggregated
pass/fail counts — reproduced most recently from a clean clone of the
current commit (30 suites, 738 assertions, 0 failures). See
[`docs/VERIFICATION.md`](docs/VERIFICATION.md) for supported
environments, how the runner classifies failures (code/test failure
vs. build/toolchain failure vs. environment error), and the QEMU-
dependent tests it deliberately does not run.

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
| **Enum types**: `enum Name { Variant, ... }` as named `i32` constants (not a distinct nominal type — no storage, no exhaustiveness checking), `Name.Variant` reusing struct field-access dot syntax, resolved to a compile-time ordinal | TESTED | `tests/programs/enum_variants.og`; `tests/unit_tests.cpp` — parsing, unknown-variant/duplicate-variant/duplicate-enum-name rejection, struct-variable-vs-enum-type dot-access priority, well-typed round trip; codegen fingerprint confirms a variant access is a plain immediate constant with no address arithmetic |
| **Runtime array bounds checking**: out-of-range/negative index traps (exit 101) | TESTED | `tests/programs/array_out_of_bounds.og`, `array_negative_index.og` |
| Memory-safety model | DESIGNED | [`docs/ADR/0001-oglang-memory-model.md`](docs/ADR/0001-oglang-memory-model.md) — explicitly documents the current model as raw/unsafe (C-like), records which mechanisms are tested (pointer aliasing, spilled-pointer correctness, array read/write, bounds checking, const/mut pointers) versus which safety properties are *not* enforced (no use-after-return detection, no aliasing discipline beyond const/mut, no borrow checking), and records candidate next steps. Full borrow-checking remains PLANNED, not started. |
| **Modules (v1)**: one file = one module, `import other;`, qualified-only access (`other.symbol`), cross-module function/struct/enum use, cycle rejection, deterministic compile order | TESTED | `tests/programs/modules/` (cross-module call+struct+enum in one linked binary; unknown-module/unknown-symbol/cycle negative cases); `tests/unit_tests.cpp` — same-unqualified-name-in-two-modules non-ambiguity, unqualified-access-to-import rejected, entry-vs-library `main` requirement, single-file `import` rejection; see [`docs/ADR/0002-oglang-modules.md`](docs/ADR/0002-oglang-modules.md) for exact scope (no separate objects/incremental compilation, no transitive re-export, no selective imports, no visibility control, no aliasing) |
| **Inline-assembly boundary (v1)**: `asm("template")`, verbatim emission, fixed-register (`%eax`) result, live values saved/restored around it (reuses `Call`'s mechanism) | TESTED | `tests/programs/inline_asm.og`, `inline_asm_register_pressure.og` (real linked binaries, correct exit codes under light and heavy register pressure); `tests/unit_tests.cpp` — string-literal lexing incl. unterminated-literal rejection, parsing, i32 typing, codegen fingerprint for verbatim emission + save/restore; see [`docs/ADR/0003-oglang-inline-asm.md`](docs/ADR/0003-oglang-inline-asm.md) for exact scope (no operand binding, one output only) |
| Atomics, volatile, MMIO | PLANNED | not started — deliberately: the compiler performs no instruction reordering/elimination of any kind today, so `volatile`/atomics would be vacuous syntax with no backing guarantee until the optimizer (or a concurrency model) exists to make them meaningful; see ADR 0003 |
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
| **Kernel heap** (`kmalloc`/`kfree`, first-fit free-list, real block splitting + coalescing, on top of the physical page allocator) | TESTED | `tests/heap_test.sh`/`heap_test.cpp` (16 hosted unit assertions against a fake page allocator backed by real host memory — the real one hands back physical addresses only valid inside the kernel's own address space); `tests/boot_test.sh` — a real boot-time self-test (alloc two blocks, write through both, confirm no overlap, free one, confirm reuse) against the actual physical allocator. v1 limit: each heap segment is exactly one physical page, so a single allocation cannot exceed one page minus header overhead (segments never coalesce across each other, since two physical pages aren't guaranteed contiguous) |
| **Real ELF32/i386 loader** (`elf/`, `scheduler_create_elf_user_task`): header/program-header validation with overflow-safe bounds checks, `PT_LOAD` mapping with real per-segment W permission (`paging_map_user_page` gained a `writable` param), BSS zero-init, entry-point-in-executable-segment enforcement, full rollback on any failure | TESTED | `tests/elf_test.sh`/`elf_test.cpp` — 22 hosted unit assertions (3 positive: minimal image, multi-segment, BSS; 19 negative: bad magic/class/endianness/machine/type, truncated header, phdr-table/segment-offset out-of-bounds incl. integer-overflow variants, memsz<filesz, unaligned/out-of-window vaddr incl. overflow, segment overlap, too-many-segments/pages, invalid entry point in any/non-executable segment, non-LOAD segments ignored); `tests/boot_test.sh` — two real ELF binaries (`userland/elf_hello.S`, `elf_write_to_code.S`, built with explicit `PHDRS` linker scripts, embedded unflattened) booted for real: one proves a real, distinct, writable `.data` segment (read-write-read-back round trip); the other proves a code segment is genuinely non-writable at the hardware level (a write into it faults #PF, task killed in isolation, code after it never runs). v1 scope: ELF32/`ET_EXEC`/`EM_386` only, no dynamic linking/relocations/PIE, max 4 `PT_LOAD` segments / 8 physical pages total, no NX (architectural — 32-bit non-PAE paging has no execute-disable bit), no `exec()`/process replacement; see [`docs/ADR/0004-elf-loader.md`](docs/ADR/0004-elf-loader.md) |
| VFS, security/capability subsystems | PROTOTYPE | `vfs_init()`/`security_init()` exist and print but implement no actual filesystem or capability enforcement yet |
| Real process creation from disk (`exec()`), init, shell, utilities | PLANNED | the ELF loader itself is TESTED (see above), but every ELF image today is still embedded into the kernel image at build time, not loaded from any filesystem — there is no filesystem yet, and no `exec()` syscall for an existing task to load and replace itself |
| Filesystem, block device abstraction, file descriptors | PLANNED | not started |
| Drivers beyond timer/PIC/keyboard | PLANNED | not started |
| Documented OGLang kernel-migration stages | PLANNED | not started (OGLang cannot yet target freestanding code at all — see Layer 3) |

**Test suite:** `tests/boot_test.sh` (24 assertions against real QEMU
boot behavior), `tests/keyboard_test.sh` (real injected PS/2 input),
`tests/keyboard_translation_test.sh` (12 hosted unit assertions).

---

## Layer 6 (Networking)

| Requirement | Status | Evidence |
|---|---|---|
| **Protocol codec layer**: Ethernet, ARP, IPv4, ICMP, UDP — real parsing/serialization/checksums against the actual RFCs (826/791/792/768) | TESTED | `06-networking/protocols/`; `tests/protocols_test.sh` — 40 hosted unit assertions (round-trip + every realistic rejection case per protocol: truncated buffers, wrong ARP hardware type/address lengths/opcode, wrong IPv4 version/IHL/totalLength, corrupted checksums, UDP pseudo-header participation, RFC 768's zero-checksum rule). Pure arithmetic over a byte buffer, no hardware/OS dependency; see [`docs/ADR/0005-networking-protocol-layer.md`](docs/ADR/0005-networking-protocol-layer.md) |
| NIC driver, TCP, sockets API, routing, ARP cache | PLANNED | no network interface card driver exists anywhere in the repository; TCP's stateful protocol is deliberately deferred rather than attempted partially |
| TLS/QUIC/HTTP (FR-NET-2) | PLANNED | depends on TCP, which doesn't exist yet |
| Gamified network simulator (FR-NET-3) — see Layer 24 below | FOUNDATION | the simulation-engine foundation (topology, real Ethernet frames, L2 switch learning) now exists independently of TCP — see Layer 24 |

## Layer 10 (Cryptography & PQC)

| Requirement | Status | Evidence |
|---|---|---|
| **SHA-256** (FIPS 180-4), from-scratch, streaming API | TESTED | `10-cryptography/hashing/`; `tests/sha256_test.sh` — 7 hosted assertions against the standard's own published known-answer test vectors (empty string, "abc", a two-block 56-byte message, one million repeated 'a's) plus incremental-vs-one-shot equivalence and reset() correctness. See [`docs/ADR/0006-cryptography-hashing.md`](docs/ADR/0006-cryptography-hashing.md) |
| **HMAC-SHA256** (RFC 2104), built on the SHA-256 above, plus constant-time MAC comparison | TESTED | `10-cryptography/mac/`; `tests/hmac_test.sh` — 6 hosted assertions against RFC 4231's own published test vectors (incl. the key-longer-than-block-size branch) plus different-key/different-MAC and constant-time-comparison correctness. See [`docs/ADR/0011-hmac.md`](docs/ADR/0011-hmac.md) |
| AEAD, PKI, key exchange, a KDF, ML-KEM, ML-DSA, SLH-DSA, hybrid PQC | PLANNED | not started |

## Layer 13 (Developer Ecosystem)

| Requirement | Status | Evidence |
|---|---|---|
| **OGGit content-addressed object store** (blob/tree/commit, SHA-256-based, real disk I/O) | TESTED | `13-developer-ecosystem/oggit/`; `tests/oggit_object_store_test.sh` — 24 hosted assertions (round trips, content-addressing determinism, corrupted-object detection via re-hash on read, canonical tree ordering, commit parent-count variations, full commit→tree→blob reconstruction). Genuine integration with Layer 10's SHA-256, not a reimplementation. See [`docs/ADR/0007-oggit-object-store.md`](docs/ADR/0007-oggit-object-store.md) |
| **OGGit refs, HEAD, first-parent history** (branches, symbolic/detached HEAD, ancestry walk) | TESTED | `13-developer-ecosystem/oggit/refs.*`; `tests/oggit_refs_test.sh` — 18 hosted assertions incl. a branch moving while HEAD symbolically follows it, clean failure on an empty repo, and first-parent history on a real merge commit correctly excluding the other parent's chain. See [`docs/ADR/0014-oggit-refs.md`](docs/ADR/0014-oggit-refs.md) |
| **OGGit index/staging area** (path→blob map, disk-persisted, real hierarchical tree construction from staged paths) | TESTED | `13-developer-ecosystem/oggit/index.*`; `tests/oggit_index_test.sh` — 31 hosted assertions incl. stage/retrieve/replace/remove, path normalization, save/reload round trip, an empty index producing a real empty tree, a genuinely nested multi-level directory hierarchy independently re-read two levels deep, staging-order-independence of the resulting tree id, and a documented deterministic resolution for a file/directory path conflict. See [`docs/ADR/0017-oggit-index.md`](docs/ADR/0017-oggit-index.md) |
| **OGGit checkout** (real Tree → filesystem materialization, the mirror of the index) | TESTED | `13-developer-ecosystem/oggit/checkout.*`; `tests/oggit_checkout_test.sh` — 23 hosted assertions incl. a full stage→tree→checkout→re-read round trip preserving binary content byte-for-byte, nested-hierarchy materialization, overwrite-vs-leave-untracked-alone behavior, and clean failure (not a crash) on an unknown tree id, a non-Tree object, and a tree referencing a never-written blob. See [`docs/ADR/0018-oggit-checkout.md`](docs/ADR/0018-oggit-checkout.md) |
| **OGGit diff** (file-level comparison of two Tree objects) | TESTED | `13-developer-ecosystem/oggit/diff.*`; `tests/oggit_diff_test.sh` — 20 hosted assertions incl. correct Added/Removed/Modified detection with real blob ids, multi-depth nested changes, a whole added directory expanding into one entry per contained file, a documented type-change (file↔directory) resolution, sorted deterministic output, and clean (non-crashing) handling of unreadable tree ids. See [`docs/ADR/0019-oggit-diff.md`](docs/ADR/0019-oggit-diff.md) |
| **OGGit merge** (full-ancestry-DAG merge-base discovery + three-way tree merge) | TESTED | `13-developer-ecosystem/oggit/merge.*`; `tests/oggit_merge_test.sh` — 36 hosted assertions incl. fast-forward, already-up-to-date, a genuine common-ancestor discovery via real multi-parent BFS (not first-parent-only), clean merges across nested directories, ModifyModify/AddAdd/ModifyDelete/FileDirectory conflict classification with correct populated/zero ids, a criss-cross topology with two genuine lowest common ancestors resolved deterministically, and clean (non-crashing) handling of a wrong-type object, a never-written object, malformed commit content, and a corrupt ancestor mid-walk. Deliberately does not write a Commit or touch RefStore — see [`docs/ADR/0020-oggit-merge.md`](docs/ADR/0020-oggit-merge.md) for the full design and explicit non-goals (no rename detection, no recursive/virtual merge-base strategy, no unrelated-histories support) |
| **OGForge server foundation** (multi-repository host built on real OGGit, password-based auth, push, branch moves, public browsing, real persistence) | TESTED | `13-developer-ecosystem/forge/forge_server.*`; `tests/forge_server_test.sh` — 24 hosted assertions incl. auth-gated repository creation/push/branch-move, RepositoryAlreadyExists/RepositoryNotFound/InvalidRequest/ObjectNotFound classification, a branch move rejected when its target commit was never pushed, a corrupted on-disk object reported (not silently trusted) via OGGit's existing re-hash-on-read check, and a genuine process-restart test recovering users/repositories/objects/branches. See [`docs/ADR/0023-ogforge-server-foundation.md`](docs/ADR/0023-ogforge-server-foundation.md) for explicit non-goals (no network transport yet, unsalted non-constant-time password hashing — not production auth, no issues/PRs/CI/registry) |
| OGGit remote sync, OGForge network transport/issues/PRs/CI/registry, OGRegistry, OGJudge | PLANNED | no remote transport exists (OGForge's server logic is in-process only so far); OGRegistry/OGJudge not started |

## Layer 7 (Distributed Systems)

| Requirement | Status | Evidence |
|---|---|---|
| **RPC + deterministic network simulation with fault injection** (drop/duplicate/reorder/delay, hard partitions with heal) | TESTED | `07-distributed-systems/rpc/`; `tests/rpc_test.sh` — 35 hosted assertions (serialization + RPC framing round trips and malformed-input rejection, full call success, unknown-method error handling, drop-induced timeout, partition block+heal, duplication/reordering robustness, and a genuine determinism proof: identical seed → identical 8-call outcome sequence, different seed → different sequence). See [`docs/ADR/0008-distributed-rpc.md`](docs/ADR/0008-distributed-rpc.md) |
| **Raft leader election** (randomized timeouts, RequestVote/Heartbeat RPCs, term-based safety) | TESTED | `07-distributed-systems/raft/`; `tests/raft_test.sh` — 10 hosted assertions incl. a safety check at every tick of a 300-tick run (never two simultaneous leaders per term), leader-failure→new-election with strictly higher term, determinism, and liveness under 20% message loss. A real addressing bug (a node's own server-pump logic drained and discarded its own pending call's responses) was caught and fixed during development — see [`docs/ADR/0009-raft-leader-election.md`](docs/ADR/0009-raft-leader-election.md) |
| **Authenticated RPC envelopes** (HMAC-SHA256 tag over message bytes, cross-layer integration with Layer 10) | TESTED | `07-distributed-systems/rpc/auth.*`; `tests/auth_test.sh` — 10 hosted assertions (round trip, wrong-key/tampered-message/tampered-tag/truncated-input rejection, full RpcRequest pipeline, forged-message rejection). Not wired into RpcClient/RpcServer's default path (no key distribution yet) — see [`docs/ADR/0012-authenticated-rpc.md`](docs/ADR/0012-authenticated-rpc.md) |
| Real network transport, membership, Raft log replication/committed entries, replicated state machines, key exchange/distribution | PLANNED | RPC/Raft run entirely in-process/in-memory; no socket transport, no membership protocol, no log replication, no key-exchange mechanism exists yet |

## Layer 8 (Storage)

| Requirement | Status | Evidence |
|---|---|---|
| **Write-ahead log** (CRC-32-checked, crash-tested recovery) + **KV store** built on it | TESTED | `08-storage/wal/`, `kv/`; `tests/storage_test.sh` — 23 hosted assertions incl. genuine crash simulation (a torn trailing record and a bit-flipped checksum, both produced by directly modifying on-disk bytes) and full-restart/repeated-restart recovery correctness. Record encoding reuses Layer 7's serialization codec directly. See [`docs/ADR/0010-storage-wal.md`](docs/ADR/0010-storage-wal.md) |
| Block-device abstraction, compaction, transactions/MVCC, indexes/query engine, replication | PLANNED | operates directly on a host file via fstream, not a block-level interface; no transaction/concurrency-control/replication layer exists yet |

## Layer 11 (Formal Verification & Reliability)

| Requirement | Status | Evidence |
|---|---|---|
| **Property-based testing framework**, applied to 5 real subsystems across 4 layers + the OS | TESTED | `11-verification/`; `tests/property_tests.sh` — seeded/reproducible generation (`Random`/`forAll`), applied to: RPC serialization round-trip (Layer 7), HMAC determinism (Layer 10), WAL append/recover exactness against real disk I/O (Layer 8), and fuzz-style crash-robustness of the ELF loader and every network parser (Layer 6) across thousands of random buffers each — all passed on first run. See [`docs/ADR/0013-property-testing.md`](docs/ADR/0013-property-testing.md) |
| Shrinking, model checking, symbolic execution, coverage-guided fuzzing, chaos testing infrastructure | PLANNED | not started (Raft's own deterministic-simulation fault injection in Layer 7 is the only chaos-testing-adjacent infrastructure that exists) |

## Layer 15 (Quantum)

| Requirement | Status | Evidence |
|---|---|---|
| **Classical state-vector simulator** (X/Z/H/CNOT gates, measurement, Bell/GHZ correctness) | TESTED | `15-quantum/simulator/`; `tests/qsim_test.sh` — 17 hosted assertions: exact amplitude checks (X/Z/H/CNOT/Bell state), measurement-correlation over 1000 (Bell) and 500 (GHZ) trials, collapse verification, and a property test (real integration with Layer 11) confirming total probability stays 1.0 across 300 random gate sequences. See [`docs/ADR/0015-quantum-simulator.md`](docs/ADR/0015-quantum-simulator.md) |
| Algorithm library (Deutsch-Jozsa/Grover/QFT/Shor prototype), noise models, error correction, circuit representation | PLANNED | not started; Bell/GHZ states are constructed directly in tests, not as reusable named circuits |

## Layer 18 (Scientific Computing) + Layer 20 (Space Systems)

Built together as a deliberate dependency chain (Layer 18's numerics
used directly by Layer 20's orbital mechanics), per
[`docs/ADR/0016-scientific-computing-linalg.md`](docs/ADR/0016-scientific-computing-linalg.md).

| Requirement | Status | Evidence |
|---|---|---|
| **Vec3 + RK4 ODE integrator** (Layer 18) | TESTED | `18-scientific-computing/`; `tests/sci_test.sh` — 9 hosted assertions against known-exact solutions (exponential decay, harmonic oscillator) and hand-computed vector algebra |
| **Two-body orbital propagation** (Layer 20), built directly on Layer 18's RK4 | TESTED | `20-space-systems/orbital/`; `tests/orbital_test.sh` — 7 hosted assertions incl. matching the real geostationary period, circular-orbit position/velocity/radius conservation over a full period, elliptical-orbit energy conservation, and step-size accuracy scaling. A real step-count truncation bug (losing up to one full step's worth of orbital motion) was caught and fixed during development |
| Perturbations, drag, spacecraft model, flight software, ADCS/EPS/thermal, telemetry/telecommand, digital twin, mission simulator, HIL | PLANNED | not started; general linear algebra (matrices/solvers) and adaptive-step ODE methods also not started in Layer 18 |

## Layer 23 (Techuilaguy Blockchain — added per project direction, NOT in PRD.md's original layer numbering)

**Status: FOUNDATION.** A single-node account/transaction/block chain
with real persistence — the smallest real vertical slice, explicitly
not multi-node, not consensus, not a VM, and not signed. This layer
does not exist in `PRD.md`'s Layer 0-21 numbering (that document
predates this addition); it is tracked here honestly as a distinct
addition rather than folded into an unrelated existing layer number.

| Requirement | Status | Evidence |
|---|---|---|
| **Account model, canonical transaction serialization, transaction hashing, deterministic state transition, block structure, single-node chain, real persistence** | TESTED | `23-blockchain/l1/`; `tests/l1_test.sh` — 41 hosted assertions incl. deterministic address derivation and transaction serialization, malformed-transaction-byte rejection (empty/garbage/truncated/trailing-garbage), the full state-transition rule set (insufficient balance, invalid nonce, successful apply with correct debit/credit/nonce-advance), a direct double-spend/replay rejection via the nonce check, genesis block correctness (zero previousHash, no transactions), real block-to-block hash chaining across multiple produced blocks, block header serialization round-trip and malformed-input rejection, and a genuine process-restart simulation (a second `Chain` instance over the same on-disk path recovering the identical height and historical block). Built on real cross-layer integration: SHA-256 (Layer 10), `dist::Encoder`/`Decoder` (Layer 7), and `storage::KVStore` (Layer 8) for persistence — not bespoke reimplementations. See [`docs/ADR/0021-techuilaguy-blockchain-l1.md`](docs/ADR/0021-techuilaguy-blockchain-l1.md) for the full design and extensive explicit non-goals (no signatures, no Merkle trees — whole-state/whole-tx hashes only, no P2P/consensus/multi-node, no mempool fee market, no chain reorg, no VM/contracts/L2) |
| Signatures/identity, Merkle structures, mempool economics, P2P networking, consensus, multi-node simulation, chain reorganization, VM, smart contracts, L2 | PLANNED | none of these exist yet; this slice is deliberately a single-node, unsigned, whole-state-hash chain only |

## Layer 24 (Techuilaguy NetLab — network simulator, fulfilling PRD Layer 6's FR-NET-3)

**Status: FOUNDATION.** A real (simplified) L2 network simulation
engine — topology graph, real Ethernet frames, and genuine
learning-switch behavior — with no IP/ARP/DHCP/DNS protocol behavior,
no visual editor, and no missions/grading/XP yet.

| Requirement | Status | Evidence |
|---|---|---|
| **Simulation engine: Node/Link topology graph, real Ethernet frames, L2 switch MAC learning + flooding** | TESTED | `24-network-simulator/netlab/`; `tests/netlab_test.sh` — 18 hosted assertions incl. a real Ethernet frame round-tripping through `06-networking`'s actual codec (genuine cross-layer reuse, not a reimplementation), unicast delivery through a switch that does NOT reach an uninvolved third host, an unknown-destination frame correctly flooding, a learning switch correctly forwarding a reply to only the learned port after one prior frame (not flooding again), broadcast reaching every host, an unreachable-MAC frame delivered to nobody without error, and a genuinely cyclic topology terminating without hanging or crashing (explicitly not "correctly" simulating the cycle — no spanning-tree protocol exists). See [`docs/ADR/0022-techuilaguy-netlab-foundation.md`](docs/ADR/0022-techuilaguy-netlab-foundation.md) |
| **IPv4 + ARP + single-hop routing + packet timeline + first mission** (NetLab is now a first-class product frontier, per explicit project direction) | TESTED | `24-network-simulator/netlab/ip.*`, `mission.*`, `topology.*` (Router node kind); `tests/ip_routing_test.sh` — 22 hosted assertions incl. same-subnet ping via direct ARP, the full `PC1 → Switch → Router → Switch → PC2` example pinging end to end with a real, inspectable ARP+IP packet timeline, clean (non-crashing) ARP-failure and no-route-failure reporting with specific diagnostic reasons, byte-for-byte deterministic replay of an identical simulation, and the first gamified mission ("Connect Two Networks") succeeding and failing correctly for same-subnet/missing-IP-config cases. Every frame is built through `06-networking`'s existing Ethernet/ARP/IPv4/ICMP codecs — no protocol reimplementation. See [`docs/ADR/0028-netlab-ip-arp-routing-mission.md`](docs/ADR/0028-netlab-ip-arp-routing-mission.md) for explicit non-goals (no multi-hop routing, no DHCP/DNS/TCP/UDP, no persistent ARP cache, no browser UI/WebAssembly build yet, no additional missions) |
| DHCP/DNS/TCP/UDP behavior, multi-hop routing, VLANs/NAT/firewalls, latency/loss/bandwidth simulation, visual topology editor, packet capture UI, WebAssembly build, browser UI, additional missions/grading/XP/skill tree, multiplayer, save/load | PLANNED | none of these exist yet |

## Layer 14 (AI/ML)

**Status: FOUNDATION.** A real numeric `Tensor` (shape/storage/
elementwise ops/matmul, no gradient tracking) plus a real, separate
reverse-mode **scalar** autodiff engine, used together to actually
train a linear regression model to convergence. Explicitly not
Tensor-level autodiff, not a neural network, and not a general ML
framework.

| Requirement | Status | Evidence |
|---|---|---|
| **Tensor** (shape, flat storage, elementwise add/subtract/multiply, 2D matmul, shape-mismatch rejection) | TESTED | `14-ai/tensor/`; hand-computed expected values incl. a non-square matmul case, and shape-mismatch rejection returning `false` rather than crashing |
| **Scalar reverse-mode autodiff** (`autodiff::Value`: +, unary/binary −, *, real topological-sort-based `backward()`) | TESTED | `14-ai/autodiff/`; gradients verified against hand-derived expected values AND independently against real numerical (finite-difference) gradients; gradient accumulation for a value reused twice in one expression (`x*x` → `2x`) directly tested, not just single-use chains |
| **Linear regression training demo** (`y = wx + b` fit via plain gradient descent on MSE, built on the autodiff engine above) | TESTED | `14-ai/training/linear_regression.*`; `tests/ai_test.sh` (33 hosted assertions total across Tensor/autodiff/training) — trains on a fixed synthetic `y=2x+3` dataset for a fixed epoch count and converges to `weight`/`bias` within a documented 0.05 tolerance every run, loss provably decreases, and inference on an unseen input generalizes correctly. See [`docs/ADR/0024-ai-tensor-autodiff-foundation.md`](docs/ADR/0024-ai-tensor-autodiff-foundation.md) for extensive explicit non-goals (no Tensor-level autodiff, no broadcasting, no neural network layers/optimizers beyond plain gradient descent, no GPU/batching) |
| Tensor-level autodiff, neural network layers/activations, optimizers beyond plain gradient descent, GPU/accelerator support, batching, model serialization | PLANNED | none of these exist yet |

## Layer 17 (Graphics & simulation)

**Status: FOUNDATION.** A real, deterministic CPU software rasterizer
— no GPU/window dependency, a reasoned choice (this project's own
hosted verification runner has no guaranteed GPU/display), not a
fallback. Explicitly no shaders, textures, lighting, anti-aliasing,
scene graph, or clipping yet.

| Requirement | Status | Evidence |
|---|---|---|
| **Mat4 transforms + perspective projection** (identity/translation/scale/rotationZ, matrix multiply, real lookAt view matrix, standard perspective projection), operating on `18-scientific-computing`'s existing `Vec3` | TESTED | `17-graphics/math/mat4.*`; hand-computed expected transformed points for every operation incl. a real multi-matrix composition, and a point directly on the camera's view axis verified to project to exact NDC screen center |
| **FrameBuffer + depth-tested triangle rasterization** (real barycentric rasterization, per-pixel depth test, occlusion correct regardless of draw order) | TESTED | `17-graphics/render/`; a pixel provably inside a triangle vs. provably outside verified at explicit coordinates; two overlapping triangles rendered in both draw orders proven to produce the identical (nearer-wins) result, not painter's-algorithm draw-order dependence |
| **Scene rendering** (world-space triangles + camera → FrameBuffer, end to end) | TESTED | `17-graphics/scene/`; `tests/graphics_test.sh` (18 hosted assertions total across Mat4/FrameBuffer/rasterizer/scene) — a full scene render is byte-for-byte identical across repeated calls (real determinism, not "looks right"), and a triangle centered in front of the camera actually renders visibly near the screen center. See [`docs/ADR/0025-graphics-software-rasterizer-foundation.md`](docs/ADR/0025-graphics-software-rasterizer-foundation.md) for explicit non-goals (no GPU/window/interactive input, no shaders/textures/lighting, no anti-aliasing, no scene graph/asset loading, no near/far/side-plane clipping) |
| **ECS / game-engine foundation** (generation-checked entities, per-type component pools, Transform/Velocity/Mesh/Camera components, a movement system, and a render system genuinely integrated with the software rasterizer above) | TESTED | `17-graphics/ecs/`; `tests/ecs_test.sh` — 24 hosted assertions incl. a stale-entity-handle defense (a recycled numeric id's OLD handle correctly stays "not alive" via generation mismatch), component independence across types, `Transform::toMatrix()` matching a hand-computed translate+rotate+scale composition, deterministic multi-step movement integration, `renderWorld` correctly calling the real (unmodified) `graphics::renderScene`, live ECS state (moving/destroying an entity) provably changing subsequent renders rather than a cached scene, and full render determinism. See [`docs/ADR/0026-ecs-game-engine-foundation.md`](docs/ADR/0026-ecs-game-engine-foundation.md) for explicit non-goals (no archetype storage, no arbitrary 3D rotation, no scene-graph hierarchy, no physics/collision/audio/input/networking, no scripting/serialization) |
| GPU execution, windowing/interactive input, shaders, textures, lighting, anti-aliasing, scene graph, asset loading, clipping, physics/collision, scripting | PLANNED | none of these exist yet |

## Layer 16 (Robotics & autonomy)

**Status: FOUNDATION.** Real forward kinematics for a 2-link planar
arm and a real proportional joint controller, verified against an
independent closed-form analytic solution. No inverse kinematics, no
dynamics, no sensors, no PID, no collision/path planning.

| Requirement | Status | Evidence |
|---|---|---|
| **2-link planar-arm forward kinematics** (standard closed-form trigonometric chain, built on `18-scientific-computing`'s `Vec3`) | TESTED | `16-robotics/kinematics/`; hand-computed expected end-effector positions for zero-angle, 90-degree-first-joint, and second-joint-bend configurations, plus a zero-length-second-link identity check |
| **Proportional (P-only) joint controller + simulated control loop** | TESTED | `16-robotics/control/`; `tests/robotics_test.sh` (9 hosted assertions total) — a single control step matches the hand-derived update exactly; the simulated controller's error after N fixed-dt steps matches an independently-derived closed-form `(1-gain·dt)^N` prediction (not just "got closer"); a full simulated control loop converges the end-effector to within a tight tolerance of the true target configuration's forward kinematics; and the simulation is bit-for-bit deterministic across repeated runs. See [`docs/ADR/0027-robotics-planar-arm-foundation.md`](docs/ADR/0027-robotics-planar-arm-foundation.md) for explicit non-goals (no inverse kinematics, no 3D/arbitrary-link kinematics, no dynamics, no sensors, no PID, no collision/path planning, no visualization integration yet) |
| Inverse kinematics, dynamics, sensors/SLAM, PID, collision/path planning, multi-arm/swarm coordination, Graphics/ECS visualization integration | PLANNED | none of these exist yet |

## Layers 9, 12, 19, 21 (Security,
Cloud,
Finance,
VLEO research)

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
