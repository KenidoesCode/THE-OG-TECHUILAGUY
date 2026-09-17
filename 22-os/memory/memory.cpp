#include <stdint.h>
#include <stddef.h>

namespace {

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t MEMORY_SIZE = 16 * 1024 * 1024;
constexpr uint32_t PAGE_COUNT = MEMORY_SIZE / PAGE_SIZE;
constexpr uint32_t BITMAP_SIZE = PAGE_COUNT / 8;

uint8_t bitmap[BITMAP_SIZE];

uint32_t allocated_pages = 0;
uint32_t reserved_pages = 0;

// Defined by the linker script: the first address past the kernel's own
// loaded image (text/rodata/data/bss). Never a real object to read —
// only its address matters.
extern "C" uint8_t _kernel_end;

void mark_used(uint32_t page) {
    bitmap[page / 8] |=
        static_cast<uint8_t>(1u << (page % 8));
}

void mark_free(uint32_t page) {
    bitmap[page / 8] &=
        static_cast<uint8_t>(
            ~(1u << (page % 8))
        );
}

bool is_used(uint32_t page) {
    return
        (bitmap[page / 8] &
         (1u << (page % 8))) != 0;
}

}

extern "C" void memory_init() {
    for (uint32_t i = 0; i < BITMAP_SIZE; ++i)
        bitmap[i] = 0;

    /*
     * Reserve everything from address 0 through the end of the
     * kernel's own loaded image.
     *
     * The kernel is linked to start exactly at the 1 MiB mark (see
     * linker.ld), so a fixed "reserve the first 1 MiB" guess reserves
     * *nothing* of the kernel's actual code/data/bss — the very first
     * call to memory_alloc_page() would hand back address 0x100000,
     * the kernel's own first instruction, for immediate overwriting.
     * This was never exercised before now: memory_alloc_page() had no
     * callers anywhere in the tree until real userspace pages needed
     * one.
     */
    uintptr_t kernelEnd =
        reinterpret_cast<uintptr_t>(&_kernel_end);

    uint32_t reservedBytes =
        static_cast<uint32_t>(kernelEnd) > (1024u * 1024u)
            ? static_cast<uint32_t>(kernelEnd)
            : (1024u * 1024u);

    reserved_pages =
        (reservedBytes + PAGE_SIZE - 1) / PAGE_SIZE;

    if (reserved_pages > PAGE_COUNT) {
        reserved_pages = PAGE_COUNT;
    }

    for (uint32_t page = 0;
         page < reserved_pages;
         ++page) {
        mark_used(page);
    }

    allocated_pages = reserved_pages;
}

extern "C" uintptr_t memory_alloc_page() {
    for (uint32_t page = 0;
         page < PAGE_COUNT;
         ++page) {

        if (!is_used(page)) {
            mark_used(page);
            ++allocated_pages;

            return static_cast<uintptr_t>(
                page * PAGE_SIZE
            );
        }
    }

    return 0;
}

extern "C" void memory_free_page(uintptr_t address) {
    if (address == 0)
        return;

    if (address % PAGE_SIZE != 0)
        return;

    uint32_t page =
        static_cast<uint32_t>(
            address / PAGE_SIZE
        );

    if (page >= PAGE_COUNT)
        return;

    if (!is_used(page))
        return;

    /*
     * Never free the reserved region covering the kernel's own image.
     */
    if (page < reserved_pages)
        return;

    mark_free(page);

    if (allocated_pages > 0)
        --allocated_pages;
}

extern "C" uint32_t memory_used_pages() {
    return allocated_pages;
}

extern "C" uint32_t memory_free_pages() {
    return PAGE_COUNT - allocated_pages;
}
