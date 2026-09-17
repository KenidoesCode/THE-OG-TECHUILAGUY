/*
 * Smallest real ring-3 userspace program: writes "U" via a syscall,
 * deliberately issues an unrecognized syscall number (99) to prove the
 * kernel returns ENOSYS and keeps running the task rather than
 * crashing it, then writes "!\n" and exits cleanly via SYS_EXIT.
 *
 * Position-independent by construction (no absolute data references,
 * only immediates and a local relative jump), since it is copied
 * wholesale into whichever physical page the scheduler allocates for
 * it — see scheduler_create_user_task.
 */

.section .text
.global _start

_start:
    movl $1, %eax        /* SYS_WRITE */
    movl $'U', %ebx
    int $0x80

    movl $99, %eax       /* deliberately invalid syscall number */
    int $0x80

    movl $1, %eax
    movl $'!', %ebx
    int $0x80

    movl $1, %eax
    movl $10, %ebx       /* '\n' */
    int $0x80

    movl $0, %eax        /* SYS_EXIT */
    int $0x80

hang:
    jmp hang

.section .note.GNU-stack,"",@progbits
