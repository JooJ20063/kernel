# Código de testes de usuário vinculado à imagem do kernel.
.section .usertext, "ax", @progbits

# Códigos positivos de erro; as syscalls retornam seus valores negativos.
.set CZK_EBADF, 9
.set CZK_ECHILD, 10
.set CZK_EFAULT, 14
.set CZK_ENOSYS, 38
.set CZK_ESPIPE, 29

# Flags de abertura de arquivos.
.set CZK_O_RDONLY, 0x0000
.set CZK_O_RDWR,   0x0002
.set CZK_O_CREAT,  0x0040

# Referência de deslocamento: início do arquivo.
.set CZK_SEEK_SET, 0

# ABI de syscall x86:
#   EAX = número da chamada e valor de retorno
#   EBX = argumento 1
#   ECX = argumento 2
#   EDX = argumento 3
#
# Chamadas utilizadas:
#   1=write, 2=exit, 3=getpid, 4=yield, 5=getppid, 6=sleep,
#   7=wait, 8=open, 9=read, 10=close, 11=lseek, 12=fstat, 13=readdir.

.global user_test_entry
user_test_entry:
    # Emite a mensagem inicial em stdout.
    mov $1, %eax
    mov $1, %ebx
    mov $user_message, %ecx
    mov $(user_message_end-user_message), %edx
    int $0x80

    # Tenta escrever usando um endereço reservado ao kernel.
    # A chamada deve retornar -EFAULT sem provocar falha no kernel.
    mov $1, %eax
    mov $1, %ebx
    mov $0x00100000, %ecx
    mov $4, %edx
    int $0x80

    cmp $-CZK_EFAULT, %eax
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
    # Um descritor inválido deve produzir -EBADF.
    mov $1, %eax
    mov $99, %ebx
    mov $user_message, %ecx
    mov $1, %edx
    int $0x80

    cmp $-CZK_EBADF, %eax
    jne abi_error_test_failed

    # Sem processos filhos, wait(NULL) deve retornar -ECHILD.
    mov $7, %eax
    xor %ebx, %ebx
    int $0x80

    cmp $-CZK_ECHILD, %eax
    jne abi_error_test_failed

    # Uma syscall desconhecida deve retornar -ENOSYS.
    mov $0x7FFFFFFF, %eax
    int $0x80

    cmp $-CZK_ENOSYS, %eax
    jne abi_error_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $abi_error_ok, %ecx
    mov $(abi_error_ok_end-abi_error_ok), %edx
    int $0x80
    jmp abi_error_test_done

abi_error_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $abi_error_fail, %ecx
    mov $(abi_error_fail_end-abi_error_fail), %edx
    int $0x80

abi_error_test_done:
    # O descritor 2 representa stderr e deve aceitar escrita.
    mov $1, %eax
    mov $2, %ebx
    mov $fd_stderr_message, %ecx
    mov $(fd_stderr_message_end-fd_stderr_message), %edx
    int $0x80

    cmp $(fd_stderr_message_end-fd_stderr_message), %eax
    jne fd_table_test_failed

    # O descritor 0 representa stdin e deve rejeitar escrita.
    mov $1, %eax
    xor %ebx, %ebx
    mov $user_message, %ecx
    mov $1, %edx
    int $0x80

    cmp $-CZK_EBADF, %eax
    jne fd_table_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $fd_table_ok, %ecx
    mov $(fd_table_ok_end-fd_table_ok), %edx
    int $0x80
    jmp fd_table_test_done

fd_table_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $fd_table_fail, %ecx
    mov $(fd_table_fail_end-fd_table_fail), %edx
    int $0x80

