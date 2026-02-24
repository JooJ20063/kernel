# kernel

Kernel educacional em **C + Assembly (x86 32-bit)** para estudo de:
- boot com Multiboot2
- GDT/IDT
- ISRs/IRQs
- remapeamento e controle do PIC 8259
- saída de diagnóstico em VGA text mode

> Status atual: possui `Makefile` para build, geração de ISO e execução via QEMU.

## Estrutura do projeto

```text
.
├── boot/
│   ├── boot.s            # ponto de entrada (_start), carga de GDT e salto para kernel_main
│   ├── gdt.s             # tabela GDT e descritor
│   ├── isr.s             # stubs de exceções (0-31) e dispatch para C
│   ├── irq.s             # stubs de IRQs (32-47) e dispatch para C
│   └── idt_descriptor.s  # descritor IDT (símbolo global)
├── arch/x86/
│   ├── idt.c             # inicialização da IDT + instalação de ISRs/IRQs
│   ├── irq.c             # handler C de IRQ (envio de EOI)
│   └── pic.c             # remap/mask/unmask do PIC 8259
├── include/arch/x86/
│   ├── idt.h             # estruturas/protótipos de IDT
│   ├── irq.h             # interface do handler de IRQ
│   ├── pic.h             # interface do PIC
│   └── regs.h            # layout de registradores empilhados
├── include/kernel/
│   ├── vga.h             # interface da abstração VGA
│   ├── sched.h           # scheduler round-robin simples
│   └── pmm.h             # gerenciador físico de páginas
├── kernel/
│   ├── kernel.c          # kernel_main + handler de exceções
│   ├── vga.c             # abstração VGA (cor, scroll, cursor HW, decimal/hex)
│   ├── sched.c           # scheduler por quantum em ticks
│   └── pmm.c             # PMM bitmap de frames
├── build/                # objetos .o gerados pelo Makefile
├── linker.ld             # script de link
└── grub.cfg              # configuração GRUB
```

## Fluxo de inicialização

1. `boot/boot.s` entra em `_start`, desabilita interrupções, carrega a GDT e ajusta segmentos.
2. A pilha é inicializada e `kernel_main` é chamado.
3. `kernel_main`:
   - inicializa a IDT,
   - instala entradas de ISRs (0-31) e IRQs (32-47),
   - remapeia o PIC para `0x20-0x2F`,
   - mascara tudo e habilita IRQ0 (timer) e IRQ1 (teclado),
   - renderiza status inicial em VGA e habilita interrupções com `sti`.


## Observação importante sobre nome do binário

O `grub.cfg` deste projeto carrega **`/boot/kernel.bin`**. Portanto, no passo de link/cópia para ISO, use esse nome (ou ajuste o `grub.cfg` para o nome que preferir).

## Pré-requisitos (desenvolvimento)

- Linux com toolchain GCC que suporte `-m32`
- binutils (`as`, `ld`)
- opcional para imagem bootável:
  - `grub-mkrescue`
  - `xorriso`
  - `qemu-system-i386`



## IRQs implementadas

- **IRQ0 (timer/PIT)**: frequência configurável (atual em `irq_init(100, ...)`), atualiza status (`TIMER`, tarefa atual e trocas de contexto).
- **IRQ1 (teclado/PS2)**: leitura de scancode set 1 com suporte a **Shift/CapsLock** e mapeamento base ABNT2.

## PMM e scheduler

- PMM inicializado para 64 MiB com bitmap de frames de 4 KiB.
- API: `pmm_alloc_frame`, `pmm_free_frame`, `pmm_free_frame_count`.
- Scheduler simples round-robin por quantum (configurado em ticks no `irq_init`).

## Uso rápido com Makefile

```bash
make            # gera kernel.bin (objetos em build/)
make iso        # gera kernel.iso com GRUB
make run        # inicia no QEMU (boot via CD)
make check      # valida compilação freestanding dos .c
make clean      # limpa artefatos
```

## Build rápido (validação de compilação C)

Este comando verifica os módulos C principais em modo freestanding:

```bash
gcc -m32 -ffreestanding -Iinclude -Wall -Wextra -Werror -c \
  kernel/kernel.c kernel/vga.c kernel/sched.c kernel/pmm.c \
  arch/x86/idt.c arch/x86/irq.c arch/x86/pic.c
```

## Build manual (referência)

Mesmo com `Makefile`, abaixo está um fluxo manual de referência:

```bash
# 1) objetos .o (C + ASM)
gcc -m32 -ffreestanding -Iinclude -c kernel/kernel.c -o kernel.o
gcc -m32 -ffreestanding -Iinclude -c kernel/vga.c -o vga.o
gcc -m32 -ffreestanding -Iinclude -c kernel/sched.c -o sched.o
gcc -m32 -ffreestanding -Iinclude -c kernel/pmm.c -o pmm.o
gcc -m32 -ffreestanding -Iinclude -c arch/x86/idt.c -o idt.o
gcc -m32 -ffreestanding -Iinclude -c arch/x86/irq.c -o irq.o
gcc -m32 -ffreestanding -Iinclude -c arch/x86/pic.c -o pic.o

as --32 boot/boot.s -o boot.o
as --32 boot/gdt.s -o gdt.o
as --32 boot/isr.s -o isr.o
as --32 boot/irq.s -o irq_stubs.o
as --32 boot/idt_descriptor.s -o idt_desc.o

# 2) link do kernel ELF
ld -m elf_i386 -T linker.ld -o kernel.bin \
  boot.o gdt.o isr.o irq_stubs.o idt_desc.o kernel.o vga.o sched.o pmm.o idt.o irq.o pic.o

# 3) (opcional) gerar ISO com GRUB
mkdir -p iso/boot/grub
cp kernel.bin iso/boot/kernel.bin
cp grub.cfg iso/boot/grub/grub.cfg
grub-mkrescue -o kernel.iso iso

# 4) (opcional) executar
qemu-system-i386 -cdrom kernel.iso
```


## CI

Há workflow em `.github/workflows/build.yml` com:
- build do kernel (`make`)
- checagem freestanding (`make check`)

## Troubleshooting (QEMU)

- **`/boot/kernel.bin not found`**: verifique se você copiou `kernel.bin` para `iso/boot/kernel.bin` antes do `grub-mkrescue`.
- **Tela mostra `OK KERNEL !` e reinicia voltando ao BIOS/iPXE**: isso indica reset/triple fault após o kernel iniciar (não é erro de ISO). Garanta que os stubs de IRQ retornem com `iret` sem habilitar interrupções manualmente no meio do retorno.
- Para boot direto da ISO, prefira:

```bash
qemu-system-i386 -cdrom kernel.iso -boot d
```

## Próximos passos sugeridos

- Expandir mapa ABNT2 com AltGr e teclas mortas.
- Evoluir scheduler para tasks reais (context switch de registradores).
- PMM com mapa de memória Multiboot2 em vez de limite fixo.

## Licença

Consulte o arquivo [LICENSE](LICENSE).
