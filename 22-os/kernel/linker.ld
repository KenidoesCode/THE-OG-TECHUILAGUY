ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R + X, no W */
    rodata PT_LOAD FLAGS(4); /* R only */
    data PT_LOAD FLAGS(6);   /* R + W, no X */
}

SECTIONS
{
    . = 1M;

    .text : {
        *(.multiboot)
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :rodata

    .data : {
        *(.data*)
    } :data

    .bss : {
        *(COMMON)
        *(.bss*)
    } :data

    _kernel_end = .;
}
