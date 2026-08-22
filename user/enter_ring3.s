.section .text
.global enter_ring3

enter_ring3:
    # cdecl:
    # 4($esp) = entry
    # 8($esp) = user_stack

    mov 4(%esp), %eax
    mov 8(%esp), %edx

    # User Data segments
    mov $0x23, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs

    # iret frame chance CPL0 -> CPL3

    pushl $0x23         # SS
    pushl %edx          # User ESP

    pushfl
    popl %ecx

    andl $~0x200, %ecx

    pushl %ecx          # EFLAGS
    pushl $0x1B         # CS
    pushl %eax          # User EIP

    iret