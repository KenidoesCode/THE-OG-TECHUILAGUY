#include "interrupts.hpp"
#include "../kernel/serial.hpp"
#include "../interrupts/pic.hpp"
#include "../scheduler/pit.hpp"
#include "../scheduler/scheduler.hpp"
#include "../drivers/keyboard.hpp"

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct IDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static IDTEntry idt[256];
static IDTPointer idt_pointer;

extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();
extern "C" void isr20();
extern "C" void isr21();
extern "C" void isr22();
extern "C" void isr23();
extern "C" void isr24();
extern "C" void isr25();
extern "C" void isr26();
extern "C" void isr27();
extern "C" void isr28();
extern "C" void isr29();
extern "C" void isr30();
extern "C" void isr31();

extern "C" void irq0();
extern "C" void irq1();
extern "C" void isr_yield();

using ISR = void (*)();

static uint16_t kernel_code_selector() {
    uint16_t cs;

    asm volatile(
        "movw %%cs, %0"
        : "=r"(cs)
    );

    return cs;
}

static void idt_set_gate(
    uint8_t vector,
    uintptr_t handler,
    uint16_t selector
) {
    idt[vector].offset_low =
        static_cast<uint16_t>(handler & 0xFFFF);

    idt[vector].selector = selector;
    idt[vector].zero = 0;

    // 32-bit interrupt gate, present, DPL=0.
    idt[vector].type_attr = 0x8E;

    idt[vector].offset_high =
        static_cast<uint16_t>((handler >> 16) & 0xFFFF);
}

static void install_exception(uint8_t vector, ISR handler) {
    idt_set_gate(
        vector,
        reinterpret_cast<uintptr_t>(handler),
        kernel_code_selector()
    );
}

void interrupts_init() {
    for (int i = 0; i < 256; ++i) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    install_exception(0, isr0);
    install_exception(1, isr1);
    install_exception(2, isr2);
    install_exception(3, isr3);
    install_exception(4, isr4);
    install_exception(5, isr5);
    install_exception(6, isr6);
    install_exception(7, isr7);
    install_exception(8, isr8);
    install_exception(9, isr9);
    install_exception(10, isr10);
    install_exception(11, isr11);
    install_exception(12, isr12);
    install_exception(13, isr13);
    install_exception(14, isr14);
    install_exception(15, isr15);
    install_exception(16, isr16);
    install_exception(17, isr17);
    install_exception(18, isr18);
    install_exception(19, isr19);
    install_exception(20, isr20);
    install_exception(21, isr21);
    install_exception(22, isr22);
    install_exception(23, isr23);
    install_exception(24, isr24);
    install_exception(25, isr25);
    install_exception(26, isr26);
    install_exception(27, isr27);
    install_exception(28, isr28);
    install_exception(29, isr29);
    install_exception(30, isr30);
    install_exception(31, isr31);

    // Hardware IRQs after PIC remapping.
    idt_set_gate(
        32,
        reinterpret_cast<uintptr_t>(irq0),
        kernel_code_selector()
    );

    idt_set_gate(
        33,
        reinterpret_cast<uintptr_t>(irq1),
        kernel_code_selector()
    );

    idt_set_gate(
        YIELD_VECTOR,
        reinterpret_cast<uintptr_t>(isr_yield),
        kernel_code_selector()
    );

    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base =
        reinterpret_cast<uintptr_t>(&idt[0]);

    asm volatile(
        "lidtl %0"
        :
        : "m"(idt_pointer)
    );

    serial_write("[INT ] real IDT installed\n");
    serial_write("[INT ] CPU exception handlers online\n");
}

extern "C" uint32_t interrupt_handler(InterruptFrame* frame) {
    if (frame == nullptr) {
        serial_write("[INT ] null interrupt frame\n");

        while (true) {
            asm volatile("cli; hlt");
        }
    }

    if (frame->interrupt_number < IRQ_BASE_VECTOR) {
        serial_write("[EXC ] CPU exception\n");

        while (true) {
            asm volatile("cli; hlt");
        }
    }

    uint32_t currentEsp = reinterpret_cast<uint32_t>(frame);

    // The software yield vector carries no PIC-owned hardware interrupt
    // to acknowledge; it is pure scheduling policy.
    if (frame->interrupt_number == YIELD_VECTOR) {
        return scheduler_on_yield(currentEsp);
    }

    if (frame->interrupt_number >= IRQ_BASE_VECTOR &&
        frame->interrupt_number < IRQ_BASE_VECTOR + 16) {

        uint8_t irq =
            static_cast<uint8_t>(
                frame->interrupt_number - IRQ_BASE_VECTOR
            );

        uint32_t nextEsp = currentEsp;

        if (frame->interrupt_number == TIMER_VECTOR) {
            pit_tick();
            nextEsp = scheduler_on_timer_tick(currentEsp);
        } else if (frame->interrupt_number == KEYBOARD_VECTOR) {
            keyboard_on_irq();
        }

        // Hardware acknowledgement is intentionally separate from, and
        // always runs regardless of, whatever the scheduler decided
        // above — the PIC must be told this IRQ is handled whether or
        // not a task switch happened.
        pic_send_eoi(irq);

        return nextEsp;
    }

    return currentEsp;
}

void interrupts_enable() {
    asm volatile("sti");
}

void interrupts_disable() {
    asm volatile("cli");
}
