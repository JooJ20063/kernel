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
- `panic null`: tenta escrita em ponteiro nulo (esperado `#PF`).
- `panic int <n>`: panic manual com metadata de interrupção.

## Limitações atuais

- Sem histórico de comandos.
- Sem edição avançada de linha.
- Parser simples (sem quoting/escaping).


## Segurança no comando panic

Os modos como `panic div0`, `panic ud2` e `panic int3` são ferramentas de teste de falhas controladas.

Quando disparados:

1. o kernel captura o evento por ISR;
2. chama o handler de panic central;
3. imprime contexto (registradores/causa);
4. entra em halt seguro.

Isso evita estado indefinido e facilita depuração reproduzível.
