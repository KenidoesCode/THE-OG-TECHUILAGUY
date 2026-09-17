#pragma once

#include <stdint.h>

// 32-bit (non-PAE) paging with real per-process address spaces.
//
// Every address space shares the same identity-mapped kernel region
// (physical address == virtual address, covering the physical
// allocator's full 16 MiB range, supervisor-only) so kernel code and
// data are always reachable regardless of which address space is
// active. On top of that, each user task gets its own page directory
// and its own dedicated page table for a fixed private virtual region
// (see USER_CODE_VADDR/USER_STACK_*), mapping that task's own
// physical code/stack pages there and nowhere else. No other address
// space's tables reference those physical pages at that virtual
// address at all — this is what makes one process structurally unable
// to reach another's memory, not merely permission-denied from it.
//
// This does not give per-process protection *within* the identity-
// mapped kernel region itself (every address space's kernel-region
// PDEs point at the exact same shared page tables, by design — the
// kernel must always be reachable) — only the private per-task region
// is genuinely isolated between processes.

inline constexpr uint32_t PAGE_SIZE_BYTES = 4096;

// Fixed virtual layout every user task's private region uses,
// identical across tasks — what differs between tasks is which
// physical pages their own address space's tables translate these
// virtual addresses to. 0x01000000 (16 MiB) is chosen because it sits
// immediately past the shared identity-mapped kernel region.
inline constexpr uint32_t USER_VIRTUAL_BASE = 0x01000000;
inline constexpr uint32_t USER_CODE_VADDR = USER_VIRTUAL_BASE;
inline constexpr uint32_t USER_STACK_PAGE_VADDR = USER_VIRTUAL_BASE + PAGE_SIZE_BYTES;
inline constexpr uint32_t USER_STACK_TOP_VADDR = USER_VIRTUAL_BASE + 2 * PAGE_SIZE_BYTES;

// Builds the shared kernel identity map and enables paging (CR0.PG).
// Must run before any address space is created or switched to.
void paging_init();

// Returns the physical address (== CR3 value) of the shared, kernel-
// only base address space every kernel-mode task runs under. Never
// destroyed.
uint32_t paging_kernel_address_space();

// Allocates a new address space (one physical page for its directory)
// with the shared kernel region already mapped and nothing else.
// Returns its physical address (== the CR3 value to switch to it), or
// 0 if a physical page could not be allocated.
uint32_t paging_create_address_space();

// Maps `physicalPage` into the given address space at `virtualAddress`
// (both must be page-aligned), user-accessible, allocating a private
// page table for that address space if one doesn't already exist for
// the relevant region. Refuses (returns false) any virtualAddress
// inside the shared kernel region — that mapping is fixed and shared,
// never per-address-space. Returns false if a physical page for a new
// page table could not be allocated.
bool paging_map_user_page(
    uint32_t addressSpace,
    uint32_t virtualAddress,
    uint32_t physicalPage
);

// Loads CR3. Called by the scheduler on every task switch so the
// currently running task's own address space is always the one
// actually translating memory accesses.
extern "C" void paging_switch_address_space(uint32_t addressSpace);

// Frees an address space's directory and any private page tables it
// owns (never the shared kernel tables). Does NOT free the physical
// pages that were mapped into it — the caller (the scheduler, which
// already tracks task->userCodePage/userStackPage) remains responsible
// for returning those to the general allocator itself, exactly as
// before this existed.
void paging_destroy_address_space(uint32_t addressSpace);
