.section .text

.extern irq_handler_c

#Macro IRQ (0-15)
.macro IRQ n
.global irq\n
irq\n:
	cli
	pushl $0		#erro falso
	pushl $(32 + \n)	#vetor da IRQ
	jmp irq_common_stub
.endm

IRQ 0	#Timer
IRQ 1	#Keyboard
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

irq_common_stub:
	pusha
	push %ds
	push %es
	push %fs
	push %gs
	
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	
	push %esp
	call irq_handler_c
	add $4, %esp
	
	pop %gs
	pop %fs
	pop %es
	pop %ds
	popa
	
	add $8, %esp
	iret

.section .note.GNU-stack,"",@progbits
