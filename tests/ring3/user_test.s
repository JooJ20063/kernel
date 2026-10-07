.section .usertext, "ax", @progbits
.global user_test_entry

user_test_entry:
    # write("Hello from ring 3\n")
    mov $1, %eax
    mov $1, %ebx
    mov $user_message, %ecx
    mov $(user_message_end-user_message), %edx
    int $0x80

    # Attempt to pass a kernel-only address to write().
    # The kernel must reject it with -1 instead of faulting.
    mov $1, %eax
    mov $1, %ebx
    mov $0x00100000, %ecx
    mov $4, %edx
    int $0x80

    cmp $0xFFFFFFFF, %eax
    jne uaccess_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $uaccess_ok, %ecx
    mov $(uaccess_ok_end-uaccess_ok), %edx
    int $0x80
    jmp uaccess_test_done

uaccess_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $uaccess_fail, %ecx
    mov $(uaccess_fail_end-uaccess_fail), %edx
    int $0x80

uaccess_test_done:

    # getpid()
    mov $3, %eax
    int $0x80

    # EAX = PID
    add $'0', %al
    mov %al, user_pid_digit

    # write("pid=")
    mov $1, %eax
    mov $1, %ebx
    mov $pid_message, %ecx
    mov $(pid_message_end-pid_message), %edx
    int $0x80

    # write("Before sleep\n")
    mov $1, %eax
    mov $1, %ebx
    mov $before_sleep, %ecx
    mov $(before_sleep_end-before_sleep), %edx
    int $0x80

    # sleep(300)
    mov $6, %eax
    mov $300, %ebx
    int $0x80

    # write("After sleep\n")
    mov $1, %eax
    mov $1, %ebx
    mov $after_sleep, %ecx
    mov $(after_sleep_end-after_sleep), %edx
    int $0x80

    # write "before yield\n"
    mov $1, %eax
    mov $1, %ebx
    mov $before_yield, %ecx
    mov $(before_yield_end-before_yield), %edx
    int $0x80

    # yield
    mov $4, %eax
    int $0x80

    # write "after yield\n"
    mov $1, %eax
    mov $1, %ebx
    mov $after_yield, %ecx
    mov $(after_yield_end-after_yield), %edx
    int $0x80

    # getppid()
    mov $5, %eax
    int $0x80

    # EAX = PPID
    add $'0', %al
    mov %al, user_ppid_digit

    #write("ppid=")
    mov $1, %eax
    mov $1, %ebx
    mov $ppid_message, %ecx
    mov $(ppid_message_end-ppid_message), %edx
    int $0x80

    # exit(42)
    mov $2, %eax
    mov $42, %ebx
    int $0x80

1:
    jmp 1b


.global user_ud_test_entry
user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
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

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
