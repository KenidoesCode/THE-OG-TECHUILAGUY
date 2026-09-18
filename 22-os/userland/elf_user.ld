/*
 * A real ELF32/i386 ET_EXEC linker script — unlike user.ld, this one
 * is meant to produce a genuine ELF program the kernel's own loader
 * (elf/elf.hpp, scheduler_create_elf_user_task) parses and maps
 * itself, not a flat binary copied verbatim into one page. Two
 * explicit PT_LOAD segments with different permissions, laid out at
 * the exact fixed virtual base the loader's v1 window expects
 * (ELF_SEGMENT_VIRTUAL_BASE in elf/elf.hpp) — text is read+execute
 * only (FLAGS(5) = PF_R|PF_X), data is read+write only
 * (FLAGS(6) = PF_R|PF_W), each page-aligned so the loader's
 * page-alignment requirement on every segment's virtual address is
 * satisfied without the loader needing to handle sub-page offsets.
 */

ENTRY(_start)

PHDRS
{
    code PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x01000000;

    .text : {
        *(.text)
    } :code

    . = ALIGN(4096);

    .data : {
        *(.data)
    } :data

    .bss : {
        *(.bss) *(COMMON)
    } :data
}
