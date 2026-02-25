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

## Testar no hardware físico

Após gravar a ISO em mídia de boot e iniciar a máquina:

1. No shell, execute `help`, `ticks`, `task`, `pmm` e `echo teste-hw`.
2. Valide que:
   - `ticks` aumenta entre chamadas (timer/IRQ0 ativo);
   - teclado responde corretamente com `Shift` e `CapsLock`;
   - shell continua responsivo sem travamentos.
3. Execute um teste de exceção por boot e reinicie:
   - `panic int3`
   - `panic ud2`
   - `panic div0`
   - `panic null`

Resultado esperado: tela de panic consistente com halt controlado.

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
