#include "paging.hpp"
#include "../kernel/serial.hpp"

namespace {

constexpr uint32_t PAGE_SIZE = 4096;
constexpr uint32_t ENTRIES_PER_TABLE = 1024;

// Must match (or exceed) memory/memory.cpp's MEMORY_SIZE — every
// physical page the allocator can ever hand out has to be mapped.
// 4 tables * 1024 entries * 4 KiB = 16 MiB.
constexpr uint32_t IDENTITY_MAPPED_TABLES = 4;

constexpr uint32_t PAGE_PRESENT = 0x1;
constexpr uint32_t PAGE_WRITABLE = 0x2;
constexpr uint32_t PAGE_USER = 0x4;

alignas(PAGE_SIZE) uint32_t pageDirectory[ENTRIES_PER_TABLE];
alignas(PAGE_SIZE) uint32_t pageTables[IDENTITY_MAPPED_TABLES][ENTRIES_PER_TABLE];

// The U/S bit is enforced at *both* the page-directory and page-table
// level, and the effective permission is the more restrictive of the
// two — so every PDE here is left permissively user-accessible, and
// individual pages are locked to supervisor-only (or not) purely
// through their own PTE. That's what lets most pages in a directory
// stay supervisor-only while one or two specific pages in the same
// range are user-accessible.
bool addressToTableAndIndex(
    uintptr_t physicalAddress,
    uint32_t& tableIndex,
    uint32_t& entryIndex
) {
    if (physicalAddress % PAGE_SIZE != 0) {
        return false;
    }

    uint32_t page = static_cast<uint32_t>(physicalAddress / PAGE_SIZE);
    tableIndex = page / ENTRIES_PER_TABLE;
    entryIndex = page % ENTRIES_PER_TABLE;

    return tableIndex < IDENTITY_MAPPED_TABLES;
}

void setPageFlag(uintptr_t physicalAddress, bool userAccessible) {
    uint32_t tableIndex = 0;
    uint32_t entryIndex = 0;

    if (!addressToTableAndIndex(physicalAddress, tableIndex, entryIndex)) {
        return;
    }

    uint32_t& entry = pageTables[tableIndex][entryIndex];

    if (userAccessible) {
        entry |= PAGE_USER;
    } else {
        entry &= ~PAGE_USER;
    }

    // Flush this single translation from the TLB so the change takes
    // effect immediately rather than on the next unrelated flush.
    asm volatile("invlpg (%0)" : : "r"(physicalAddress) : "memory");
}

}  // namespace

void paging_init() {
    for (uint32_t t = 0; t < IDENTITY_MAPPED_TABLES; ++t) {
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
            uint32_t physicalPage = (t * ENTRIES_PER_TABLE + i) * PAGE_SIZE;

            // Identity map, present, writable, supervisor-only by
            // default. scheduler_create_user_task grants user access
            // to specific pages afterward.
            pageTables[t][i] =
                physicalPage | PAGE_PRESENT | PAGE_WRITABLE;
        }

        pageDirectory[t] =
            reinterpret_cast<uint32_t>(&pageTables[t][0]) |
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    for (uint32_t d = IDENTITY_MAPPED_TABLES; d < ENTRIES_PER_TABLE; ++d) {
        pageDirectory[d] = 0;  // Not present: unmapped.
    }

    uint32_t cr3 = reinterpret_cast<uint32_t>(&pageDirectory[0]);
    asm volatile("movl %0, %%cr3" : : "r"(cr3));

    uint32_t cr0;
    asm volatile("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // PG
    asm volatile("movl %0, %%cr0" : : "r"(cr0));

    serial_write("[PAGE] paging enabled: ");
    serial_write_decimal(IDENTITY_MAPPED_TABLES * ENTRIES_PER_TABLE * PAGE_SIZE / (1024 * 1024));
    serial_write(" MiB identity-mapped, supervisor-only by default\n");
}

void paging_set_user_accessible(uintptr_t physicalAddress) {
    setPageFlag(physicalAddress, true);
}

void paging_set_supervisor_only(uintptr_t physicalAddress) {
    setPageFlag(physicalAddress, false);
}
