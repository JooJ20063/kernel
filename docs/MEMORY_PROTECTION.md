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

## Próximos passos recomendados

1. Marcar páginas de `.text` como somente leitura (W=0).
2. Desmapear página zero para capturar `NULL` com `#PF` controlado.
3. Separar espaço virtual de kernel e userland.
4. Introduzir tabelas de páginas por processo e troca de contexto real.
