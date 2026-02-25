# kernel

Kernel educacional em **C + Assembly (x86 32-bit)** com:
- IDT/ISRs/IRQs
- PIC + PIT
- teclado PS/2 (ABNT2 base, Shift/CapsLock)
- PMM (bitmap de frames via Multiboot2 memory map)
- VMM (paginação com mapeamento identidade inicial)
- scheduler round-robin simples
- VGA text mode + logging + shell primitivo

## Estrutura do projeto

```text
.
├── boot/
├── arch/x86/
├── include/arch/x86/
├── include/kernel/
│   ├── vga.h
│   ├── sched.h
│   ├── pmm.h
│   ├── klog.h
│   ├── panic.h
│   └── shell.h
├── kernel/
│   ├── kernel.c
│   ├── vga.c
│   ├── sched.c
│   ├── pmm.c
│   ├── klog.c
│   ├── panic.c
│   └── shell.c
├── .github/workflows/build.yml
├── Makefile
└── grub.cfg
```

## Funcionalidades implementadas

- **Kernel panic** com dump de registradores e halt (`kernel_panic`).
- **Debug logging** com níveis (`DBG`, `INF`, `WRN`, `ERR`).
- **Shell primitivo** (linha de comando no VGA).
- **IRQ0**: timer configurável + tick do scheduler.
- **IRQ1**: teclado com Shift/CapsLock e mapa ABNT2 base.
- **PMM**: alocador/liberador de frames físicos de 4 KiB, inicializado via mapa de memória do Multiboot2.
- **VMM**: paginação habilitada com mapeamento identidade inicial para o bootstrap.

## Comandos do shell

- `help`
- `clear`
- `ticks`
- `task`
- `pmm`
- `echo <texto>`
- `panic`
- `panic int3`
- `panic ud2`
- `panic div0`
- `panic null`
- `panic int <n>`

## Uso rápido

```bash
make            # build (objetos em build/)
make check      # checagem freestanding dos .c
make iso        # gerar ISO (requer grub-mkrescue)
make run        # rodar no qemu via CD
make clean
```

## Testes em hardware real (checklist rápido)

Depois de gerar a ISO (`make iso`) e dar boot na máquina real, rode estes comandos no shell:

1. `help`
   - Esperado: lista de comandos sem travar o kernel.
2. `ticks`
   - Esperado: valor de `ticks` aumentando ao repetir o comando (IRQ0/PIT ativo).
3. `task`
   - Esperado: contador de trocas/scheduler variando com o tempo.
4. `pmm`
   - Esperado: totais de frames coerentes e sem valores negativos.
5. `echo teste-hw`
   - Esperado: texto ecoado corretamente.
6. Digitação normal com e sem `Shift`/`CapsLock`
   - Esperado: mapeamento de teclado funcionando sem caracteres aleatórios.

### Testes de exceção controlada (um por boot)

> Estes comandos entram em panic/halt por design. Execute **um de cada vez** e reinicie entre eles.

- `panic int3` (breakpoint)
- `panic ud2` (invalid opcode)
- `panic div0` (divisão por zero)
- `panic null` (page fault esperado)

Esperado em todos: tela de panic com contexto de registradores e sistema em halt estável.

## CI

Há workflow em `.github/workflows/build.yml` rodando:
- `make clean && make`
- `make check`
- sanity check: definição única de `kernel_main` em `kernel/kernel.c`

## Observações

- `grub.cfg` espera `/boot/kernel.bin`.
- `make iso` depende de `grub-mkrescue` no host.


## Documentação

- [Arquitetura](docs/ARCHITECTURE.md)
- [Shell](docs/SHELL.md)
- [Guia de desenvolvimento](docs/DEVELOPMENT.md)
- [Proteção de memória (plano)](docs/MEMORY_PROTECTION.md)
