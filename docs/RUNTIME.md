# Runtime do Cruzeiro Kernel (CZK_x86)

Este documento descreve, em ordem, o caminho que o **Cruzeiro Kernel x86 de 32 bits** percorre desde o build até o momento em que o sistema entra no loop normal de execução, recebe interrupções, agenda tarefas e executa código em Ring 3.

> **Importante:** “ordem dos arquivos” e “ordem de execução” são coisas diferentes. Durante o build, vários arquivos `.c` e `.s` são compilados separadamente e viram objetos `.o`. Depois do link, eles deixam de existir como unidades independentes em runtime: a CPU vê um único binário com seções, símbolos e endereços.

## 1. Visão geral

```text
Arquivos fonte
    |
    | gcc / assembler
    v
Objetos .o
    |
    | linker.ld
    v
czk_x86.bin
    |
    | GRUB / Multiboot2
    v
_start em boot/boot.s
    |
    v
GDT inicial
    |
    v
Stack inicial
    |
    v
kernel_main(multiboot_info)
    |
    +--> TSS
    +--> IDT
    +--> ISRs
    +--> IRQs
    +--> PIC
    +--> PIT / scheduler
    +--> FPU
    +--> PMM
    +--> VMM / paging
    +--> heap / kmalloc
    +--> RAMFS / VFS
    +--> shell
    |
    v
sti
    |
    v
loop principal com hlt
    |
    +--> timer IRQ
    +--> teclado IRQ
    +--> syscalls int 0x80
    +--> scheduler
    +--> tarefas Ring 3
```

# Parte I — Build

## 2. Compilação dos arquivos fonte

O kernel possui código em C e Assembly. Arquivos como `kernel/kernel.c`, `kernel/task.c`, `kernel/syscall.c` e `arch/x86/...` são compilados para arquivos objeto `.o`. Arquivos Assembly como `boot/boot.s`, `boot/isr.s` e `boot/irq.s` também são montados e transformados em objetos.

Nesse momento ainda não existe um kernel executável completo. Cada `.o` contém código de máquina, seções, símbolos, referências externas, relocations, dados e BSS.

Por exemplo, `boot/boot.s` declara:

```asm
.extern kernel_main
```

O assembler não precisa saber o endereço final de `kernel_main`; quem resolve isso é o linker.

## 3. O linker junta o kernel

O arquivo central dessa etapa é `linker.ld`. No CZK_x86 ele define:

```ld
ENTRY(_start)
```

Isso informa ao linker que o ponto de entrada é `_start`.

O script também posiciona o kernel começando em:

```ld
. = 1M;
```

ou seja, `0x00100000`.

A ordem atual das seções é:

```text
.text
.rodata
.usertext
.userdata
.data
.bss
```

As principais regiões são alinhadas a páginas de 4 KiB.

## 4. Seção `.text`

A região `.text` contém principalmente código executável e também incorpora o header Multiboot:

```ld
.text ALIGN(4K) : {
    _text_start = .;
    *(.multiboot)
    *(.text*)
    _text_end = .;
}
```

Os símbolos `_text_start` e `_text_end` permitem ao próprio kernel saber onde essa região começa e termina.

## 5. Seção `.rodata`

A `.rodata` guarda dados somente de leitura, como strings constantes e tabelas `const`.

O linker exporta:

```text
_rodata_start
_rodata_end
```

O kernel pode usar esses limites para tornar a região read-only no paging.

## 6. `.usertext`

O CZK possui uma seção específica para código destinado ao Ring 3:

```ld
.usertext ALIGN(4K)
```

Ela produz os símbolos:

```text
_user_text_start
_user_text_end
```

Isso permite mapear somente essa faixa como acessível ao userspace, em vez de expor o `.text` inteiro do kernel.

## 7. `.userdata`

A seção `.userdata` contém dados destinados ao Ring 3 e exporta:

```text
_user_data_start
_user_data_end
```

Ela pode conter strings, stack de usuário e dados mutáveis do programa de teste. Normalmente precisa ser mapeada como `PRESENT | USER | WRITE`.

## 8. `.data`

A `.data` contém variáveis globais e estáticas inicializadas com valores diferentes de zero.

