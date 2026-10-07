ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
}

SECTIONS
{
    . = 0x02000000;

    .text ALIGN(0x1000) : {
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
    }
}
