.section .text
.global _start
.type _start, @function
.extern main

_start:
    # Ponto de entrada do executável em Ring 3.
    # O carregador prepara a pilha inicial com palavras de 32 bits:
    #   argc
    #   argv[0..argc-1]
    #   NULL
    #   envp[0..]
    #   NULL

    # Preserva o endereço inicial da pilha e obtém argc.
    mov %esp, %eax
    mov (%eax), %ecx

    # argv começa imediatamente após argc.
    lea 4(%eax), %edx

    # envp começa após os ponteiros de argv e seu terminador NULL.
    # Cada ponteiro ocupa 4 bytes na ABI x86.
    lea 4(%edx,%ecx,4), %ebx

    # Chama main(argc, argv, envp) seguindo a convenção cdecl:
    # argumentos empilhados da direita para a esquerda.
    push %ebx
    push %edx
    push %ecx
    call main

    # Na convenção cdecl, o chamador remove os argumentos da pilha.
    add $12, %esp

    # Converte o retorno de main(), recebido em EAX, no status de saída.
    # ABI de syscall do CZK: EAX = número da chamada; EBX = argumento 1.
    mov %eax, %ebx
    mov $2, %eax             # SYS_EXIT = 2
    int $0x80

1:
    # Fallback caso a syscall de saída retorne inesperadamente.
    # PAUSE não bloqueia a tarefa; permanece em um loop de espera ativa.
    pause
    jmp 1b

.size _start, .-_start

# Marca este objeto como não exigindo uma pilha executável.
.section .note.GNU-stack,"",@progbits