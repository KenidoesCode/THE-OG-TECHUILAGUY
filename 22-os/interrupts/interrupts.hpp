#pragma once

#include <stdint.h>

// Hardware IRQs are remapped to start at vector 32 (see interrupts_init).
inline constexpr uint32_t IRQ_BASE_VECTOR = 32;
inline constexpr uint32_t TIMER_VECTOR = IRQ_BASE_VECTOR + 0;

// Software interrupt tasks use to voluntarily reschedule.
inline constexpr uint32_t YIELD_VECTOR = 129;

struct InterruptFrame {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t interrupt_number;
    uint32_t error_code;

    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;

    uint32_t useresp;
    uint32_t ss;
};

void interrupts_init();
void interrupts_enable();
void interrupts_disable();

// Returns the (possibly different) kernel stack pointer to resume from.
// isr_common loads this into %esp before restoring registers, which is
// the entire context-switch mechanism the scheduler relies on: every
// task's saved state is a trap frame sitting on its own kernel stack.
extern "C" uint32_t interrupt_handler(InterruptFrame* frame);
