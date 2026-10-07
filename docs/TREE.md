# Source Tree

A Tree v2 separa responsabilidade arquitetural, núcleo, drivers, runtime e ABI pública.

```text
arch/
├── x86/
│   ├── boot/
│   ├── cpu/
│   ├── include/
│   └── linker.ld
└── x86_64/
    ├── boot/
    ├── cpu/
    ├── experimental/
    ├── include/
    ├── mm/
    └── linker.ld

boot/
└── grub/

drivers/
└── console/

kernel/
├── core/
├── debug/
├── fs/
├── mm/
├── sched/
└── syscall/

libk/
├── include/
└── memory.c

tests/
└── ring3/

uapi/
└── include/czk/

mk/
├── x86.mk
└── image.mk
```

## Regras

- `arch/` contém implementação dependente da arquitetura.
- `kernel/` contém subsistemas conceitualmente independentes da arquitetura.
- `drivers/` contém código que conversa diretamente com dispositivos.
- `libk/` contém primitivas C freestanding usadas pelo kernel.
- `uapi/` contém somente contratos que podem ser compartilhados com userspace.
- `tests/ring3/` contém o scaffolding Ring 3 atualmente linkado ao kernel para validação; não representa o userspace definitivo.
- O userspace real deverá ser carregado como binário separado quando o ELF loader estiver disponível.