fd_table_test_done:
    # Cria ou abre ring3.txt para leitura e escrita.
    mov $8, %eax
    mov $fileio_path, %ebx
    mov $(CZK_O_CREAT | CZK_O_RDWR), %ecx
    int $0x80

    # Os descritores 0, 1 e 2 já estão reservados para stdio.
    cmp $3, %eax
    jl fileio_test_failed

    # Mantém o descritor em EBX para as operações seguintes.
    mov %eax, %ebx
    mov $1, %eax
    mov $fileio_payload, %ecx
    mov $(fileio_payload_end-fileio_payload), %edx
    int $0x80

    cmp $(fileio_payload_end-fileio_payload), %eax
    jne fileio_test_failed

    # Fecha o arquivo após a escrita.
    mov $10, %eax
    int $0x80

    test %eax, %eax
    jne fileio_test_failed

    # Reabre o arquivo somente para leitura.
    mov $8, %eax
    mov $fileio_path, %ebx
    mov $CZK_O_RDONLY, %ecx
    int $0x80

    cmp $3, %eax
    jl fileio_test_failed

    # Consulta os metadados e verifica o tamanho registrado.
    # O primeiro campo da estrutura stat contém o tamanho do arquivo.
    mov %eax, %ebx
    mov $12, %eax
    mov $fileio_stat, %ecx
    int $0x80

    test %eax, %eax
    jne fileio_test_failed

    cmpl $(fileio_payload_end-fileio_payload), fileio_stat
    jne fileio_test_failed

    # Lê o conteúdo escrito anteriormente.
    mov $9, %eax
    mov $fileio_buffer, %ecx
    mov $(fileio_payload_end-fileio_payload), %edx
    int $0x80

    cmp $(fileio_payload_end-fileio_payload), %eax
    jne fileio_test_failed

    # Compara os bytes lidos do RAMFS com o conteúdo original.
    # CLD garante avanço dos ponteiros nas instruções de string.
    cld
    mov $fileio_payload, %esi
    mov $fileio_buffer, %edi
    mov $(fileio_payload_end-fileio_payload), %ecx
    repe cmpsb
    jne fileio_test_failed

    # Posiciona o cursor no byte 4 a partir do início do arquivo.
    mov $11, %eax
    mov $4, %ecx
    mov $CZK_SEEK_SET, %edx
    int $0x80

    cmp $4, %eax
    jne fileio_test_failed

    # Lê oito bytes a partir da nova posição.
    mov $9, %eax
    mov $fileio_seek_buffer, %ecx
    mov $8, %edx
    int $0x80

    cmp $8, %eax
    jne fileio_test_failed

    # Confere o trecho lido com os bytes correspondentes da origem.
    cld
    mov $(fileio_payload + 4), %esi
    mov $fileio_seek_buffer, %edi
    mov $8, %ecx
    repe cmpsb
    jne fileio_test_failed

    mov $10, %eax
    int $0x80

    test %eax, %eax
    jne fileio_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $fileio_ok, %ecx
    mov $(fileio_ok_end-fileio_ok), %edx
    int $0x80
    jmp fileio_test_done

fileio_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $fileio_fail, %ecx
    mov $(fileio_fail_end-fileio_fail), %edx
    int $0x80

fileio_test_done:
    # Abre o diretório raiz para enumeração.
    mov $8, %eax
    mov $root_path, %ebx
    mov $CZK_O_RDONLY, %ecx
    int $0x80

    cmp $3, %eax
    jl dirent_test_failed

    mov %eax, %ebx

dirent_scan_next:
    # Obtém a próxima entrada do diretório.
    mov $13, %eax
    mov $dirent_buffer, %ecx
    int $0x80

    # Chegar ao fim antes de encontrar ring3.txt representa falha.
    test %eax, %eax
    jz dirent_test_failed

    # O retorno 1 indica que uma entrada foi obtida.
    cmp $1, %eax
    jne dirent_test_failed

    # Compara o nome incluindo NUL: "ring3.txt" ocupa 10 bytes.
    # Outras entradas da raiz são ignoradas.
    cld
    mov $fileio_path, %esi
    mov $dirent_buffer, %edi
    mov $10, %ecx
    repe cmpsb
    jne dirent_scan_next

    # Layout esperado de dirent:
    #   bytes 0..127 = nome
    #   offset 128   = flags
    #   offset 132   = tamanho
    # Confirma FS_FILE e o tamanho do conteúdo.
    testl $1, dirent_buffer+128
    jz dirent_test_failed

    cmpl $(fileio_payload_end-fileio_payload), dirent_buffer+132
    jne dirent_test_failed

    # Fecha o descritor do diretório.
    mov $10, %eax
    int $0x80

    test %eax, %eax
    jne dirent_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $dirent_ok, %ecx
    mov $(dirent_ok_end-dirent_ok), %edx
    int $0x80
    jmp dirent_test_done

