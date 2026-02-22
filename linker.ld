ENTRY(_start)

SECTIONS
{
    /* Kernel começa em 1MB */
    . = 1M;

    .text ALIGN(4K) : {
        *(.multiboot)
        *(.text*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss*)
    }
}
