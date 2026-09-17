/*
 * Links a userland program starting at address 0. The resulting ELF is
 * never loaded by an ELF loader (this kernel has none yet) — it exists
 * only so objcopy can extract a flat binary of raw machine code, which
 * the scheduler then copies verbatim into an allocated physical page
 * and runs from wherever that page happens to be (see
 * scheduler_create_user_task). Starting at 0 here is just a convention
 * for computing the flat binary's own internal layout; it plays no
 * role in where the code actually ends up executing.
 */

ENTRY(_start)

SECTIONS
{
    . = 0;

    .text : {
        *(.text)
    }
}
