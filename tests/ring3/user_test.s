.section .usertext, "ax", @progbits
.global user_test_entry

user_test_entry:
    # write("Hello from ring 3\n")
    mov $1, %eax
    mov $1, %ebx
    mov $user_message, %ecx
    mov $(user_message_end-user_message), %edx
    int $0x80

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


.section .userdata, "aw", @progbits
.align 16

user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

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

