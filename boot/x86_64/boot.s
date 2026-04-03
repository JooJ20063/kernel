.set ALIGN,     1<<0
.set MEMINFO,   1<<1
.set FLAGS,     ALIGN | MEMINFO
.set MAGIC,     0xE85250D6
.set ARCH,      0
.set HEADER_LEN, header_end - header_start
.set CHECKSUM,  -(MAGIC + ARCH + HEADER_LEN)

.section .multiboot
.align 8
header_start:
.long MAGIC
.long ARCH
.long HEADER_LEN
.long CHECKSUM

header_tags_start:
    .short 0
    .short 0
    .long 8
header_end:

.section .text
.global _start
.extern kernel_main
.extern gdt_descriptor

    .code32
_start:
    cli
    mov $stack_top, %esp

    # Load GDT
    lgdt gdt_descriptor

    # Enable PAE
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    # Set up page tables (identity map 0-1GB, including 0xC00000 heap)
    # PDPT[0] = 1GB page, present RW, PS
    mov $0x87, %eax  # 1GB page
    mov %eax, pdpt

    # PML4[0] = pdpt, present RW
    mov $pdpt, %eax
    or $0x3, %eax
    mov %eax, pml4

    # Load CR3
    mov $pml4, %eax
    mov %eax, %cr3

    # Enable long mode (EFER, MSR 0xC0000080)
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    xor %edx, %edx
    wrmsr

    # Enable paging and protected mode
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

    # Jump to 64-bit code via far jump
    ljmp $0x18, $start64

    .align 16
start64:
    .code64
    mov $stack_top, %rsp
    call kernel_main
    hlt
    jmp start64

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096
