/*
 * Negative test for paging-based memory isolation. Writes "K" via a
 * legitimate syscall, then directly reads a hardcoded kernel-only
 * physical address (1 MiB — the kernel's own load address; see
 * linker.ld's ". = 1M") that this task was never granted access to.
 *
 * With paging enabled and every page supervisor-only by default
 * except the two (code, stack) explicitly granted to this task, that
 * read must fault with #PF (page fault, vector 14) rather than
 * succeeding — proving memory isolation is real, not just instruction
 * privilege isolation (which userland/evil.S already covers). If
 * isolation actually works, this task is killed and "L" below is
 * never printed; the kernel and every other task must keep running
 * regardless.
 */

.section .text
.global _start

_start:
    movl $1, %eax    /* SYS_WRITE */
    movl $'K', %ebx
    int $0x80

    movl $0x100000, %eax
    movl (%eax), %ebx   /* read the kernel's own code: must #PF */

    /* Unreachable if isolation actually works. */
    movl $1, %eax
    movl $'L', %ebx
    int $0x80

hang:
    jmp hang

.section .note.GNU-stack,"",@progbits
