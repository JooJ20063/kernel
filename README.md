# kernel

Kernel educacional em **C + Assembly (x86 32-bit)** com:
- IDT/ISRs/IRQs
- PIC + PIT
- teclado PS/2 (ABNT2 base, Shift/CapsLock)
- PMM (bitmap de frames)
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
- **PMM**: alocador/liberador de frames físicos de 4 KiB.

## Comandos do shell

- `help`
- `clear`
- `ticks`
- `task`
- `pmm`
- `echo <texto>`
- `panic`

## Uso rápido

```bash
make            # build (objetos em build/)
make check      # checagem freestanding dos .c
make iso        # gerar ISO (requer grub-mkrescue)
make run        # rodar no qemu via CD
make clean
```

## CI

Há workflow em `.github/workflows/build.yml` rodando:
- `make`
- `make check`

## Observações

- `grub.cfg` espera `/boot/kernel.bin`.
- `make iso` depende de `grub-mkrescue` no host.
