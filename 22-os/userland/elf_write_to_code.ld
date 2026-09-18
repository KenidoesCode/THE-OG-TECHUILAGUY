/*
 * A single read+execute-only PT_LOAD segment (FLAGS(5) = PF_R|PF_X,
 * no PF_W) — used by elf_write_to_code.S to prove the loader's
 * per-segment write permission is genuinely enforced by the CPU, not
 * just recorded and ignored: a write into this segment must fault.
 */

ENTRY(_start)

PHDRS
{
    code PT_LOAD FLAGS(5);
}

SECTIONS
{
    . = 0x01000000;

    .text : {
        *(.text)
    } :code
}
