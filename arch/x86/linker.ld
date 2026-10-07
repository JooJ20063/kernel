ENTRY(_start)

SECTIONS
{
    . = 1M;
    _kernel_start = .;

    .text ALIGN(4K) : {
        _text_start = .;
        *(.multiboot)
        *(.text*)
        _text_end = .;
    }

    .rodata ALIGN(4K) : {
        _rodata_start = .;
        *(.rodata*)
        _rodata_end = .;
    }

    .usertext ALIGN(4K) : {
        _user_text_start = .;
        *(.usertext)
        _user_text_end = .;
    }

    .userdata ALIGN(4K) : {
        _user_data_start = .;
        *(.userdata)
        _user_data_end = .;
    }

    .data ALIGN(4K) : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    }

    .bss ALIGN(4K) : {
        _bss_start = .;
        *(COMMON)
        *(.bss*)
        _bss_end = .;
    }

    _kernel_end = .;
}
