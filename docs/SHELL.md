# Shell primitivo

## Objetivo
Fornecer uma interface mínima para inspeção/debug em runtime.

## Comandos

- `help`: lista comandos.
- `clear`: limpa tela VGA.
- `ticks`: mostra ticks, segundos e HZ do timer.
- `task`: mostra tarefa atual e switches do scheduler.
- `pmm`: mostra total/livres de frames do PMM.
- `vmm`: estado da paginação e do bit CR0.WP.
- `wp`: mostra apenas estado do CR0.WP.
- `nullguard`: lembrete de teste do null-page guard.
- `kheap`: estado do heap do kernel (`kmalloc`).
- `kmalloc <bytes>`: aloca memória no heap de kernel, escreve padrão de teste e imprime endereço.
- `ls`: lista entradas do RAMFS no VFS raiz.
- `cat <arquivo>`: lê arquivo do RAMFS via VFS.
- `touch <arquivo>`: cria arquivo vazio no RAMFS (se não existir).
- `echo <texto> > <arquivo>`: cria/sobrescreve arquivo no RAMFS.
- `cat > <arquivo> <texto>`: atalho de escrita usando `cat` em modo simples.
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
# 📑 Especificação da Interface de Controle (Shell)

O Shell do sistema é uma interface de linha de comando (CLI) minimalista, operando em Ring 0, projetada para diagnóstico em tempo real, auditoria de recursos de hardware e testes de resiliência do Kernel.

---

## 1. Filosofia de Design e Segurança

Diferente de shells de alto nível, esta interface foi construída com foco em estabilidade absoluta:

* **Sanitização de Entrada:** O buffer de entrada (`SHELL_BUF`) é rigorosamente limitado a 128 bytes. O sistema ignora qualquer tentativa de escrita além deste limite, prevenindo ataques de *Stack Smashing*.
* **Parser Determinístico:** A execução ocorre apenas após a detecção do caractere de terminação (`\n`). O parser utiliza comparações binárias diretas, evitando dependências externas complexas.
* **Comunicação Direta com o Kernel:** O Shell atua como um supervisor, extraindo dados diretamente dos subsistemas PMM, Scheduler e IRQ.

---

## 2. Dicionário de Comandos

### 🛠️ Gestão e Diagnóstico do Sistema
| Comando | Descrição Técnica | Objetivo de Auditoria |
| :--- | :--- | :--- |
| `help` | Lista os comandos registrados. | Verificação de disponibilidade de rotinas. |
| `clear` | Reseta o cursor e limpa o buffer VGA. | Reinicialização da interface visual. |
| `ticks` | Exibe Ticks, Segundos e frequência Hz. | Validação da precisão do Timer (IRQ0). |
| `task` | Identifica a tarefa e context switches. | Monitorização da saúde do Scheduler. |
| `pmm` | Reporta o estado da memória física. | Verificação de fugas de memória (frames). |
| `vmm` | Exibe estado do paging e CR0.WP. | Auditoria de hardening da memória no Ring 0. |
| `wp` | Mostra apenas CR0.WP. | Confirma proteção de escrita em páginas RO. |
| `nullguard` | Explica teste de null-page guard. | Verifica captura de ponteiro nulo via `panic null`. |
| `kheap` | Exibe bytes usados/mapeados no heap. | Inspeção do uso de `kmalloc`. |
| `kmalloc <n>` | Aloca `n` bytes e faz write-test. | Teste positivo de mapeamento e escrita de heap. |
| `ls` | Lista os nós montados no RAMFS. | Verifica parser TAR e enumeração de diretório. |
| `cat <arquivo>` | Lê arquivo via `read_fs`. | Verifica caminho de leitura VFS->RAMFS. |
| `touch <arquivo>` | Cria arquivo vazio no RAMFS. | Valida criação dinâmica de nós no VFS. |
| `echo <txt> > <arquivo>` | Escreve/sobrescreve conteúdo textual. | Valida caminho de escrita VFS->RAMFS. |
| `cat > <arquivo> <txt>` | Escrita simplificada via comando `cat`. | Exercita criação/escrita por fluxo alternativo. |

### 📝 Utilitários de Output
| Comando | Descrição | Comportamento |
| :--- | :--- | :--- |
| `echo <txt>` | Imprime a string no buffer VGA. | Validação da rotina de scroll e cursor. |

---

## 3. Vetores de Teste de Resiliência (`panic`)

O comando `panic` não é apenas um encerramento; é uma ferramenta de teste para validar se a **IDT (Interrupt Descriptor Table)** está a capturar corretamente as exceções de hardware.

| Subcomando | Exceção Gatilhada | Finalidade do Teste |
| :--- | :--- | :--- |
| `panic` | Invocação Direta | Testa o fluxo de halting e dump de registos. |
| `panic int3` | **#BP** (Breakpoint) | Valida interrupções de depuração. |
| `panic ud2` | **#UD** (Invalid Opcode) | Testa a reação a código corrompido/malicioso. |
| `panic div0` | **#DE** (Divide Error) | Garante que erros lógicos não travem a CPU. |
| `panic int <n>`| Vetor Genérico | Auditoria de qualquer entrada da IDT via software. |

---

## 4. Cadeia de Custódia de Dados (Input)

Para garantir que o Shell seja resiliente, o dado percorre o seguinte fluxo:
1.  **Hardware:** O controlador PS/2 dispara a `IRQ1`.
2.  **Kernel:** O handler lê a porta `0x60` (Scancode).
3.  **Abstração:** O tradutor ABNT2 processa modificadores (`Shift`/`Caps`).
4.  **Interface:** O caractere é enviado para o buffer do Shell, onde aguarda o processamento atómico.

---

## 5. Limitações Atuais e Roadmap

### Limitações (Versão Atual):
* **Execução em Ring 0:** O Shell partilha o espaço de privilégio do Kernel.
* **Buffer Estático:** Sem suporte para histórico de comandos (Command History).
* **Parser Simples:** Não suporta argumentos entre aspas ou escapes.

### Evolução Planeada:
1.  **Isolamento em Ring 3:** Transição do Shell para o Espaço do Utilizador.
2.  **Syscall Interface:** Toda a comunicação com o hardware passará a ser feita via interrupção `0x80`.
3.  **Argument Validation:** Implementação de verificação de tipos para comandos como `panic int`.
