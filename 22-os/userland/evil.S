/*
 * Negative test for privilege isolation. Writes "E" via a legitimate
 * syscall, then executes `cli` directly — a privileged instruction
 * that requires CPL <= IOPL (kernel-only here) — which must fault with
 * a general-protection exception (vector 13) rather than actually
 * disabling interrupts. If the fault handler's ring-3 isolation logic
 * works, this task is killed and the "X" below is never written; the
 * kernel and every other task must keep running regardless.
 */

.section .text
.global _start

_start:
    movl $1, %eax    /* SYS_WRITE */
    movl $'E', %ebx
    int $0x80

    cli              /* privileged instruction at CPL 3: must fault */

    /* Unreachable if isolation actually works. */
    movl $1, %eax
    movl $'X', %ebx
    int $0x80

hang:
    jmp hang

.section .note.GNU-stack,"",@progbits
