/*
 * Negative test for real per-process address-space isolation
 * (distinct from userland/kernel_peek.S, which tests the *shared*
 * kernel region, and userland/evil.S, which tests instruction
 * privilege rather than memory).
 *
 * Every user task's private region is two pages at a fixed virtual
 * base (see paging.hpp: USER_CODE_VADDR, USER_STACK_PAGE_VADDR) —
 * identical addresses for every task, backed by that task's own
 * distinct physical pages. This program writes "N" via a legitimate
 * syscall, then reads the virtual address immediately past its own
 * two granted pages (USER_CODE_VADDR + 2 pages). That address is
 * never mapped for *any* task — not this one, not another one sharing
 * the same nominal virtual layout — so the read must fault with #PF
 * (page fault, vector 14) rather than succeed. This is the honest form
 * "one process cannot read another's memory" takes under a design
 * where processes cannot even *name* another process's data (every
 * task's own view of this virtual base always shows its own memory):
 * reaching anything beyond what was explicitly mapped for the
 * currently running task must fail, structurally, not by permission
 * check alone.
 */

.section .text
.global _start

_start:
    movl $1, %eax    /* SYS_WRITE */
    movl $'N', %ebx
    int $0x80

    movl $0x01002000, %eax   /* USER_CODE_VADDR + 2 pages: never mapped */
    movl (%eax), %ebx        /* must #PF */

    /* Unreachable if address-space isolation actually works. */
    movl $1, %eax
    movl $'Z', %ebx
    int $0x80

hang:
    jmp hang

.section .note.GNU-stack,"",@progbits
