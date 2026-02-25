# Arquitetura do Kernel

Este documento descreve os componentes atuais do kernel e como eles interagem.

## Visão geral de boot

1. `boot/boot.s` executa `_start`, carrega GDT e prepara segmentos.
2. `kernel_main` inicializa IDT/ISRs/IRQs, remapeia PIC e habilita IRQs de timer/teclado.
3. IRQs passam pelos stubs ASM e chegam em `irq_handler_c`.

## Subsistemas

### IDT/ISRs/IRQs
- `arch/x86/idt.c`: instala gates padrão, ISRs (0..31) e IRQs (32..47).
- `boot/isr.s`: stubs de exceções.
- `boot/irq.s`: stubs de IRQ com retorno via `iret`.

### PIC/PIT
- `arch/x86/pic.c`: remap, mask/unmask, EOI.
- `arch/x86/irq.c`: configura PIT e trata IRQ0/IRQ1.

### VGA/log/panic
- `kernel/vga.c`: saída em modo texto, cursor, scroll, cores.
- `kernel/klog.c`: logging com níveis.
- `kernel/panic.c`: tela de panic + dump de registradores + halt.

### Shell
- `kernel/shell.c`: parser de linha, comandos e integração com teclado.

### PMM e scheduler
- `kernel/pmm.c`: bitmap de frames de 4KiB.
- `kernel/sched.c`: scheduler round-robin em ticks.

## Fluxo de entrada do teclado

1. IRQ1 lê scancode da porta `0x60`.
2. Traduz para caractere (ABNT2 base + Shift/CapsLock).
3. Encaminha para `shell_on_key`.

## Fluxo de exceção

1. Exceção entra em stub ISR.
2. `isr_handler_c` identifica motivo.
3. `kernel_panic` imprime contexto e para CPU.


## Hierarquia de proteção

- **Ring 0 (Kernel)**: acesso total a PMM, IDT, PIC/PIT e I/O de hardware.
- **Memória reservada (0..1MiB)**: região historicamente usada por BIOS/dispositivos; tratada como área sensível e não exposta para lógica de alto nível.
- **Isolamento de contexto em ISR**: cada entrada de exceção/IRQ preserva contexto em `registers_t`, reduzindo risco de corrupção silenciosa de estado.

## Observações de robustez de hardware

- VGA usa portas CRT padrão (`0x3D4/0x3D5`) e memória de vídeo em `0xB8000`.
- PIC é remapeado para `0x20/0x28` para evitar conflito com vetores de exceção da CPU.
- Teclado PS/2 (IRQ1) utiliza leitura de status/controlador antes da leitura do scancode para maior resiliência.

# 📑 Especificação de Arquitetura de Sistema (x86-32)

Este documento define as diretrizes de interação entre o Kernel e a CPU, estabelecendo os mecanismos de isolamento, proteção de memória e tratamento de eventos críticos. O objetivo primário é a resiliência do sistema e a proteção contra falhas de execução.

---

## 1. Modelo de Privilégios e Segmentação (GDT)

O sistema utiliza o **Flat Memory Model** para simplificar o endereçamento, mas mantém o rigor na separação de privilégios através dos anéis de proteção (*Protection Rings*).

| Seletor | Segmento        | Base         | Limite | Ring | Atributos             |
| :---    | :---            | :---         | :---   | :--- | :---                  |
| `0x08`  | **Kernel Code** | `0x00000000` | 4 GB   | 0    | Exec/Read, Conforming |
| `0x10`  | **Kernel Data** | `0x00000000` | 4 GB   | 0    | Read/Write, Expand-up |
| `0x18`  | **User Code** | `0x00000000` | 4 GB   | 3    | Exec/Read (Roadmap)   |
| `0x20`  | **User Data** | `0x00000000` | 4 GB   | 3    | Read/Write (Roadmap)   |

### Mecanismos de Isolamento:
* **Hierarquia de Rings:** O hardware impede que código executando em Ring 3 utilize instruções privilegiadas (`HLT`, `CLI`, `LIDT`, `IN/OUT`).
* **Transição de Contexto:** (Roadmap) Implementação do **Task State Segment (TSS)** para permitir o *Stack Switching* seguro durante a transição de privilégio (Ring 3 -> Ring 0).



---

## 2. Subsistema de Eventos e Vetores (IDT)

A **Interrupt Descriptor Table (IDT)** atua como o portão de controle do sistema. Todas as entradas são configuradas como **Interrupt Gates (0x8E)**, o que garante que as interrupções sejam desabilitadas automaticamente ao entrar no manipulador (*handler*), prevenindo condições de corrida.

### Classificação de Vetores:
1. **Exceções da CPU (0-31):** Tratamento de falhas de hardware e erros lógicos.
    * **#GP (Vetor 13):** Proteção geral contra acessos a segmentos inválidos ou instruções restritas.
    * **#PF (Vetor 14):** Pilar central para o VMM (Virtual Memory Manager), permitindo a carga de memória sob demanda.
2. **Interrupções de Hardware (32-47):** Remapeadas via **PIC 8259** para evitar conflitos com as exceções nativas da CPU.
3. **Interface de Sistema (0x80):** Vetor reservado para **System Calls**, permitindo que o Ring 3 solicite serviços ao Kernel de forma segura.



---

## 3. Gestão de Memória Física (PMM)

O Kernel utiliza um gerenciador baseado em **Bitmap** para o controle de frames físicos de 4 KiB.

* **Grão de Alocação:** Cada bit no bitmap representa 4 KiB de memória física.
* **Reserva de Segurança:** Os primeiros 1024 KiB (`0x00000000` - `0x000FFFFF`) são marcados como ocupados permanentemente para proteger a IVT, BDA da BIOS e o **Buffer VGA (`0xB8000`)**.
* **Integridade:** O PMM garante que apenas frames alinhados sejam entregues, prevenindo falhas de desalinhamento na ativação da paginação.

---

## 4. Estado da CPU e Registradores de Controle

O Kernel mantém controle estrito sobre o estado da CPU através dos registros de controle:

* **EFLAGS:** O bit IF (Interrupt Flag) é manipulado para proteger seções críticas do Kernel.
* **CR0:** Configurado para habilitar a Proteção de Escrita (**WP Bit**) e, futuramente, a Paginação (**PG Bit**).
* **CR3:** Gerencia o endereço base do *Page Directory* ativo, isolando os espaços de endereçamento virtual.



---

## 5. Roadmap de Hardening (Segurança Ativa)

Para garantir um sistema resiliente contra ataques externos e falhas de usuário, as seguintes tecnologias serão integradas:

1. **WP (Write Protect):** Bloqueia a escrita em páginas Read-Only mesmo para o Kernel (Ring 0), mitigando corrupção acidental de código.
2. **SMEP/SMAP:** Impede que o Kernel execute ou acesse dados em endereços de memória de usuário, mitigando ataques de escalada de privilégio (*Privilege Escalation*).
3. **NX Bit (No-Execute):** (Em Long Mode) Marca páginas de dados (pilha e heap) como não-executáveis para impedir a execução de *payloads* maliciosos.

---
