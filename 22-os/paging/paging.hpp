#pragma once

#include <stdint.h>

// 32-bit (non-PAE) identity-mapped paging: virtual address == physical
// address for every mapped page. This does not yet give each process
// its own address space (that needs per-task page directories, not
// implemented yet) — what it gives right now is real per-page
// user/kernel memory protection, which nothing enforced before this at
// all: with paging disabled, ring 3 could read or write any physical
// address whatsoever (segment limits are flat 0..4GiB, so they impose
// no restriction), which made the "privilege isolation" from the
// GDT/TSS/ring-3 work isolate *instructions* but not *memory*.
//
// Every page starts supervisor-only (U/S = 0) except the ones
// explicitly granted to a user task via paging_set_user_accessible —
// see scheduler_create_user_task, which calls this for exactly the
// code and stack pages it allocates for that task, nothing else.

void paging_init();

// `physicalAddress` must be 4 KiB-aligned. Marks that page
// user-accessible (U/S = 1) in the identity-mapped page table.
void paging_set_user_accessible(uintptr_t physicalAddress);

// Marks a page supervisor-only again (U/S = 0) — used when a page
// returns to the general allocator, so a stale user-accessible mapping
// can never persist onto whatever the page is reused for next.
void paging_set_supervisor_only(uintptr_t physicalAddress);