## 9. `.bss`

A `.bss` contém dados inicialmente zerados, como buffers, arrays estáticos e stacks reservadas.

O BSS não precisa ocupar no arquivo final a mesma quantidade de bytes que ocupa em RAM. Por isso `ls -l czk_x86.bin` pode mostrar um tamanho menor que a soma `text + data + bss` apresentada por `size`.

# Parte II — Entrada pelo bootloader

## 10. O GRUB encontra o kernel

O CZK_x86 usa Multiboot2. No início de `boot/boot.s` existe uma seção `.multiboot` com os campos exigidos pelo protocolo, incluindo `MAGIC`, `ARCH`, `HEADER_LEN` e `CHECKSUM`.

O GRUB procura esse header para reconhecer o binário como um kernel Multiboot2 válido.

## 11. GRUB carrega o binário

O bootloader localiza o kernel, carrega suas seções, prepara as informações Multiboot e transfere o controle para o entry point definido pelo binário.

No CZK:

```text
ENTRY(_start)
```

Portanto a execução começa em `_start`, dentro de `boot/boot.s`.

# Parte III — `_start`

## 12. `cli`

A primeira ação relevante é:

```asm
cli
```

Isso limpa `EFLAGS.IF` e desativa interrupções mascaráveis temporariamente. O kernel ainda está construindo estruturas fundamentais e não quer receber um timer IRQ antes de existir uma IDT funcional.

## 13. Carregamento da GDT

Em seguida:

```asm
lgdt gdt_descriptor
```

A instrução `LGDT` carrega o `GDTR`. A partir daí a CPU passa a usar a GDT do Cruzeiro Kernel.

Entre os seletores usados no CZK estão:

```text
0x08 -> kernel code
0x10 -> kernel data
0x1B -> user code, RPL 3
0x23 -> user data, RPL 3
0x28 -> TSS
```

## 14. Recarregamento dos segmentos de dados

Depois de `lgdt`, o kernel faz:

```asm
mov $0x10, %ax
mov %ax, %ds
mov %ax, %es
mov %ax, %fs
mov %ax, %gs
mov %ax, %ss
```

Isso faz os registradores de segmento passarem a usar o descritor de dados do kernel.

## 15. Recarregamento de `CS`

`CS` não pode ser trocado com um `mov` comum. Por isso o kernel usa um far jump:

```asm
ljmp $0x08, $flush_cs
```

A execução continua em `flush_cs` já com `CS = 0x08`.

# Parte IV — Stack inicial

## 16. Configuração de `ESP`

O kernel reserva uma stack estática em `.bss`:

```asm
stack_bottom:
    .skip 16384
stack_top:
```

Ela possui 16 KiB.

A stack x86 cresce para endereços menores, então o kernel inicia com:

```asm
mov $stack_top, %esp
```

A partir daí chamadas C, pushes, variáveis locais e endereços de retorno possuem uma stack válida.

# Parte V — Entrada no C

## 17. Ponteiro Multiboot

O GRUB entrega informações Multiboot ao kernel. No caminho atual, o ponteiro relevante chega em `EBX`.

Antes de chamar C:

```asm
push %ebx
call kernel_main
add $4, %esp
```

Isso passa o endereço como primeiro argumento de:

```c
kernel_main(uint32_t mb_info_addr)
```

## 18. `kernel_main()`

Esse é o ponto em que a inicialização deixa de ser majoritariamente Assembly e passa para C.

A partir daqui, a ordem das chamadas dentro de `kernel_main()` determina a ordem de inicialização dos subsistemas. Isso é completamente diferente da ordem dos arquivos no Makefile.

# Parte VI — Inicialização arquitetural

## 19. VGA e logging

O kernel prepara a saída VGA e os primeiros logs de boot. Nesse estágio ainda não existe um terminal userspace convencional; a VGA é uma das principais formas de observar o estado da inicialização.

## 20. TSS

O kernel inicializa a `Task State Segment`.

No CZK ela não é usada como o mecanismo clássico de hardware task switching. O uso central é fornecer à CPU a stack Ring 0 que deve ser usada numa transição:

```text
Ring 3 -> Ring 0
```

Por exemplo:

