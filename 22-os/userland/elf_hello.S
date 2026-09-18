/*
 * A real ELF32/i386 ET_EXEC program (see userland/elf_user.ld) — the
 * kernel's own ELF loader (elf/elf.hpp, scheduler_create_elf_user_task)
 * parses this file's actual ELF header and program headers and maps
 * its two PT_LOAD segments itself, rather than the kernel copying a
 * pre-flattened blob into one page the way the other userland/*.S
 * programs do. Reads a byte out of its own .data segment (proving
 * that segment is really mapped and readable, distinct from .text),
 * writes to it (proving it's really writable), and reads it back
 * (proving the write landed in real memory, not silently discarded).
 */
.section .text
.global _start

_start:
    movb value, %cl
    movl $1, %eax        /* SYS_WRITE */
    movzbl %cl, %ebx
    int $0x80            /* prints 'D' — the data segment's initial value */

    movb $'G', value
    movb value, %cl
    movl $1, %eax
    movzbl %cl, %ebx
    int $0x80            /* prints 'G' — proves the write to .data stuck */

    movl $0, %eax        /* SYS_EXIT */
    int $0x80

hang:
    jmp hang

.section .data
value:
    .byte 'D'

.section .note.GNU-stack,"",@progbits
