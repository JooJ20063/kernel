.section .data
.align 8

.global idt_descriptor
idr_descriptor:
	.word 0 	#limit preenchido pelo C
	.long 0		#base
