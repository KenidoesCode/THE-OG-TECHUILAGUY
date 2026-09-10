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
 * IRQ0
 *
 * Diagnostic handler.
 *
 * No C++.
 * No scheduler.
 * No IRET.
 *
 * Once IRQ0 arrives, mask it, acknowledge it, then halt.
 * --------------------------------------------------------- */

.global irq0
.type irq0, @function
irq0:
    cli

    /*
     * Mask IRQ0.
     */
    inb $0x21, %al
    orb $0x01, %al
    outb %al, $0x21

    /*
     * EOI to master PIC.
     */
    movb $0x20, %al
    outb %al, $0x20

    /*
     * Write '!' to COM1.
     * DX is required because 0x3F8 does not fit
     * in the short immediate-port encoding.
     */
    movw $0x3F8, %dx
    movb $'!', %al
    outb %al, %dx

irq0_hang:
    hlt
    jmp irq0_hang


/* ---------------------------------------------------------
 * IRQ1
 * --------------------------------------------------------- */

.global irq1
.type irq1, @function
irq1:
    cli

    movb $0x20, %al
    outb %al, $0x20

    iret
