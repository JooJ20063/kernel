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

.extern kernel_main
.extern gdt_descriptor


_start:
    cli

    #load GDT
    lgdt gdt_descriptor

    #reload data segments (selector 0x10)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    #faar jump to reload CS (selector 0x08)
    ljmp $0x08, $flush_cs

flush_cs:
    mov $stack_top, %esp

    push %ebx               # multiboot2 info pointer
    call kernel_main
    add $4, %esp

hang:
    hlt
    jmp hang


.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
