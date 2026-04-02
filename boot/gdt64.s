.section .data
.align 8

gdt64_start:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF   # 32-bit code
    .quad 0x00CF92000000FFFF   # 32-bit data
    .quad 0x00209A0000000000   # 64-bit code
    .quad 0x00A0920000000000   # 64-bit data

gdt64_end:
.global gdt64_descriptor
gdt64_descriptor:
    .word gdt64_end - gdt64_start - 1
    .quad gdt64_start
