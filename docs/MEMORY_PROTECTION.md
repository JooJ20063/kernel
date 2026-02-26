# Proteção de Memória

Este documento descreve o estado atual e próximos passos de endurecimento de memória.

## Estado atual

- **PMM** inicializado via **Multiboot2 memory map** (não mais fixo em 64 MiB).
- **VMM/paginação** habilitado no bootstrap com mapeamento identidade inicial (16 MiB).
- Kernel ainda roda em Ring 0 (sem userland isolado).

## Proteções já aplicadas

- Reserva explícita da região baixa (0..1MiB).
- Reserva da memória ocupada pelo kernel (`_kernel_start`..`_kernel_end`).
- Reserva da área de informações do Multiboot2.
- **Null-page guard**: endereço `0x0` fica sem mapeamento virtual para capturar ponteiro nulo via `#PF`.
- **CR0.WP habilitado**: ativa proteção de escrita em páginas marcadas read-only mesmo em Ring 0.
- **`.text`/`.rodata` remapeadas como read-only** após `vmm_init`, usando `vmm_map_page`.

## Próximos passos recomendados

1. Expandir o VMM para além dos 16 MiB iniciais e suportar tabelas sob demanda.
2. Separar espaço virtual de kernel e userland.
3. Introduzir tabelas de páginas por processo e troca de contexto real.
