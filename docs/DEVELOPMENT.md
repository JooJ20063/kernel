# Desenvolvimento

## Target suportado

O target principal é:

```text
ARCH=x86
```

O caminho x86-64 permanece experimental e não faz parte do build suportado nesta fase.

## Build local

```bash
make
make check
```

## Imagem bootável

```bash
make iso
```

O target requer `grub-mkrescue`.

## QEMU

VGA:

```bash
make run
```

Serial:

```bash
make run-serial
```

## Diretórios de headers

O build x86 usa:

```text
include/
libk/include/
uapi/include/
arch/x86/include/
```

## CI

Workflow:

```text
.github/workflows/build.yml
```

O CI:

1. executa sanity checks;
2. compila o kernel;
3. executa `make check`;
4. valida o header Multiboot2 com `grub-file`;
5. produz a ISO com `make iso`.

## Convenções

- C freestanding.
- x86 32-bit como target principal.
- SIMD implícito do compilador desabilitado no kernel x86.
- Objetos gerados sob `build/`.
- Código específico de arquitetura deve permanecer sob `arch/<arch>/`.
- Drivers não devem ser colocados em `kernel/core/`.
- Contratos públicos com userspace pertencem a `uapi/`.
