#include <stdint.h>
#include <stddef.h>

namespace {

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t MEMORY_SIZE = 16 * 1024 * 1024;
constexpr uint32_t PAGE_COUNT = MEMORY_SIZE / PAGE_SIZE;
constexpr uint32_t BITMAP_SIZE = PAGE_COUNT / 8;

uint8_t bitmap[BITMAP_SIZE];

uint32_t allocated_pages = 0;

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
     * Reserve the first 1 MiB.
     *
     * This region contains boot/kernel/hardware-sensitive
     * memory and is not available to the general allocator.
     */
    constexpr uint32_t RESERVED =
        1024 * 1024 / PAGE_SIZE;

    for (uint32_t page = 0;
         page < RESERVED;
         ++page) {
        mark_used(page);
    }

    allocated_pages = RESERVED;
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
     * Never free the reserved low-memory region.
     */
    constexpr uint32_t RESERVED =
        1024 * 1024 / PAGE_SIZE;

    if (page < RESERVED)
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