dirent_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $dirent_fail, %ecx
    mov $(dirent_fail_end-dirent_fail), %edx
    int $0x80

dirent_test_done:
    # stdout é um dispositivo sem suporte a reposicionamento.
    # lseek deve retornar -ESPIPE.
    mov $11, %eax
    mov $1, %ebx
    xor %ecx, %ecx
    mov $CZK_SEEK_SET, %edx
    int $0x80

    cmp $-CZK_ESPIPE, %eax
    jne seek_stdio_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $seek_stat_ok, %ecx
    mov $(seek_stat_ok_end-seek_stat_ok), %edx
    int $0x80
    jmp seek_stdio_test_done

seek_stdio_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $seek_stat_fail, %ecx
    mov $(seek_stat_fail_end-seek_stat_fail), %edx
    int $0x80

seek_stdio_test_done:
    # Obtém o PID do processo atual.
    mov $3, %eax
    int $0x80

    # Conversão simplificada: representa corretamente apenas PIDs de 0 a 9.
    add $'0', %al
    mov %al, user_pid_digit

    mov $1, %eax
    mov $1, %ebx
    mov $pid_message, %ecx
    mov $(pid_message_end-pid_message), %edx
    int $0x80

    # Informa que o teste solicitará suspensão.
    mov $1, %eax
    mov $1, %ebx
    mov $before_sleep, %ecx
    mov $(before_sleep_end-before_sleep), %edx
    int $0x80

    # Solicita suspensão por 300 ticks.
    mov $6, %eax
    mov $300, %ebx
    int $0x80

    mov $1, %eax
    mov $1, %ebx
    mov $after_sleep, %ecx
    mov $(after_sleep_end-after_sleep), %edx
    int $0x80

    # Marca o ponto anterior à cessão voluntária da CPU.
    mov $1, %eax
    mov $1, %ebx
    mov $before_yield, %ecx
    mov $(before_yield_end-before_yield), %edx
    int $0x80

    mov $4, %eax
    int $0x80

    # Confirma que a execução foi retomada após yield.
    mov $1, %eax
    mov $1, %ebx
    mov $after_yield, %ecx
    mov $(after_yield_end-after_yield), %edx
    int $0x80

    # Obtém o PID do processo pai.
    mov $5, %eax
    int $0x80

    # Mesma limitação da conversão simplificada do PID.
    add $'0', %al
    mov %al, user_ppid_digit

    mov $1, %eax
    mov $1, %ebx
    mov $ppid_message, %ecx
    mov $(ppid_message_end-ppid_message), %edx
    int $0x80

    # Encerra o teste principal com status 42.
    mov $2, %eax
    mov $42, %ebx
    int $0x80

1:
    # Fallback caso exit retorne inesperadamente.
    jmp 1b


.global user_tty_block_test_entry
user_tty_block_test_entry:
    # Anuncia a leitura que deverá bloquear enquanto não houver entrada.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_block_waiting, %ecx
    mov $(tty_block_waiting_end-tty_block_waiting), %edx
    int $0x80

    # Aguarda um byte em stdin, fornecido pelo mecanismo de teste.
    mov $9, %eax
    xor %ebx, %ebx
    mov $tty_block_byte, %ecx
    mov $1, %edx
    int $0x80

    cmp $1, %eax
    jne tty_block_failed

    cmpb $'Z', tty_block_byte
    jne tty_block_failed

    mov $1, %eax
    mov $1, %ebx
    mov $tty_block_ok, %ecx
    mov $(tty_block_ok_end-tty_block_ok), %edx
    int $0x80

    # Sucesso: exit(0).
    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

tty_block_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $tty_block_fail, %ecx
    mov $(tty_block_fail_end-tty_block_fail), %edx
    int $0x80

    # Falha: exit(1).
    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_tty_foreground_test_entry