```text
user task
    |
    | int 0x80
    v
CPU consulta TSS.esp0
    |
    v
troca para kernel stack
    |
    v
handler de syscall
```

O scheduler atualiza `esp0` ao trocar de tarefa para que cada task possa possuir sua própria kernel stack.

## 21. IDT

A `Interrupt Descriptor Table` informa à CPU qual handler corresponde a cada vetor.

Conceitualmente:

```text
IDT[0]   -> Divide Error
IDT[6]   -> Invalid Opcode
IDT[7]   -> #NM / FPU
IDT[13]  -> General Protection
IDT[14]  -> Page Fault
IDT[32]  -> IRQ0 / timer
IDT[33]  -> IRQ1 / keyboard
...
IDT[128] -> syscall entry
IDT[129] -> yield interno
```

## 22. ISRs

Os vetores `0–31` correspondem às exceções definidas pela arquitetura x86.

Exemplos:

```text
0  #DE
6  #UD
7  #NM
8  #DF
13 #GP
14 #PF
```

O vetor `128 / 0x80` não é uma exceção arquitetural. Ele foi escolhido pelo Cruzeiro como entrada de syscall e utiliza a infraestrutura de software interrupt/ISR.

## 23. IRQs

As IRQs tradicionais do PIC são remapeadas para `32–47`, evitando conflito com as exceções da CPU.

```text
IRQ0  -> IDT[32] -> timer
IRQ1  -> IDT[33] -> keyboard
...
IRQ15 -> IDT[47]
```

O vetor `129 / 0x81` é usado internamente pelo scheduler para yield no kernel. Ele não vem de hardware.

Esse vetor deve possuir DPL 0, por exemplo com uma gate `0x8E`, impedindo Ring 3 de executar diretamente:

```asm
int $0x81
```

O userspace deve passar pela ABI oficial via `int $0x80` e `SYS_YIELD`.

# Parte VII — PIC e IRQs reais

## 24. Remapeamento do PIC

O PIC é remapeado para começar em `0x20` e `0x28`:

```text
Master PIC: 32–39
Slave PIC : 40–47
```

Isso impede conflito com os vetores arquiteturais 0–31.

## 25. Máscaras de IRQ

Durante a inicialização, IRQs podem ser mascaradas e depois liberadas seletivamente. No estágio atual, as mais importantes são:

```text
IRQ0 -> timer
IRQ1 -> keyboard
```

# Parte VIII — Timer e scheduler

## 26. PIT

O PIT gera interrupções periódicas:

```text
PIT
 |
 v
PIC
 |
 v
IRQ0
 |
 v
IDT[32]
 |
 v
irq stub
 |
 v
C IRQ handler
```

## 27. Salvamento do contexto

O stub Assembly salva o estado da CPU. No CZK_x86 o frame usado pelo scheduler é representado por `registers_t` e inclui registradores de segmento, registradores gerais, número de interrupção, código de erro, `EIP`, `CS`, `EFLAGS`, `useresp` e `SS`.

Esse frame permite restaurar uma tarefa posteriormente usando `iret`.

## 28. Escolha da próxima task

Quando o scheduler decide trocar de tarefa, ele:

1. guarda o frame da tarefa atual;
2. encontra outra task `READY`;
3. atualiza os estados;
4. define a nova `current`;
5. atualiza a kernel stack na TSS;
6. prepara o estado lazy da FPU;
7. retorna o frame da próxima tarefa.

O stub Assembly troca `ESP` para o novo frame e, ao chegar em `iret`, a CPU restaura a próxima tarefa.

# Parte IX — FPU

## 29. Inicialização e lazy FPU switching

O CZK possui gerenciamento lazy de contexto da FPU.

Durante uma troca de task o kernel pode marcar `CR0.TS = 1`. Quando a nova tarefa tenta usar FPU, a CPU gera:

```text
#NM
vetor 7
```

O handler pode então salvar o estado da tarefa anterior, restaurar o da atual, atualizar o owner, limpar `TS` e continuar.

Isso evita salvar/restaurar FPU em toda troca de contexto quando uma task nunca usa ponto flutuante.

# Parte X — Gerenciamento de memória

## 30. PMM

