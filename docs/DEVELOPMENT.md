# Desenvolvimento

## Build local

```bash
make
make check
```

## Rodar em QEMU

```bash
make iso
make run
```

> `make iso` requer `grub-mkrescue` no host.

## CI

Workflow: `.github/workflows/build.yml`

Etapas:
- instala `gcc-multilib` e `binutils`
- executa `make`
- executa `make check`

## Convenções atuais

- Código C freestanding com `-m32`.
- Headers em `include/`.
- Objetos em `build/`.
