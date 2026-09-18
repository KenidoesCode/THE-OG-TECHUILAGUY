/*
 * A real ELF32/i386 ET_EXEC program with a single read+execute-only
 * PT_LOAD segment (see userland/elf_write_to_code.ld — no PF_W flag).
 * Deliberately attempts to write into its own code segment, which the
 * loader mapped without the page-table W bit set (see
 * scheduler_create_elf_user_task's use of segment.writable). This must
 * fault with #PF and kill only this task, exactly like the existing
 * evil.S (privileged instruction) and kernel_peek.S/neighbor_peek.S
 * (reading unmapped memory) negative tests — the concrete proof that
 * an ELF segment's permissions are genuinely enforced by the CPU, not
 * merely recorded and ignored by the loader.
 */
.section .text
.global _start

_start:
    movl $1, %eax
    movl $'W', %ebx
    int $0x80              /* proves the task reached the kernel first */

    movb $0x90, _start     /* attempt to write into the code segment itself */

    /* Unreachable if the write above faults, as it must. */
    movl $1, %eax
    movl $'Q', %ebx
    int $0x80

hang:
    jmp hang

.section .note.GNU-stack,"",@progbits
