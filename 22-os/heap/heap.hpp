#pragma once

#include <stdint.h>
#include <stddef.h>

// A real, dynamically-growing kernel heap (kmalloc/kfree) on top of the
// physical page allocator (memory/memory.cpp) — see heap.cpp for the
// full design and its explicitly-scoped v1 limits (each heap segment
// is exactly one physical page, so a single allocation cannot exceed
// one page minus header overhead; no corruption detection beyond a
// best-effort double-free check).
extern "C" void heap_init();
extern "C" void* kmalloc(uint32_t size);
extern "C" void kfree(void* pointer);

// Total bytes currently handed out across all live allocations (used
// blocks only, not the header/segment bookkeeping overhead) — exposed
// for tests and diagnostics, not something callers need for normal use.
extern "C" uint32_t heap_bytes_allocated();
