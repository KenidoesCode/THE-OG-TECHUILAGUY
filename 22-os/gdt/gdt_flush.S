.section .text

/* ---------------------------------------------------------
 * gdt_flush(uint32_t gdt_pointer_address)
 *
 * Loads the GDTR, then reloads every segment register to point at
 * this kernel's own descriptors. CS can only be reloaded via a
 * control-transfer instruction (a far jump here), never a plain mov —
 * that's what actually takes effect the new code descriptor; without
 * it the CPU keeps using whatever CS the bootloader left active.
 * --------------------------------------------------------- */

.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    movl 4(%esp), %eax
    lgdt (%eax)

    movw $0x10, %ax      /* KERNEL_DATA_SELECTOR */
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    ljmp $0x08, $.Lgdt_flush_reloaded   /* KERNEL_CODE_SELECTOR */

.Lgdt_flush_reloaded:
    ret


/* ---------------------------------------------------------
 * tss_flush()
 *
 * Loads the task register with the TSS selector, which is what makes
 * the CPU actually consult that TSS's ss0:esp0 on a ring3 -> ring0
 * transition.
 * --------------------------------------------------------- */

.global tss_flush
.type tss_flush, @function
tss_flush:
    movw $0x28, %ax       /* TSS_SELECTOR */
    ltr %ax
    ret


.section .note.GNU-stack,"",@progbits