O `Physical Memory Manager` é inicializado usando o mapa de memória entregue pelo Multiboot2.

Ele trabalha com frames físicos e precisa saber quais regiões existem, quais pertencem ao kernel, quais estão ocupadas e quais estão livres.

## 31. VMM

Depois vem o `Virtual Memory Manager`, responsável por page directory, page tables, PDEs e PTEs.

O kernel então passa a controlar permissões e mapeamentos de memória virtual.

## 32. Proteção das regiões do kernel

Depois de existir paging funcional, o CZK pode tornar regiões como `.text` e `.rodata` somente leitura.

Isso reduz corrupção acidental e impede escritas indevidas em código do kernel.

## 33. Regiões Ring 3

`.usertext` e `.userdata` são mapeadas com permissão `USER`.

Conceitualmente:

```text
.usertext:
    PRESENT
    USER
    normalmente sem WRITE

.userdata:
    PRESENT
    USER
    WRITE
```

A flag USER precisa estar correta nos níveis relevantes de PDE e PTE. Sem ela, uma task CPL3 recebe page fault ao tentar acessar a página.

# Parte XI — Heap

## 34. `kmalloc`

Depois que memória física e virtual estão utilizáveis, o kernel inicializa o allocator.

A partir daí subsistemas podem fazer alocações dinâmicas para `task_t`, stacks de kernel, buffers, estruturas de filesystem e estados auxiliares.

# Parte XII — RAMFS e VFS

## 35. RAMFS

O kernel inicializa o filesystem em RAM. Isso permite representar arquivos e diretórios mesmo sem um driver de disco físico completo.

No futuro, o VFS pode servir como abstração comum para RAMFS, storage ATA, outros filesystems e devices.

# Parte XIII — Interrupções habilitadas

## 36. `sti`

Somente depois de estruturas fundamentais estarem prontas é seguro executar:

```asm
sti
```

Isso seta `EFLAGS.IF = 1`. A partir daí timer e teclado podem interromper o fluxo normal.

# Parte XIV — Shell

## 37. Inicialização do shell

O shell atual é uma ferramenta interna do kernel para desenvolvimento, testes e diagnóstico. Ele ainda não é um shell userspace tradicional.

Ele permite observar tarefas, ticks, estados, testes Ring 3, zombies, exit codes e outros componentes do CZK.

# Parte XV — Loop principal

## 38. `hlt`

Depois da inicialização, `kernel_main()` entra num loop com `hlt`.

```c
for (;;) {
    asm volatile ("hlt");
}
```

Com interrupções habilitadas:

```text
hlt
 |
 | IRQ
 v
handler
 |
 v
iret
 |
 v
hlt novamente
```

Isso evita desperdiçar CPU em um busy loop.

# Parte XVI — Criação de uma tarefa Ring 3

## 39. `ring3test`

Quando o shell cria uma user task, o scheduler prepara uma nova `task_t` com PID, PPID, estado, kernel stack, context frame e estado de FPU.

O frame inicial recebe seletores de usuário:

```text
CS = 0x1B
SS = 0x23
DS = 0x23
ES = 0x23
FS = 0x23
GS = 0x23
```

Também recebe:

```text
EIP     = user_test_entry
USERESP = user_stack_top
EFLAGS  = IF habilitado
```

## 40. A task entra na fila

Criar a task não significa necessariamente executá-la imediatamente. Ela fica pronta para ser escolhida pelo scheduler.

## 41. Entrada real em Ring 3

Quando o frame da user task é restaurado, o stub chega ao `iret`.

Como o frame possui seletores com RPL3, a CPU percebe que está retornando para um nível menos privilegiado e restaura `EIP`, `CS`, `EFLAGS`, `USERESP` e `SS`.

A partir daí o código executa com `CPL = 3`.

# Parte XVII — Syscalls

## 42. Userspace não chama o kernel diretamente

Ring 3 não deve chamar funções internas como `sched_yield_irq()` ou `vga_puts()` diretamente.

Ele usa a ABI de syscall:

```asm
int $0x80
```

## 43. Vetor 128

`0x80` é 128 decimal. A IDT possui uma gate correspondente e, para permitir chamadas de CPL3, ela precisa de DPL3.

