.section .data
.align 8

gdt_start:
    .quad 0x0000000000000000 #NULL

    # 0x08 - Kernel code: base=0, limit=4GB, RX, DPL=0
    .quad 0x00CF9A000000FFFF

    # 0x10 - Kernel data: base=0, limit=4GB, RW, DPL=0
    .quad 0x00CF92000000FFFF

    # 0x18 - User code: base=0, limit=4GB, RX, DPL=3
    .quad 0x00CFFA000000FFFF

    # 0x20 - User data: base=0, limit=4GB, RW, DPL=3
    .quad 0x00CFF2000000FFFF

.global gdt_tss
gdt_tss:
    .quad 0x0000000000000000
gdt_end:
.global gdt_descriptor
gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start
