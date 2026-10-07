.section .text
.global _start
.type _start, @function
.extern main

_start:
    # ABI v1 guarantees a 16-byte aligned process-entry stack.
    test $0xF, %esp
    jz 1f

    mov $127, %ebx
    mov $2, %eax
    int $0x80

1:
    # Initial userspace stack:
    #   argc
    #   argv[0..argc-1]
    #   NULL
    #   envp[0..]
    #   NULL
    mov %esp, %eax
    mov (%eax), %ecx
    lea 4(%eax), %edx
    lea 4(%edx,%ecx,4), %ebx

    push %ebx
    push %edx
    push %ecx
    call main
    add $12, %esp

    # main() return value becomes the process exit status.
    mov %eax, %ebx
    mov $2, %eax
    int $0x80

2:
    pause
    jmp 2b

.size _start, .-_start
.section .note.GNU-stack,"",@progbits
