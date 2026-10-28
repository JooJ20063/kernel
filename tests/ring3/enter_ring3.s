.section .text
.global enter_ring3

enter_ring3:
    # Transfere a execução de Ring 0 para Ring 3.
    # Argumentos recebidos pela convenção cdecl:
    #   4(%esp) = endereço de entrada do código de usuário
    #   8(%esp) = ponteiro inicial da pilha de usuário
    #
    # Requer GDT, paginação e TSS previamente configuradas.
    # Não prepara um retorno convencional ao chamador.

    mov 4(%esp), %eax
    mov 8(%esp), %edx

    # Carrega os seletores de dados de usuário.
    # 0x23 = entrada 0x20 da GDT com RPL = 3.
    mov $0x23, %cx
    mov %cx, %ds
    mov %cx, %es
    mov %cx, %fs
    mov %cx, %gs

    # Constrói o frame consumido por IRET na mudança de privilégio.
    # Ordem de empilhamento: SS, ESP, EFLAGS, CS e EIP.
    pushl $0x23         # Seletor da pilha de usuário
    pushl %edx          # ESP inicial de usuário

    # Obtém as flags atuais e limpa IF (bit 9).
    # O código de usuário iniciará com interrupções mascaráveis desabilitadas.
    pushfl
    popl %ecx
    andl $~0x200, %ecx

    pushl %ecx          # EFLAGS para execução em Ring 3
    pushl $0x1B         # Código de usuário: entrada 0x18 com RPL = 3
    pushl %eax          # EIP inicial de usuário

    # Restaura o contexto preparado e realiza a transição CPL0 -> CPL3.
    iret

# Marca este objeto como não exigindo uma pilha executável.
.section .note.GNU-stack,"",@progbits