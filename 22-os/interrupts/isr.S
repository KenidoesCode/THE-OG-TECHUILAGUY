.section .text

.extern interrupt_handler

.macro ISR_NO_ERROR num
.global isr\num
.type isr\num, @function
isr\num:
    pushl $0
    pushl $\num
    jmp isr_common
.endm

.macro ISR_ERROR num
.global isr\num
.type isr\num, @function
isr\num:
    pushl $\num
    jmp isr_common
.endm


/* CPU exceptions 0-31 */

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_ERROR    8
ISR_NO_ERROR 9
ISR_ERROR    10
ISR_ERROR    11
ISR_ERROR    12
ISR_ERROR    13
ISR_ERROR    14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_ERROR    17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_NO_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_NO_ERROR 29
ISR_ERROR    30
ISR_NO_ERROR 31


/* ---------------------------------------------------------
 * CPU exception common path
 * --------------------------------------------------------- */

.type isr_common, @function
isr_common:
    pusha

    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    /*
     * Kernel data selector.
     */
    movw $0x18, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    pushl %esp
    call interrupt_handler
    addl $4, %esp

    /*
     * interrupt_handler returns the (possibly different) kernel
     * stack pointer to resume execution from. On every path except
     * a scheduler-driven task switch this is simply the frame we
     * were already on; when the scheduler decides to switch tasks,
     * it is a different task's previously saved frame, and loading
     * it here is the entire context-switch mechanism — every task's
     * saved register state lives on its own stack in exactly this
     * trap-frame layout, so resuming it is just a normal interrupt
     * return.
     */
    movl %eax, %esp

    popl %gs
    popl %fs
    popl %es
    popl %ds

    popa

    /*
     * Remove:
     *   interrupt number
     *   error code
     */
    addl $8, %esp

    iret


/* ---------------------------------------------------------
 * IRQ0 (PIT timer) and IRQ1 (keyboard).
 *
 * Hardware IRQs after PIC remapping share the exact same trap
 * frame and dispatch path as CPU exceptions (isr_common): push a
 * dummy error code and the interrupt vector, then fall into the
 * common handler. This is what lets interrupt_handler's generic
 * IRQ range check EOI them, and lets vector 32 (the timer) drive
 * the scheduler through the same return-a-new-esp mechanism used
 * by the software yield vector below — hardware acknowledgement
 * stays entirely in C (interrupt_handler / pic_send_eoi); this
 * file only ever transports a trap frame.
 * --------------------------------------------------------- */

ISR_NO_ERROR 32
ISR_NO_ERROR 33

.global irq0
.type irq0, @function
irq0 = isr32

.global irq1
.type irq1, @function
irq1 = isr33


/* ---------------------------------------------------------
 * Software "yield" interrupt.
 *
 * Tasks request a voluntary reschedule with `int $0x81` rather
 * than a hand-rolled context-switch routine, so voluntary yields
 * reuse the exact same trap-frame save/restore path as timer
 * preemption instead of a second, separately-maintained mechanism.
 * --------------------------------------------------------- */

ISR_NO_ERROR 129

.global isr_yield
.type isr_yield, @function
isr_yield = isr129


.section .note.GNU-stack,"",@progbits
