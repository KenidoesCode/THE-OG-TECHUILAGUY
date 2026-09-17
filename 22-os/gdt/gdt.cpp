#include "gdt.hpp"
#include "../kernel/serial.hpp"

namespace {

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Standard i386 hardware task-switch segment layout. Only used here as
// a place for the CPU to find ss0:esp0 on a ring3 -> ring0 transition;
// this kernel does not use hardware task-switching (ljmp/ltr-driven
// task gates) as its actual multitasking mechanism — that remains the
// software scheduler in scheduler.cpp, which resumes tasks by loading
// a saved trap frame the same way isr_common always has.
struct TSS {
    uint32_t prev_task;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

constexpr int GDT_ENTRIES = 6;

GDTEntry gdt[GDT_ENTRIES];
GDTPointer gdt_pointer;
TSS tss;

void set_gate(
    int index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity
) {
    gdt[index].base_low = static_cast<uint16_t>(base & 0xFFFF);
    gdt[index].base_mid = static_cast<uint8_t>((base >> 16) & 0xFF);
    gdt[index].base_high = static_cast<uint8_t>((base >> 24) & 0xFF);

    gdt[index].limit_low = static_cast<uint16_t>(limit & 0xFFFF);
    gdt[index].granularity =
        static_cast<uint8_t>(((limit >> 16) & 0x0F) | (granularity & 0xF0));

    gdt[index].access = access;
}

}  // namespace

extern "C" void gdt_flush(uint32_t gdt_pointer_address);
extern "C" void tss_flush();

void gdt_init() {
    // Null descriptor.
    set_gate(0, 0, 0, 0, 0);

    // Kernel code: base 0, limit 4 GiB, present, DPL 0, code
    // (execute/read), 4 KiB granularity, 32-bit.
    set_gate(1, 0, 0xFFFFF, 0x9A, 0xC0);

    // Kernel data: present, DPL 0, data (read/write).
    set_gate(2, 0, 0xFFFFF, 0x92, 0xC0);

    // User code: present, DPL 3, code (execute/read).
    set_gate(3, 0, 0xFFFFF, 0xFA, 0xC0);

    // User data: present, DPL 3, data (read/write).
    set_gate(4, 0, 0xFFFFF, 0xF2, 0xC0);

    // TSS: present, DPL 0, type 1001 (32-bit TSS, available). No
    // page/byte granularity bits set — the limit is an exact byte
    // count, not a page count.
    for (uint32_t i = 0; i < sizeof(TSS); ++i) {
        reinterpret_cast<uint8_t*>(&tss)[i] = 0;
    }

    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.esp0 = 0;  // Set per-task by tss_set_kernel_stack before use.
    // No I/O permission bitmap: point iomap_base past the segment
    // limit so every port access from ring 3 faults.
    tss.iomap_base = sizeof(TSS);

    uint32_t tssBase = reinterpret_cast<uint32_t>(&tss);
    uint32_t tssLimit = sizeof(TSS) - 1;
    set_gate(5, tssBase, tssLimit, 0x89, 0x00);

    gdt_pointer.limit = sizeof(gdt) - 1;
    gdt_pointer.base = reinterpret_cast<uint32_t>(&gdt[0]);

    gdt_flush(reinterpret_cast<uint32_t>(&gdt_pointer));
    tss_flush();

    serial_write("[GDT ] GDT + TSS installed; ring-3 segments ready\n");
}

extern "C" void tss_set_kernel_stack(uint32_t esp0) {
    tss.esp0 = esp0;
}
