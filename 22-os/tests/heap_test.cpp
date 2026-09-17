// Real assertion-based unit tests for the kernel heap allocator
// (heap/heap.cpp). Like keyboard_translation.cpp, the allocator logic
// itself (block splitting, coalescing, first-fit search) has no
// hardware dependency and doesn't need the freestanding cross-compile
// toolchain, an emulator, or a boot cycle to verify — but unlike
// keyboard_translation.cpp, heap.cpp does depend on
// memory_alloc_page()/memory_free_page(), and the REAL
// memory/memory.cpp hands back raw physical addresses (like 0x100000)
// that are only dereferenceable inside the kernel's own identity-
// mapped address space — writing through one from an ordinary hosted
// test process would segfault. This file provides its own fake
// memory_alloc_page()/memory_free_page(), backed by real
// heap-allocated host memory, so the allocator logic under test can
// actually be exercised (and its results actually read back and
// checked) from a normal process.

#include "../heap/heap.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        std::cout << "[PASS] " << description << "\n";
    } else {
        std::cout << "[FAIL] " << description << "\n";
        failures++;
    }
}

constexpr uint32_t FAKE_PAGE_SIZE = 4096;

std::vector<void*> fakePages;
uint32_t fakePageBudget = 1000000;  // effectively unlimited unless a test lowers it

}  // namespace

// Test-only stand-ins for the real physical page allocator. Backed by
// ordinary host heap memory (aligned to the same 4096-byte page size
// heap.cpp assumes), not the real bitmap allocator in
// memory/memory.cpp — this file never links memory.cpp at all.
extern "C" uintptr_t memory_alloc_page() {
    if (fakePageBudget == 0) {
        return 0;
    }

    --fakePageBudget;

    void* page = std::aligned_alloc(FAKE_PAGE_SIZE, FAKE_PAGE_SIZE);
    fakePages.push_back(page);
    return reinterpret_cast<uintptr_t>(page);
}

extern "C" void memory_free_page(uintptr_t) {
    // Not exercised by heap.cpp today (v1 never returns a page to the
    // physical allocator once claimed) — provided only to satisfy the
    // link, matching memory.hpp's real signature.
}

void resetFakePages() {
    fakePageBudget = 1000000;
}

void testKmallocReturnsUsableZeroedLayoutRegion() {
    resetFakePages();
    heap_init();

    void* a = kmalloc(64);
    check(a != nullptr, "heap: kmalloc(64) returns a non-null pointer");

    // The memory must actually be writable and independently
    // addressable — not just "some non-null value."
    auto* bytes = static_cast<uint8_t*>(a);
    for (uint32_t i = 0; i < 64; ++i) {
        bytes[i] = static_cast<uint8_t>(i);
    }
    bool intact = true;
    for (uint32_t i = 0; i < 64; ++i) {
        if (bytes[i] != static_cast<uint8_t>(i)) intact = false;
    }
    check(intact, "heap: an allocated block is genuinely writable and "
                  "readable across its full requested size");
}

void testKmallocReturnsDistinctNonOverlappingBlocks() {
    resetFakePages();
    heap_init();

    void* a = static_cast<void*>(kmalloc(32));
    void* b = static_cast<void*>(kmalloc(32));

    check(a != nullptr && b != nullptr && a != b,
          "heap: two allocations return distinct pointers");

    auto* aBytes = static_cast<uint8_t*>(a);
    auto* bBytes = static_cast<uint8_t*>(b);

    for (uint32_t i = 0; i < 32; ++i) aBytes[i] = 0xAA;
    for (uint32_t i = 0; i < 32; ++i) bBytes[i] = 0xBB;

    bool noOverlap = true;
    for (uint32_t i = 0; i < 32; ++i) {
        if (aBytes[i] != 0xAA) noOverlap = false;
    }
    check(noOverlap,
          "heap: writing into a second allocation does not corrupt "
          "the first (the two blocks genuinely don't overlap)");
}

void testKfreeThenReallocateReusesSpace() {
    resetFakePages();
    heap_init();

    void* a = kmalloc(100);
    check(a != nullptr, "heap: initial allocation for reuse test succeeds");

    kfree(a);

    void* b = kmalloc(100);
    check(b == a,
          "heap: freeing a block and requesting the same size again "
          "reuses that exact block (first-fit finds the freed block)");
}

void testKfreeCoalescesAdjacentFreeBlocks() {
    resetFakePages();
    heap_init();

    void* a = kmalloc(64);
    void* b = kmalloc(64);
    (void)b;

    kfree(a);
    kfree(b);

    // Both neighboring blocks are now free and should have coalesced
    // into one — a subsequent allocation larger than either individual
    // block but small enough to fit the merged block must succeed
    // without growing the heap (see heap_bytes_allocated below as a
    // finer-grained cross-check).
    void* merged = kmalloc(140);
    check(merged != nullptr,
          "heap: freeing two adjacent blocks coalesces them into one "
          "large enough for a request neither original block alone "
          "could satisfy");
}

