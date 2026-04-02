.set ALIGN,     1<<0
.set MEMINFO,   1<<1
.set FLAGS,     ALIGN | MEMINFO
.set MAGIC,     0xE85250D6
.set ARCH,      0
.set HEADER_LEN, header_end - header_start
.set CHECKSUM,  -(MAGIC + ARCH + HEADER_LEN)

.section .multiboot
.align 8
.long MAGIC
.long ARCH
.long HEADER_LEN
.long CHECKSUM

header_start:
    .short 0
    .short 0
    .long 8
header_end:

.section .text
.global _start
.extern kernel_main64

_start:
    cli
    mov $stack_top, %esp
    call kernel_main64
    hlt
    jmp _start

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
