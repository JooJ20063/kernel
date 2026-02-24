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
