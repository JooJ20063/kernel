.section .data
.align 8

.global idt_descriptor
idt_descriptor:
	.word 0 	# limit preenchido pelo C
	.long 0		# base

.section .note.GNU-stack,"",@progbits
