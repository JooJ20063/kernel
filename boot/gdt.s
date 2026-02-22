.section .data
.align 8

gdt_start:
    .quad 0x0000000000000000 #NULL

    # Code segment: base=0, limit=4GB, RX
    .quad 0x00CF9A000000FFFF

    #Data segment: base=0, limit=4GB, RW
    .quad 0x00CF92000000FFFF

gdt_end:
.global gdt_descriptor
gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start
