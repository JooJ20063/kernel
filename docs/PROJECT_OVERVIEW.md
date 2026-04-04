# Visão geral do projeto kernel

Este documento substitui a descrição antiga centrada apenas em x86 32-bit e reflete o estado atual do repositório após os commits mais recentes.

## Resumo

O projeto é um kernel educacional em **C + Assembly** com duas trilhas principais:

- **x86 (32-bit)**: caminho mais maduro e funcional
- **x86_64 (64-bit)**: caminho em evolução, já com bootstrap, IDT, PIC, IRQs e alvo de build expandido

O boot é feito via **GRUB / Multiboot2**, com geração de ISO e execução via QEMU.

## Trilhas por arquitetura

### 1. x86 32-bit
A trilha 32-bit continua sendo a base mais completa do projeto. Ela inclui:

- IDT, ISRs e IRQs
- PIC + PIT
- teclado PS/2 com mapa ABNT2 base
- VGA text mode
- logging
- shell primitivo
- kernel panic com diagnóstico
- PMM
- VMM inicial
- kmalloc/kfree/krealloc
- scheduler simples
- VFS + RAMFS

### 2. x86_64 64-bit
Os commits mais recentes expandiram consideravelmente o caminho 64-bit. Hoje ele inclui:

- `boot/boot64.s`: bootstrap do caminho 64-bit
- `boot/gdt64.s`: GDT usada no bring-up 64-bit
- `linker64.ld`: linker script específico para ELF64
- `arch/x86_64/idt.c`: implementação de IDT 64-bit
- `arch/x86_64/pic.c`: remapeamento e controle do PIC em 64-bit
- `arch/x86_64/irq.c`: camada de IRQs em 64-bit
- `boot/isr64.s`: stubs das exceções em 64-bit
- `boot/irq64.s`: stubs das IRQs em 64-bit
- `kernel/x86_64/kernel.c`: ponto de entrada do kernel 64-bit completo
- `kernel/vmm64_stub.c`: shim temporário para permitir bring-up sem portar ainda o VMM real

## Estado real do x86_64

A documentação correta do projeto precisa deixar explícito que o caminho 64-bit já **existe e compila**, mas ainda não está no mesmo nível funcional da trilha 32-bit.

O ponto-chave é o arquivo `kernel/vmm64_stub.c`, que fornece implementações temporárias para o VMM no caminho 64-bit. Isso permite testar integração de módulos como shell, IRQ, PIC e IDT sem depender ainda de uma implementação final de paginação para long mode.

Em outras palavras:

- o **bring-up 64-bit está ativo**
- o **caminho de interrupções 64-bit existe**
- o **caminho de build 64-bit foi expandido**
- a **memória virtual 64-bit ainda está em fase de transição**

## Build atual

### 32-bit

```bash
make
make iso
make run
make check
```

### 64-bit

```bash
make kernel64.bin
make kernel64_full.bin
make iso64
make run64
make check64
```

## Alvos importantes da Makefile

### `kernel64.bin`
Build mínimo do kernel 64-bit.

### `kernel64_full.bin`
Build ampliado do caminho 64-bit, incluindo:

- núcleo 64-bit em `kernel/x86_64/kernel.c`
- módulos genéricos reutilizados do kernel
- IDT/PIC/IRQ 64-bit
- stubs 64-bit de exceção e interrupção

### `check64`
Checagem de compilação dos arquivos principais do caminho 64-bit, útil para detectar regressões cedo.

## Shell

A base do shell foi expandida e hoje inclui, entre outros, comandos como:

- `help`
- `clear`
- `ticks`
- `task`
- `pmm`
- `vmm`
- `wp`
- `nullguard`
- `kmalloc`
- `kfree`
- `krealloc`
- `kslots`
- `kheap`
- `kheapcheck`
- `ls`
- `cat`
- `touch`
- `echo`
- `panic`
- `arch`
- `shutdown`
- `virt`
- `mapped`
- `unmap`

O comando `arch` diferencia 32-bit e 64-bit por `sizeof(void*)`.

## GRUB

O `grub.cfg` mantém entradas para 32-bit e 64-bit. Quando a ISO 64-bit é gerada, o binário 64-bit é copiado como `/boot/kernel.bin`.

## Interpretação recomendada do repositório

Hoje o projeto deve ser entendido como:

- um kernel educacional **multi-arquitetura em transição**
- com **x86 consolidado como base funcional**
- e **x86_64 em bring-up acelerado**, já com interrupções e build mais completos

## Próximos passos naturais

- portar o VMM real para long mode
- revisar a organização entre `arch/x86` e `arch/x86_64`
- aprofundar o teste de `kernel64_full.bin`
- eventualmente unificar a documentação principal em torno de uma visão multiarch
