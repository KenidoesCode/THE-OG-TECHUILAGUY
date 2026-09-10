#include "../kernel/serial.hpp"

#include "../memory/memory.hpp"
#include "../interrupts/interrupts.hpp"
#include "../interrupts/pic.hpp"
#include "../scheduler/scheduler.hpp"
#include "../syscalls/syscalls.hpp"
#include "../vfs/vfs.hpp"
#include "../security/security.hpp"

extern "C" void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_magic;
    (void)multiboot_info;

    serial_init();

    serial_write("\n");
    serial_write("====================================\n");
    serial_write("      TECHUILAGUY OS KERNEL\n");
    serial_write("====================================\n");

    serial_write("[BOOT] kernel entered\n");

    memory_init();
    serial_write("[MEM ] physical frame allocator online\n");

    interrupts_init();
    pic_init();

    serial_write("[INT ] interrupt subsystem online\n");

    scheduler_init();
    serial_write("[SCHED] scheduler initialized\n");

    syscall_init();
    serial_write("[SYS ] syscall subsystem initialized\n");

    vfs_init();
    serial_write("[VFS ] virtual filesystem initialized\n");

    security_init();
    serial_write("[SEC ] capability security initialized\n");

    serial_write("------------------------------------\n");
    serial_write("TECHUILAGUY OS IRQ0 HARDWARE TEST\n");
    serial_write("------------------------------------\n");

    serial_write("[TEST] all PIC IRQs masked\n");

    /*
     * IRQ0 is the PIT timer interrupt.
     *
     * The PIT hardware is already running, so once IRQ0
     * is unmasked we expect hardware timer interrupts.
     */
    pic_unmask_irq(0);

    serial_write("[TEST] IRQ0 unmasked\n");
    serial_write("[TEST] IRQ0 handler = ASM ONLY\n");
    serial_write("[TEST] enabling interrupts\n");

    interrupts_enable();

    serial_write("[PASS] STI returned\n");
    serial_write("[PASS] IRQ0 hardware path survived\n");

    while (true) {
        asm volatile("hlt");
    }
}