user_tty_foreground_test_entry:
    # Teste manual de leitura em foreground: aceita qualquer byte.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_waiting, %ecx
    mov $(tty_fg_waiting_end-tty_fg_waiting), %edx
    int $0x80

    mov $9, %eax
    xor %ebx, %ebx
    mov $tty_fg_byte, %ecx
    mov $1, %edx
    int $0x80

    cmp $1, %eax
    jne tty_fg_failed

    # Exibe o byte recebido entre prefixo e sufixo.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_prefix, %ecx
    mov $(tty_fg_prefix_end-tty_fg_prefix), %edx
    int $0x80

    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_byte, %ecx
    mov $1, %edx
    int $0x80

    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_suffix, %ecx
    mov $(tty_fg_suffix_end-tty_fg_suffix), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

tty_fg_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_fail, %ecx
    mov $(tty_fg_fail_end-tty_fg_fail), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_tty_canonical_test_entry
user_tty_canonical_test_entry:
    # Em modo canônico, a leitura aguarda o fechamento da linha com Enter.
    # O conteúdo esperado é "hello\n", totalizando seis bytes.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_canon_waiting, %ecx
    mov $(tty_canon_waiting_end-tty_canon_waiting), %edx
    int $0x80

    mov $9, %eax
    xor %ebx, %ebx
    mov $tty_canon_buffer, %ecx
    mov $32, %edx
    int $0x80

    cmp $6, %eax
    jne tty_canon_failed

    cmpb $'h', tty_canon_buffer+0
    jne tty_canon_failed
    cmpb $'e', tty_canon_buffer+1
    jne tty_canon_failed
    cmpb $'l', tty_canon_buffer+2
    jne tty_canon_failed
    cmpb $'l', tty_canon_buffer+3
    jne tty_canon_failed
    cmpb $'o', tty_canon_buffer+4
    jne tty_canon_failed
    cmpb $'\n', tty_canon_buffer+5
    jne tty_canon_failed

    mov $1, %eax
    mov $1, %ebx
    mov $tty_canon_ok, %ecx
    mov $(tty_canon_ok_end-tty_canon_ok), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

tty_canon_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $tty_canon_fail, %ecx
    mov $(tty_canon_fail_end-tty_canon_fail), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_ud_test_entry
user_ud_test_entry:
    # Anuncia a geração deliberada de uma exceção #UD.
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    # UD2 gera uma exceção de instrução inválida.
    # O kernel deve encerrar o processo sem derrubar o sistema.
    ud2

    # Status sentinela: este caminho não deve ser alcançado.
    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    # Anuncia a tentativa de executar uma instrução privilegiada.
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    # Com CPL=3 e IOPL inferior a 3, CLI deve gerar #GP.
    cli

    # Status sentinela: indica que a proteção esperada não interrompeu o fluxo.
    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    # Cada nova instância deve receber sua própria cópia inicialmente zerada.
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    # Modifica o dado privado desta instância.
    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Anuncia o acesso inválido pelo caminho normal de syscall.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Escrita deliberada na página nula, que deve estar desmapeada.
    # Espera-se #PF e encerramento apenas do processo responsável.
    movl $0xDEADBEEF, 0x00000000

    # Status sentinela: este caminho não deve ser alcançado.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


# Dados graváveis utilizados pelos testes vinculados ao kernel.
.section .userdata, "aw", @progbits
.align 16

# Mensagem exportada para o teste de syscall chamado pelo kernel.
.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

# Reserva de 4 KiB para a pilha de usuário.
.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

# Mensagens sem NUL: o comprimento é calculado pela diferença entre labels.
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

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

# Palavra modificada pelo teste de isolamento dos espaços de endereçamento.
.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

tty_canon_waiting:
    .ascii "ttycanon: waiting for line\n"
tty_canon_waiting_end:

tty_canon_ok:
    .ascii "ttycanon: canonical read/backspace OK\n"
tty_canon_ok_end:

tty_canon_fail:
    .ascii "ttycanon: FAILED\n"
tty_canon_fail_end:

# Buffer de recepção do teste de leitura canônica.
.align 16
tty_canon_buffer:
    .skip 32

# Caminhos fornecidos às syscalls precisam de terminação NUL.
fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

# Buffers dimensionados conforme os layouts atuais da UAPI x86.
.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

# As mensagens reservam somente um caractere para representar PID e PPID.
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

# Marca este objeto como não exigindo uma pilha executável.
.section .note.GNU-stack,"",@progbits