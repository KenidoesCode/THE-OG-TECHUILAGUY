#include "pic.hpp"

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait() {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

void pic_init() {
    // Start initialization sequence.
    outb(0x20, 0x11);
    io_wait();

    outb(0xA0, 0x11);
    io_wait();

    // Remap PIC vectors.
    outb(0x21, 0x20);
    io_wait();

    outb(0xA1, 0x28);
    io_wait();

    // Master IRQ2 is the slave cascade.
    outb(0x21, 0x04);
    io_wait();

    // Slave identity = IRQ2.
    outb(0xA1, 0x02);
    io_wait();

    // 8086 mode.
    outb(0x21, 0x01);
    io_wait();

    outb(0xA1, 0x01);
    io_wait();

    // Begin with EVERYTHING masked.
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

extern "C" void pic_unmask_irq(uint8_t irq) {
    if (irq < 8) {
        uint8_t mask;
        asm volatile("inb %1, %0" : "=a"(mask) : "Nd"(0x21));

        mask &= static_cast<uint8_t>(~(1u << irq));

        outb(0x21, mask);
    } else if (irq < 16) {
        uint8_t mask;
        asm volatile("inb %1, %0" : "=a"(mask) : "Nd"(0xA1));

        mask &= static_cast<uint8_t>(~(1u << (irq - 8)));

        outb(0xA1, mask);
    }
}

extern "C" void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }

    outb(0x20, 0x20);
}