No esquema atual isso é representado por `0xEE`.

## 44. Transição Ring 3 -> Ring 0

Quando uma user task executa `int $0x80`, a CPU:

1. lê `IDT[128]`;
2. verifica o privilégio da gate;
3. identifica a mudança para Ring 0;
4. consulta a TSS;
5. obtém a stack Ring 0;
6. troca para a kernel stack;
7. salva o contexto de usuário;
8. entra no stub;
9. chega ao syscall handler C.

## 45. ABI de registradores

No estágio atual, o número da syscall fica em `EAX`.

```text
1 -> SYS_WRITE
2 -> SYS_EXIT
3 -> SYS_GETPID
4 -> SYS_YIELD
```

Argumentos adicionais usam registradores como `EBX`, `ECX` e `EDX`.

## 46. `SYS_WRITE`

Exemplo conceitual:

```asm
mov $SYS_WRITE, %eax
mov $1, %ebx
mov $buffer, %ecx
mov $length, %edx
int $0x80
```

O kernel interpreta os registradores e executa o serviço.

> Ponteiros vindos do userspace devem ser validados cuidadosamente antes de o kernel dereferenciá-los.

## 47. `SYS_GETPID`

```asm
mov $SYS_GETPID, %eax
int $0x80
```

O valor retorna em `EAX`.

## 48. `SYS_YIELD`

O userspace não usa diretamente `int $0x81`.

Ele faz:

```asm
mov $SYS_YIELD, %eax
int $0x80
```

O syscall handler entrega o frame ao scheduler. O scheduler pode devolver outro `registers_t *`, e o stub troca para esse novo frame antes do `iret`.

Assim uma task pode ceder a CPU e, quando for escolhida novamente, continuar exatamente após o `int $0x80`.

# Parte XVIII — `SYS_EXIT`, ZOMBIE e wait

## 49. Encerramento

Uma task Ring 3 pode terminar com:

```asm
mov $SYS_EXIT, %eax
mov $42, %ebx
int $0x80
```

`EBX` contém o exit code.

## 50. Estado ZOMBIE

A tarefa pode permanecer em `TASK_ZOMBIE` para que o parent ainda consiga obter PID e exit status.

## 51. Wait

O protótipo atual possui lógica para consumir um filho zombie:

```text
child executa SYS_EXIT
        |
        v
TASK_ZOMBIE
        |
parent executa wait
        |
        v
kernel lê exit_code
        |
        v
remove task da lista
        |
        v
libera recursos
```

No futuro isso pode evoluir para um `wait/waitpid` bloqueante exposto formalmente ao userspace.

# Parte XIX — O que realmente significa “carregar um arquivo”

## 52. A CPU não carrega `task.c`

Depois do link, a CPU nunca pensa “agora vou abrir `task.c`”.

`task.c` já foi transformado em bytes de código dentro de `.text`. Da mesma forma, `boot/isr.s` virou opcodes dentro do binário.

## 53. Símbolos viram endereços

Uma chamada como:

```c
sched_yield_irq(regs);
```

vira uma transferência de controle para um endereço resolvido pelo linker.

Em runtime a CPU não sabe que aquilo veio de C, o nome do arquivo, a linguagem original ou que existiu um `.o`.

# Parte XX — Ordem de build versus ordem de runtime

## 54. Ordem de build

```text
boot.s ----\
isr.s ------\
irq.s -------\
kernel.c -----\
task.c --------> objetos .o -> linker -> kernel
syscall.c -----/
vmm.c --------/
...
```

## 55. Ordem de runtime

```text
GRUB
 |
 v
_start
 |
 v
lgdt
 |
 v
segment registers
 |
 v
stack
 |
 v
kernel_main
 |
 v
subsistemas de arquitetura
 |
 v
memória
 |
 v
heap/filesystem
 |
 v
sti
 |
 v
shell / scheduler
 |
 v
IRQs + syscalls + tasks
```

A ordem real é determinada por entry point, jumps, calls, interrupts, scheduler e frames de retorno — não pelo nome dos arquivos.

# Parte XXI — Exemplo de um ciclo completo

## 56. Do boot ao Ring 3

