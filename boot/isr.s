.section .text

#macro para exceções sem erro automático
.macro ISR_NOERR n
.global isr\n
isr\n:
    cli
    pushl $0            #erro falso
    pushl $\n           #número da exceção
    jmp isr_common_stub
.endm

#macro para exceções com erro automático
.macro ISR_ERR n
.global isr\n
isr\n:
    cli
    pushl $\n           #numero da exceção
    jmp isr_common_stub
.endm

# 0-31 (x86)
ISR_NOERR   0 #Divide by 0
ISR_NOERR   1
ISR_NOERR   2
ISR_NOERR   3 #Breakpoint
ISR_NOERR   4
ISR_NOERR   5
ISR_NOERR   6
ISR_NOERR   7
ISR_ERR     8 #Double fault
ISR_NOERR   9
ISR_ERR     10
ISR_ERR     11
ISR_ERR     12
ISR_ERR     13
ISR_ERR     14
ISR_NOERR   15
ISR_NOERR   16
ISR_NOERR   17
ISR_NOERR   18
ISR_NOERR   19
ISR_NOERR   20
ISR_NOERR   21
ISR_NOERR   22
ISR_NOERR   23
ISR_NOERR   24
ISR_NOERR   25
ISR_NOERR   26
ISR_NOERR   27
ISR_NOERR   28
ISR_NOERR   29
ISR_NOERR   30
ISR_NOERR   31

.global isr_default
isr_default:
	jmp isr0

.extern isr_handler_c

isr_common_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10,  %ax
    mov %ax,    %ds
    mov %ax,    %es
    mov %ax,    %fs
    mov %ax,    %gs

    push %esp
    call isr_handler_c
    add $4, %esp

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa

    add $8, %esp
    iret

.section .note.GNU-stack,"",@progbits
