.section .data
.align 8

gdt_start:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF   # 32-bit code
    .quad 0x00CF92000000FFFF   # 32-bit data
    .quad 0x00209A0000000000   # 64-bit code
    .quad 0x00A0920000000000   # 64-bit data

gdt_end:
.global gdt_descriptor
gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .quad gdt_start