```text
GRUB carrega czk_x86.bin
        |
        v
_start
        |
        v
cli
        |
        v
lgdt
        |
        v
carrega segmentos
        |
        v
far jump / CS
        |
        v
ESP = stack_top
        |
        v
kernel_main(mb_info)
        |
        v
TSS
        |
        v
IDT
        |
        v
PIC / IRQ / PIT
        |
        v
FPU
        |
        v
PMM
        |
        v
VMM
        |
        v
kmalloc
        |
        v
RAMFS
        |
        v
sti
        |
        v
shell
        |
        v
ring3test
        |
        v
cria task
        |
        v
timer IRQ
        |
        v
scheduler escolhe user task
        |
        v
TSS.esp0 atualizado
        |
        v
iret
        |
        v
CPL3
        |
        v
user_test_entry
        |
        v
int 0x80
        |
        v
TSS + kernel stack
        |
        v
CPL0
        |
        v
syscall_handler
        |
        v
iret
        |
        v
CPL3
```

# Parte XXII — Arquivos mais importantes no fluxo

## 57. `linker.ld`

Responsável por entry point, posição inicial, layout das seções, alinhamento, símbolos de começo/fim e separação de `.usertext/.userdata`.

## 58. `boot/boot.s`

Responsável pelo início real da execução x86: Multiboot2, `_start`, `cli`, `lgdt`, segmentos, far jump, stack inicial e chamada de `kernel_main`.

## 59. GDT

Define segmentos e níveis de privilégio básicos. Uma GDT inválida pode quebrar praticamente toda a execução protegida.

## 60. IDT

Conecta vetores da CPU a handlers e sustenta exceções, IRQs e software interrupts.

## 61. `boot/isr.s`

Faz a ponte Assembly entre eventos da CPU e handlers C e precisa preservar corretamente o frame usado pelo `iret`.

## 62. `boot/irq.s`

Faz a ponte para IRQs e o caminho de preempção/context switch. O handler poder devolver outro frame é o que permite uma troca real de task.

## 63. `kernel/kernel.c`

É o centro da inicialização geral do kernel. A ordem de chamadas nessa função determina grande parte do startup do CZK.

## 64. `task.c`

Mantém tasks, estados, stacks, contextos, PID/PPID, scheduling, zombies, exit, wait e integração com FPU/TSS.

## 65. `syscall.c`

É a fronteira principal entre Ring 3 e Ring 0. Tudo recebido ali deve ser tratado como entrada não confiável.

# Parte XXIII — Princípio central

A maneira mais correta de enxergar o runtime do CZK é:

```text
não existem arquivos em execução;
existem endereços, estados da CPU e transferências de controle.
```

Os arquivos são uma ferramenta humana de organização.

Depois que o kernel foi linkado e carregado, a CPU só conhece bytes, registradores, memória, tabelas arquiteturais, privilégios, interrupções, instruções e endereços.

# Resumo final

```text
1. Fontes são compilados para .o
2. linker.ld organiza tudo em czk_x86.bin
3. GRUB reconhece o Multiboot2
4. GRUB carrega o kernel
5. CPU entra em _start
6. Interrupções são inicialmente bloqueadas
7. GDT é carregada
8. Segmentos são recarregados
9. CS é recarregado via far jump
10. Stack inicial é configurada
11. kernel_main recebe o ponteiro Multiboot
12. TSS é preparada
13. IDT/ISRs/IRQs são instaladas
14. PIC/PIT são configurados
15. FPU é inicializada
16. PMM é inicializado
17. VMM/paging é inicializado
18. Proteções de memória são aplicadas
19. kmalloc é inicializado
20. RAMFS/VFS são inicializados
21. Interrupções são habilitadas com STI
22. Shell entra em funcionamento
23. Timer começa a preemptar
24. Scheduler escolhe tasks
25. User tasks podem entrar em Ring 3
26. int 0x80 retorna ao Ring 0 via syscall
27. TSS fornece kernel stack para a transição
28. iret restaura tasks e níveis de privilégio
29. exit gera ZOMBIE
30. wait pode consumir o filho encerrado
```

Esse é o caminho central de execução do **Cruzeiro Kernel x86** no estágio atual de desenvolvimento.