#include "paging.hpp"
#include "../kernel/serial.hpp"
#include "../memory/memory.hpp"

namespace {

constexpr uint32_t ENTRIES_PER_TABLE = 1024;

// Must match (or exceed) memory/memory.cpp's MEMORY_SIZE — every
// physical page the allocator can ever hand out has to be mapped.
// 4 tables * 1024 entries * 4 KiB = 16 MiB.
constexpr uint32_t IDENTITY_MAPPED_TABLES = 4;

constexpr uint32_t PAGE_PRESENT = 0x1;
constexpr uint32_t PAGE_WRITABLE = 0x2;
constexpr uint32_t PAGE_USER = 0x4;
constexpr uint32_t PAGE_ADDRESS_MASK = 0xFFFFF000;

// The kernel's own base address space: shared by every kernel-mode
// task (idle, the scheduler self-test tasks). Statically allocated —
// unlike per-process address spaces, this one is never destroyed.
alignas(PAGE_SIZE_BYTES) uint32_t kernelDirectory[ENTRIES_PER_TABLE];
alignas(PAGE_SIZE_BYTES) uint32_t identityTables[IDENTITY_MAPPED_TABLES][ENTRIES_PER_TABLE];

uint32_t pdeIndexFor(uint32_t vaddr) { return vaddr >> 22; }
uint32_t pteIndexFor(uint32_t vaddr) { return (vaddr >> 12) & 0x3FF; }

}  // namespace

void paging_init() {
    for (uint32_t t = 0; t < IDENTITY_MAPPED_TABLES; ++t) {
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
            uint32_t physicalPage = (t * ENTRIES_PER_TABLE + i) * PAGE_SIZE_BYTES;

            // Identity map, present, writable, supervisor-only. This
            // region is shared and fixed across every address space;
            // per-process isolation lives entirely in each address
            // space's own private tables for the user-virtual region
            // instead (see paging_map_user_page).
            identityTables[t][i] =
                physicalPage | PAGE_PRESENT | PAGE_WRITABLE;
        }

        kernelDirectory[t] =
            reinterpret_cast<uint32_t>(&identityTables[t][0]) |
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }

    for (uint32_t d = IDENTITY_MAPPED_TABLES; d < ENTRIES_PER_TABLE; ++d) {
        kernelDirectory[d] = 0;  // Not present: unmapped.
    }

    uint32_t cr3 = reinterpret_cast<uint32_t>(&kernelDirectory[0]);
    asm volatile("movl %0, %%cr3" : : "r"(cr3));

    uint32_t cr0;
    asm volatile("movl %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // PG
    asm volatile("movl %0, %%cr0" : : "r"(cr0));

    serial_write("[PAGE] paging enabled: ");
    serial_write_decimal(IDENTITY_MAPPED_TABLES * ENTRIES_PER_TABLE * PAGE_SIZE_BYTES / (1024 * 1024));
    serial_write(" MiB identity-mapped, supervisor-only by default\n");
}

uint32_t paging_kernel_address_space() {
    return reinterpret_cast<uint32_t>(&kernelDirectory[0]);
}

uint32_t paging_create_address_space() {
    uintptr_t dirPhys = memory_alloc_page();
    if (dirPhys == 0) {
        return 0;
    }

    // The kernel's own identity map covers this page (every address
    // space's low 16 MiB is identity-mapped and always active while
    // this code runs), so writing through its physical address here
    // is always valid regardless of which address space is currently
    // loaded.
    uint32_t* dir = reinterpret_cast<uint32_t*>(dirPhys);

    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
        dir[i] = 0;
    }

    // Share the kernel region's page tables verbatim — same physical
    // table, same flags — so kernel code/data are reachable no matter
    // which address space is active.
    for (uint32_t t = 0; t < IDENTITY_MAPPED_TABLES; ++t) {
        dir[t] = kernelDirectory[t];
    }

    return static_cast<uint32_t>(dirPhys);
}

bool paging_map_user_page(
    uint32_t addressSpace,
    uint32_t virtualAddress,
    uint32_t physicalPage
) {
    if (virtualAddress % PAGE_SIZE_BYTES != 0 ||
        physicalPage % PAGE_SIZE_BYTES != 0) {
        return false;
    }

    uint32_t pdeIndex = pdeIndexFor(virtualAddress);

    if (pdeIndex < IDENTITY_MAPPED_TABLES) {
        // Never let a caller repurpose the shared kernel region.
        return false;
    }

    uint32_t* dir = reinterpret_cast<uint32_t*>(addressSpace);
    uint32_t* table;

    if ((dir[pdeIndex] & PAGE_PRESENT) == 0) {
        uintptr_t tablePhys = memory_alloc_page();
        if (tablePhys == 0) {
            return false;
        }

        table = reinterpret_cast<uint32_t*>(tablePhys);
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
            table[i] = 0;
        }

        dir[pdeIndex] = static_cast<uint32_t>(tablePhys) |
                        PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    } else {
        table = reinterpret_cast<uint32_t*>(dir[pdeIndex] & PAGE_ADDRESS_MASK);
    }

    uint32_t pteIndex = pteIndexFor(virtualAddress);
    table[pteIndex] = (physicalPage & PAGE_ADDRESS_MASK) |
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    asm volatile("invlpg (%0)" : : "r"(virtualAddress) : "memory");

    return true;
}

extern "C" void paging_switch_address_space(uint32_t addressSpace) {
    asm volatile("movl %0, %%cr3" : : "r"(addressSpace) : "memory");
}

void paging_destroy_address_space(uint32_t addressSpace) {
    if (addressSpace == paging_kernel_address_space()) {
        return;  // Never destroy the shared kernel base address space.
    }

    uint32_t* dir = reinterpret_cast<uint32_t*>(addressSpace);

    for (uint32_t i = IDENTITY_MAPPED_TABLES; i < ENTRIES_PER_TABLE; ++i) {
        if (dir[i] & PAGE_PRESENT) {
            uintptr_t tablePhys = dir[i] & PAGE_ADDRESS_MASK;
            memory_free_page(tablePhys);
            dir[i] = 0;
        }
    }

    memory_free_page(addressSpace);
}
