# Shell primitivo

## Objetivo
Fornecer uma interface mínima para inspeção/debug em runtime.

## Comandos

- `help`: lista comandos.
- `clear`: limpa tela VGA.
- `ticks`: mostra ticks, segundos e HZ do timer.
- `task`: mostra tarefa atual e switches do scheduler.
- `pmm`: mostra total/livres de frames do PMM.
- `echo <texto>`: imprime texto.
- `panic`: aciona panic manual.
- `panic int3`: dispara breakpoint exception.
- `panic ud2`: dispara invalid opcode.
- `panic div0`: dispara divisão por zero.
- `panic int <n>`: panic manual com metadata de interrupção.

## Limitações atuais

- Sem histórico de comandos.
- Sem edição avançada de linha.
- Parser simples (sem quoting/escaping).
