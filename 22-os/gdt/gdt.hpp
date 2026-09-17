#pragma once

#include <stdint.h>

// This kernel's own GDT layout (previously there was no GDT at all —
// isr_common and the scheduler both hardcoded 0x18 as "the kernel data
// selector", inherited by unverified assumption from whatever GRUB's
// own default GDT happened to leave in place). These are now the
// authoritative values; isr_common and scheduler.cpp both reference
// them instead of a magic number.
inline constexpr uint16_t KERNEL_CODE_SELECTOR = 0x08;
inline constexpr uint16_t KERNEL_DATA_SELECTOR = 0x10;
inline constexpr uint16_t USER_CODE_SELECTOR = 0x18;
inline constexpr uint16_t USER_DATA_SELECTOR = 0x20;
inline constexpr uint16_t TSS_SELECTOR = 0x28;

// Segment selectors as they appear loaded into a register include the
// RPL in the low 2 bits; user-mode segments are always used with RPL 3.
inline constexpr uint16_t USER_CODE_SELECTOR_RPL3 = USER_CODE_SELECTOR | 3;
inline constexpr uint16_t USER_DATA_SELECTOR_RPL3 = USER_DATA_SELECTOR | 3;

// Builds the GDT and TSS, loads them (lgdt/ltr), and reloads every
// segment register (including a far jump to reload CS) to point at
// this kernel's own descriptors instead of the bootloader's.
void gdt_init();

// Updates the TSS's esp0 (the kernel stack the CPU automatically
// switches to on a ring3 -> ring0 transition via interrupt or
// exception). The scheduler calls this on every task switch so that
// whichever task is about to run has the *correct* kernel stack lined
// up before it can possibly take a trap.
extern "C" void tss_set_kernel_stack(uint32_t esp0);
