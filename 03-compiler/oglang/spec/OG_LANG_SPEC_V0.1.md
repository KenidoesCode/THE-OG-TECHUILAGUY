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
