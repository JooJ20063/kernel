# kernel

Kernel educacional em **C + Assembly (x86 32-bit)** para estudo de:
- boot com Multiboot2
- GDT/IDT
- ISRs/IRQs
- remapeamento e controle do PIC 8259
- saída de diagnóstico em VGA text mode

> Status atual: projeto enxuto, sem sistema de build automatizado (`Makefile`) neste repositório.

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
├── kernel/
│   └── kernel.c          # kernel_main + handler de exceções + saída VGA
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
   - mascara tudo e habilita apenas IRQ0 (timer),
   - habilita interrupções com `sti`.

## Pré-requisitos (desenvolvimento)

- Linux com toolchain GCC que suporte `-m32`
- binutils (`as`, `ld`)
- opcional para imagem bootável:
  - `grub-mkrescue`
  - `xorriso`
  - `qemu-system-i386`

## Build rápido (validação de compilação C)

Este comando verifica os módulos C principais em modo freestanding:

```bash
gcc -m32 -ffreestanding -Iinclude -Wall -Wextra -Werror -c \
  kernel/kernel.c arch/x86/idt.c arch/x86/irq.c arch/x86/pic.c
```

## Build manual (referência)

Como não há `Makefile`, abaixo está um fluxo manual de referência:

```bash
# 1) objetos .o (C + ASM)
gcc -m32 -ffreestanding -Iinclude -c kernel/kernel.c -o kernel.o
gcc -m32 -ffreestanding -Iinclude -c arch/x86/idt.c -o idt.o
gcc -m32 -ffreestanding -Iinclude -c arch/x86/irq.c -o irq.o
gcc -m32 -ffreestanding -Iinclude -c arch/x86/pic.c -o pic.o

as --32 boot/boot.s -o boot.o
as --32 boot/gdt.s -o gdt.o
as --32 boot/isr.s -o isr.o
as --32 boot/irq.s -o irq_stubs.o
as --32 boot/idt_descriptor.s -o idt_desc.o

# 2) link do kernel ELF
ld -m elf_i386 -T linker.ld -o kernel.elf \
  boot.o gdt.o isr.o irq_stubs.o idt_desc.o kernel.o idt.o irq.o pic.o

# 3) (opcional) gerar ISO com GRUB
mkdir -p iso/boot/grub
cp kernel.elf iso/boot/kernel.elf
cp grub.cfg iso/boot/grub/grub.cfg
grub-mkrescue -o kernel.iso iso

# 4) (opcional) executar
qemu-system-i386 -cdrom kernel.iso
```

## Próximos passos sugeridos

- Adicionar `Makefile` (build, iso, run, clean).
- Criar abstração simples de terminal VGA (newline, scroll, cores).
- Adicionar driver de teclado (IRQ1) e timer (IRQ0) com contadores.
- Evoluir tratamento de exceções (mensagens mais detalhadas por vetor).
- Integrar CI com checagem de build freestanding.

## Licença

Consulte o arquivo [LICENSE](LICENSE).
