#include "heap.hpp"

#include "../memory/memory.hpp"

namespace {

constexpr uint32_t PAGE_SIZE_BYTES = 4096;
constexpr uint32_t ALIGNMENT = 8;

// A minimum leftover size worth splitting into its own free block —
// below this, keeping the excess attached to the allocated block
// (some internal fragmentation) beats the bookkeeping overhead of a
// near-empty separate block.
constexpr uint32_t MIN_SPLIT_PAYLOAD = 8;

struct BlockHeader {
    uint32_t size;      // usable payload size, in bytes (excludes this header)
    bool used;

    // Address-ordered, intrusive doubly-linked list of every block
    // (used and free) *within this block's own segment* — coalescing
    // only ever looks at these neighbors, never across segments, since
    // two pages obtained from the physical allocator are not
    // guaranteed to be contiguous in physical memory.
    BlockHeader* next;
    BlockHeader* prev;
};

// One physical page, carved into one or more BlockHeaders. Segments
// themselves form a simple singly-linked list; kmalloc scans every
// segment's block list for a first-fit free block before requesting a
// new page.
struct Segment {
    Segment* next;
    BlockHeader* firstBlock;
};

Segment* segments = nullptr;
uint32_t bytesAllocated = 0;

uint32_t alignUp(uint32_t value) {
    return (value + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

void* blockPayload(BlockHeader* block) {
    return reinterpret_cast<void*>(
        reinterpret_cast<uint8_t*>(block) + sizeof(BlockHeader)
    );
}

BlockHeader* payloadToBlock(void* payload) {
    return reinterpret_cast<BlockHeader*>(
        reinterpret_cast<uint8_t*>(payload) - sizeof(BlockHeader)
    );
}

// Splits `block` (already known to be free and large enough) so that
// exactly `size` bytes remain in it and the leftover becomes a new
// free block immediately after it in the segment's block list — only
// when the leftover is large enough to be worth it; otherwise the
// excess just stays as internal fragmentation on `block`.
void splitIfWorthwhile(BlockHeader* block, uint32_t size) {
    uint32_t remaining = block->size - size;

    if (remaining < sizeof(BlockHeader) + MIN_SPLIT_PAYLOAD) {
        return;
    }

    auto* newBlock = reinterpret_cast<BlockHeader*>(
        reinterpret_cast<uint8_t*>(blockPayload(block)) + size
    );

    newBlock->size = remaining - sizeof(BlockHeader);
    newBlock->used = false;
    newBlock->next = block->next;
    newBlock->prev = block;

    if (block->next != nullptr) {
        block->next->prev = newBlock;
    }

    block->next = newBlock;
    block->size = size;
}

// Requests one fresh physical page and lays out a new Segment (at the
// page's start) followed by exactly one free BlockHeader spanning the
// rest of the page. Returns false (leaving the heap unchanged) if the
// physical allocator is out of pages.
bool growHeap() {
    uintptr_t page = memory_alloc_page();

    if (page == 0) {
        return false;
    }

    auto* segment = reinterpret_cast<Segment*>(page);

    auto* block = reinterpret_cast<BlockHeader*>(
        reinterpret_cast<uint8_t*>(segment) + sizeof(Segment)
    );

    block->size = PAGE_SIZE_BYTES - sizeof(Segment) - sizeof(BlockHeader);
    block->used = false;
    block->next = nullptr;
    block->prev = nullptr;

    segment->firstBlock = block;
    segment->next = segments;
    segments = segment;

    return true;
}

BlockHeader* findFreeBlock(uint32_t size) {
    for (Segment* segment = segments;
         segment != nullptr;
         segment = segment->next) {

        for (BlockHeader* block = segment->firstBlock;
             block != nullptr;
             block = block->next) {

            if (!block->used && block->size >= size) {
                return block;
            }
        }
    }

    return nullptr;
}

}  // namespace

extern "C" void heap_init() {
    segments = nullptr;
    bytesAllocated = 0;
}

extern "C" void* kmalloc(uint32_t size) {
    if (size == 0) {
        return nullptr;
    }

    uint32_t aligned = alignUp(size);

    BlockHeader* block = findFreeBlock(aligned);

    if (block == nullptr) {
        if (!growHeap()) {
            // Out of physical memory — return null rather than a
            // dangling/garbage pointer, exactly like a hosted
            // malloc() failing.
            return nullptr;
        }

        block = findFreeBlock(aligned);

        // A brand new segment provides
        // PAGE_SIZE_BYTES - sizeof(Segment) - sizeof(BlockHeader)
        // bytes; if `aligned` still doesn't fit, the request is
        // larger than a single page can ever satisfy — an explicit
        // v1 limit (see heap.hpp), not a bug to work around here.
        if (block == nullptr) {
            return nullptr;
        }
    }

    splitIfWorthwhile(block, aligned);
    block->used = true;
    bytesAllocated += block->size;

    return blockPayload(block);
}

extern "C" void kfree(void* pointer) {
    if (pointer == nullptr) {
        return;
    }

    BlockHeader* block = payloadToBlock(pointer);

    // Best-effort double-free guard: a block already marked free was
    // either never allocated by kmalloc or already freed once. This
    // is not a robust corruption-detection mechanism — there is no
    // canary, no redzone, and no validation that `pointer` was ever a
    // real allocation at all; a caller passing a bogus pointer still
    // has undefined behavior, exactly as with a hosted C free().
    if (!block->used) {
        return;
    }

    block->used = false;
    bytesAllocated -= block->size;

    // Coalesce with the next block first (so `block->size` is fully
    // up to date before a possible merge into `prev` immediately
    // after), then with the previous block.
    if (block->next != nullptr && !block->next->used) {
        BlockHeader* next = block->next;
        block->size += sizeof(BlockHeader) + next->size;
        block->next = next->next;
        if (block->next != nullptr) {
            block->next->prev = block;
        }
    }

    if (block->prev != nullptr && !block->prev->used) {
        BlockHeader* prev = block->prev;
        prev->size += sizeof(BlockHeader) + block->size;
        prev->next = block->next;
        if (prev->next != nullptr) {
            prev->next->prev = prev;
        }
    }
}

extern "C" uint32_t heap_bytes_allocated() {
    return bytesAllocated;
}
