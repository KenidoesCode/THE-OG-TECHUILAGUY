# ADR 0001: OGLang's current memory model

**Status:** Accepted (describes the implemented state as of this
writing; superseded whenever ownership/borrowing lands — see "Future
direction" below, which is not yet implemented).

## Context

The PRD (FR-LANG-4) requires "a documented memory-safety model
(ownership/borrowing or an explicitly designed alternative) with
tested invariants." OGLang has real pointers (`&`, `*` read/write) and
fixed-size arrays as of this ADR. Neither ownership nor borrowing is
implemented. This ADR exists so that fact is recorded precisely,
rather than left implicit or glossed over — per this project's own
rule that marketing language is a bug, "OGLang has pointers" must not
be allowed to imply "OGLang has memory safety" when it does not.

## Decision

OGLang's memory model today is **raw pointers with no safety
enforcement**, similar in spirit to C, deliberately chosen as the
simplest model that lets the compiler's existing register-allocator
and codegen infrastructure (built for the pointer and array features)
be reused without also having to design and implement a borrow
checker in the same pass. This is an explicit, temporary waypoint, not
the intended end state — see "Future direction."

### What is true today (implemented, tested)

- Pointers (`ptr`) are raw addresses with no lifetime tracking, no
  ownership tracking, and no borrow checking of any kind.
- Taking a variable's address (`&x`) forces that variable out of the
  register allocator's coloring pool and into a stable stack slot —
  this is a **codegen correctness requirement** (a register has no
  address a pointer could hold), not a safety feature. It guarantees
  the *address stays valid for the variable's stack lifetime*; it says
  nothing about what happens if that pointer is read after the
  variable's containing function has returned (see "Known gaps").
- Array indexing (`arr[i]`) **is bounds-checked at run time** (added
  after this ADR was first written — see "Amendment" below): an
  out-of-range or negative index traps (process exit 101) instead of
  computing and using an out-of-bounds address.
- Dereferencing (`*p`) performs **no null check** and **no validity
  check** of any kind. `p` is trusted unconditionally.
- There is no distinction between a mutable and an immutable pointer;
  every `ptr` can be used to write.
- There is no heap and no dynamic allocation in OGLang programs
  themselves (the OS's physical page allocator is unrelated — no
  OGLang-level `malloc`/`free` equivalent exists), so use-after-free of
  heap memory is not applicable yet; use-after-return (a dangling
  pointer to a stack frame that has already been torn down) **is**
  possible and **is not detected**.

### Tested invariants (the "with tested invariants" half of FR-LANG-4)

These are the guarantees the test suite actually verifies — correctness
of the mechanism, not safety of arbitrary programs:

- `tests/programs/pointer_aliasing.og`: mutating through `*p` is
  visible when the pointed-to variable is read by name afterward
  (i.e., `&x` really does alias `x`'s storage, not a copy).
- `tests/programs/pointer_spill.og`: a pointer value that is itself
  spilled to the stack (not just the variable it points to) survives a
  loop, a branch, and a function call without corruption — this is
  what caught a real 64-bit-address-truncation bug during development.
- `tests/programs/arrays.og`, `arrays_with_calls.og`: array elements
  written through one loop are correctly read back through a separate
  loop, including under register pressure and interleaved function
  calls.
- Unit tests (`tests/unit_test.sh`) verify the type checker rejects
  dereferencing a non-pointer, storing a non-`i32` through a pointer,
  taking the address of an undeclared variable, indexing an unknown
  array, and a non-positive array size — the checks that exist, not a
  claim that these are the only unsafe patterns the language allows.

### Known gaps (explicitly not claimed as solved)

- **No use-after-return detection.** A function can take `&local` and
  return that pointer (or store it somewhere a caller reads later);
  nothing rejects this at compile time or run time.
- **No aliasing/mutability discipline.** Any number of `ptr` values may
  alias the same storage and any of them may write through it at any
  time; there is no borrow-checker-style "at most one mutable
  reference" rule.
- **No null-pointer concept.** There is no `null`/`Option`-like value
  for `ptr`; an uninitialized or arbitrary pointer value is whatever
  bit pattern happened to be in its storage.

## Future direction (not implemented; recorded so it isn't lost)

The PRD's stated alternative to a full ownership/borrowing system is
"an explicitly designed alternative" — this ADR is that explicit
design record for the *current* state, and the following are candidate
next steps, in roughly increasing order of effort:

1. ~~Bounds-checked array indexing~~ — **done** (see "Amendment"
   below): a single unsigned comparison (`index >= size`, which also
   catches a negative index) traps with a distinct exit status (101)
   instead of computing an out-of-bounds address.
2. A `const`/`mut` distinction on pointer types, checked at compile
   time (read-only pointers cannot be used as a `StoreStmt` target).
   Not started.
3. A real borrow-checking pass (lifetimes tied to lexical scope,
   at-most-one-mutable-or-many-immutable-borrows enforcement) — a
   substantial, multi-part effort comparable in scope to the register
   allocator itself, not attempted until 2 is in place and proven with
   its own tests. Not started.

## Amendment (bounds checking)

Array indexing gained a real runtime bounds check shortly after this
ADR was first written. `arr[i]` now lowers to a `BoundsCheckI32`
instruction before the address computation: `cmpl $size, index; jb
.Lok` — an *unsigned* comparison, so `index >= size` fails the check
whether `index` is too large or negative (a negative `i32` reinterpreted
as unsigned is a huge value, well past `size`). Failure traps via a
direct `exit(101)` syscall rather than computing and dereferencing an
out-of-bounds address. This does not change any of "what is true
today" above except the one line it corrects, and does not touch
ownership, borrowing, aliasing, or use-after-return — those remain
exactly as originally documented.

Verified with both a too-large index and a negative index
(`03-compiler/oglang/tests/programs/array_out_of_bounds.og`,
`array_negative_index.og`), each asserted to produce exit code 101, not
a segfault or a silently-wrong value.

## Consequences

Every claim about OGLang elsewhere in this repository (README.md,
PROJECT_STATE.md) must describe pointers/arrays as implemented and
tested for the specific mechanisms listed above, and must not describe
OGLang as "memory-safe" — it is not, by design, at this stage. This ADR
is the single source of truth for that distinction until a future ADR
supersedes it.
