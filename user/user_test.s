.section .usertext, "ax", @progbits
.global user_test_entry

user_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $user_message, %ecx
    mov $(user_message_end - user_message), %edx
    int $0x80

1:
    jmp 1b

.section .userdata, "aw", @progbits
.align 16

user_stack_bottom:
    .skip 4096
.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end: