# OGLang Language Specification v0.1

## Goals

OGLang is a high-level systems programming language designed for:

- memory safety
- predictable performance
- low latency
- explicit resource management
- capability-based security
- safe concurrency
- portability
- long-term cryptographic agility

These are design targets, not a description of the current
implementation. For the actual, honestly-labeled state of the memory
model specifically (raw/unsafe pointers today, no ownership/borrowing),
see [`../../docs/ADR/0001-oglang-memory-model.md`](../../docs/ADR/0001-oglang-memory-model.md).

## Modules (implemented, v1)

Unlike every other section in this document, this one describes real,
tested, implemented behavior, not a design target — see
[`../../docs/ADR/0002-oglang-modules.md`](../../docs/ADR/0002-oglang-modules.md)
for the full design and its explicitly-scoped limits.

- One source file is one module, named after its filename.
- `import other;` makes `other`'s top-level functions/structs/enums
  reachable only as `other.symbol` — never unqualified.
- `ogc file1.og file2.og ...` compiles, links, and runs a real
  multi-file program as one native binary.
- Not implemented: separately-compiled objects (the whole program is
  still lowered into one assembly file), transitive re-export,
  selective/partial imports, visibility control, module aliasing, and
  import cycles (rejected outright, not supported).

## Inline assembly (implemented, v1)

Real, tested, but deliberately narrow — see
[`../../docs/ADR/0003-oglang-inline-asm.md`](../../docs/ADR/0003-oglang-inline-asm.md).

- `asm("template")` emits the template text verbatim into the
  generated assembly; the expression's value is whatever ends up in
  `%eax` afterward.
- No operand binding: no way to pass an OGLang value in, exactly one
  implicit output (`%eax`).
- Any other live value is saved/restored around the asm block (the
  same mechanism `Call` uses for a callee it can't inspect).
- `volatile` and atomics are explicitly NOT implemented: the optimizer
  performs no reordering or elimination of any kind today, so either
  keyword would be vacuous syntax rather than a real guarantee.

## Primitive Types

bool
i8 i16 i32 i64 i128
u8 u16 u32 u64 u128
f32 f64
usize isize

## Ownership

Every owned resource has a unique owner.

Moving a resource transfers ownership.

Using a moved resource is a compile-time error.

## Borrowing

Shared borrowing permits multiple readers.

Mutable borrowing permits exactly one mutable access.

Shared and mutable borrowing cannot overlap.

## Allocation

Ordinary language operations do not implicitly allocate heap memory.

Allocation strategy must be explicit.

Supported strategies will include:

- stack
- arena
- pool
- region
- heap
- custom allocator

## Capabilities

Authority is represented explicitly by capabilities.

Programs receive only the capabilities granted to them.

## Unsafe

Operations that cannot be proven safe by the language are explicitly marked unsafe.

Unsafe boundaries must be auditable.

## Concurrency

The language aims for compile-time prevention of data races.

Concurrency primitives will include:

- tasks
- channels
- atomics
- async execution
- actors

## Cryptography

Cryptographic APIs must support algorithm agility.

Post-quantum cryptography is a first-class design requirement.