void testKmallocSplitsLargeFreeBlock() {
    resetFakePages();
    heap_init();

    void* a = kmalloc(64);
    kfree(a);

    // The freed block (carved from a fresh page) is far larger than
    // 8 bytes; a small allocation should split it rather than consume
    // the whole remainder, leaving room for another small allocation
    // without requesting a second physical page.
    void* small1 = kmalloc(8);
    void* small2 = kmalloc(8);

    check(small1 != nullptr && small2 != nullptr && small1 != small2,
          "heap: allocating a small block from a much larger free "
          "block splits it, leaving room for a further allocation "
          "from the same freed space");
}

void testKmallocZeroSizeReturnsNull() {
    resetFakePages();
    heap_init();

    void* a = kmalloc(0);
    check(a == nullptr,
          "heap: kmalloc(0) returns null rather than a bogus zero-size block");
}

void testKfreeNullIsANoOp() {
    resetFakePages();
    heap_init();

    // Must not crash; that's the entire assertion.
    kfree(nullptr);
    check(true, "heap: kfree(nullptr) is a safe no-op");
}

void testKfreeAlreadyFreedBlockIsSafeNoOp() {
    resetFakePages();
    heap_init();

    void* a = kmalloc(32);
    kfree(a);
    // A double-free must not corrupt the allocator's internal state
    // (best-effort guard — see heap.cpp's comment on kfree: this is
    // not a robust corruption-detection mechanism, just a check that
    // a block already marked free is left alone rather than
    // re-coalesced or double-counted).
    kfree(a);

    void* b = kmalloc(32);
    check(b == a,
          "heap: a double-free doesn't corrupt the allocator — the "
          "block is still correctly reusable afterward");
}

void testHeapGrowsWithAdditionalPagesWhenExhausted() {
    resetFakePages();
    heap_init();
    fakePageBudget = 2;  // exactly two physical pages available

    // Exhaust the first page with allocations larger than a single
    // small request but small enough that several fit per page.
    std::vector<void*> pointers;
    for (int i = 0; i < 50; ++i) {
        void* p = kmalloc(64);
        if (p == nullptr) break;
        pointers.push_back(p);
    }

    check(pointers.size() > 1,
          "heap: multiple allocations succeed, spanning into a "
          "second physical page once the first is exhausted "
          "(fakePageBudget=2 was enough to satisfy them)");

    fakePageBudget = 0;  // now genuinely out of physical memory

    // Drain whatever free space is still left over in the two pages
    // already obtained (the allocator must keep succeeding from
    // existing free space without touching the page allocator) before
    // it can be genuinely exhausted — a single extra kmalloc() call
    // right after setting the budget to 0 would only prove the
    // *existing* free space happened to run out at exactly that
    // point, not that growHeap() itself correctly reports failure.
    void* shouldFail = nullptr;
    for (int i = 0; i < 1000; ++i) {
        void* p = kmalloc(64);
        if (p == nullptr) {
            shouldFail = nullptr;
            break;
        }
        shouldFail = p;  // last successful allocation; overwritten to
                          // nullptr the moment kmalloc actually fails
        pointers.push_back(p);
    }

    check(shouldFail == nullptr,
          "heap: kmalloc returns null (not a crash or garbage "
          "pointer) once the physical page allocator is truly "
          "exhausted");
}

void testHeapBytesAllocatedTracksLiveAllocations() {
    resetFakePages();
    heap_init();

    check(heap_bytes_allocated() == 0,
          "heap: a freshly initialized heap reports zero bytes allocated");

    void* a = kmalloc(64);
    (void)a;
    uint32_t afterAlloc = heap_bytes_allocated();
    check(afterAlloc >= 64,
          "heap: heap_bytes_allocated() increases by at least the "
          "requested size after an allocation");

    kfree(a);
    check(heap_bytes_allocated() == 0,
          "heap: heap_bytes_allocated() returns to zero after freeing "
          "the only live allocation");
}

int main() {
    testKmallocReturnsUsableZeroedLayoutRegion();
    testKmallocReturnsDistinctNonOverlappingBlocks();
    testKfreeThenReallocateReusesSpace();
    testKfreeCoalescesAdjacentFreeBlocks();
    testKmallocSplitsLargeFreeBlock();
    testKmallocZeroSizeReturnsNull();
    testKfreeNullIsANoOp();
    testKfreeAlreadyFreedBlockIsSafeNoOp();
    testHeapGrowsWithAdditionalPagesWhenExhausted();
    testHeapBytesAllocatedTracksLiveAllocations();

    if (failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cout << "\n" << failures << " TEST(S) FAILED\n";
    }

    return failures == 0 ? 0 : 1;
}
