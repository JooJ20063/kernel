#include <kernel/tty.h>
#include <kernel/vga.h>
#include <kernel/task.h>

/* Capacidade do buffer circular de entrada do terminal (256 bytes) */
#define TTY1_INPUT_CAPACITY 256U

/* Nó de arquivo VFS e buffers/estados do terminal TTY1 */
static fs_node_t tty1_fs_node;
static uint8_t tty1_input[TTY1_INPUT_CAPACITY];
static volatile uint32_t tty1_head;
static volatile uint32_t tty1_tail;
static volatile uint32_t tty1_count;
static volatile uint32_t tty1_drop_count;
static volatile tty_input_focus_t tty1_focus;
static volatile uint32_t tty1_foreground;
static volatile tty_mode_t tty1_line_mode;
static volatile uint8_t tty1_echo;
static volatile uint32_t tty1_ready_lines;
static volatile uint32_t tty1_current_line_len;
static wait_queue_t tty1_read_waiters;

/**
 * Salva a palavra de FLAGS da CPU (incluindo o bit IF) na pilha e desativa interrupções de hardware (cli).
 * Garante seções críticas atômicas contra concorrência com IRQs de teclado.
 */
static uint32_t tty_irq_save_disable(void) {
    uint32_t flags;

    asm volatile (
        "pushf\n"
        "pop %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

/**
 * Restaura o estado anterior das interrupções checando o bit 9 (IF - Interrupt Flag) da palavra de FLAGS salva.
 */
static void tty_irq_restore(uint32_t flags) {
    if ((flags & (1U << 9)) != 0U) {
        asm volatile ("sti" : : : "memory");
    }
}

/* Callback de abertura/fechamento nulo para o nó do TTY no VFS */
static void tty1_noop(fs_node_t *node) {
    (void)node;
}

/* Copia string limitando o número de caracteres para evitar estouro de buffer */
static void str_copy_limit(char *dst, const char *src, uint32_t limit) {
    uint32_t i = 0U;

    if (limit == 0U) {
        return;
    }

    while (src[i] != 0 && i + 1U < limit) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

/* Reseta os ponteiros e contadores do buffer circular de entrada (deve ser chamado dentro de seção crítica) */
static void tty1_reset_input_locked(void) {
    tty1_head = 0U;
    tty1_tail = 0U;
    tty1_count = 0U;
    tty1_ready_lines = 0U;
    tty1_current_line_len = 0U;
}

/* Apaga visualmente o último caractere impresso na tela no modo eco */
static void tty1_echo_erase(void) {
    vga_puts("\b \b");
}

/* Condição de desbloqueio para tarefas aguardando leitura na fila de espera (wait queue) */
static int tty1_input_ready(void *ctx) {
    const uint32_t *pid = (const uint32_t *)ctx;

    if (pid == 0) {
        return 0;
    }

    /* Apenas o processo que está em primeiro plano (foreground) pode consumir a entrada do TTY */
    if (tty1_foreground != *pid) {
        return 0;
    }

    /* No modo canônico, exige ao menos uma linha completa (terminada em '\n'); no modo Raw, exige ao menos 1 byte */
    if (tty1_line_mode == TTY_MODE_CANONICAL) {
        return tty1_ready_lines > 0U;
    }

    return tty1_count > 0U;
}

/**
 * Callback de leitura do VFS (/dev/tty1).
 * Consome os caracteres do buffer circular se o processo chamador for o dono do primeiro plano.
 * Caso contrário, bloqueia a tarefa em uma wait queue até que novos caracteres cheguem ou o foco mude.
 */
static uint32_t tty1_read(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    uint8_t *buffer
) {
    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    if (size == 0U) {
        return 0U;
    }

    {
        uint32_t caller_pid = sched_current_pid();

        for (;;) {
            uint32_t read_count = 0U;
            uint32_t flags = tty_irq_save_disable();

            /* Processa a leitura apenas se a tarefa ativa for a detentora do primeiro plano (Foreground PID) */
            if (tty1_foreground == caller_pid) {
                if (tty1_line_mode == TTY_MODE_CANONICAL) {
                    /* Modo Canônico: lê até esgotar o tamanho solicitado ou até encontrar a quebra de linha '\n' */
                    if (tty1_ready_lines > 0U) {
                        while (read_count < size && tty1_count > 0U) {
                            uint8_t c = tty1_input[tty1_tail];

                            tty1_tail =
                                (tty1_tail + 1U) % TTY1_INPUT_CAPACITY;
                            tty1_count--;
                            buffer[read_count++] = c;

                            if (c == (uint8_t)'\n') {
                                tty1_ready_lines--;
                                break;
                            }
                        }
                    }
                } else {
                    /* Modo Raw: consome imediatamente todos os caracteres disponíveis até o limite de 'size' */
                    while (read_count < size && tty1_count > 0U) {
                        buffer[read_count++] = tty1_input[tty1_tail];
                        tty1_tail =
                            (tty1_tail + 1U) % TTY1_INPUT_CAPACITY;
                        tty1_count--;
                    }
                }
            }

            tty_irq_restore(flags);

            if (read_count > 0U) {
                return read_count;
            }

            /* Bloqueia a tarefa se o buffer não contiver dados prontos ou se ela for um processo em background */
            if (task_wait_until(
                    &tty1_read_waiters,
                    tty1_input_ready,
                    &caller_pid) != 0) {
                return 0U; // Retorno não-bloqueante reservado para tarefas do sistema/bootstrap
            }
        }
    }
}

/* Callback de escrita do VFS (/dev/tty1): redireciona os bytes para exibição direta na tela via driver VGA */
static uint32_t tty1_write(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    const uint8_t *buffer
) {
    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    for (uint32_t i = 0U; i < size; ++i) {
        vga_putc((char)buffer[i]);
    }

    return size;
}

/* Inicializa as estruturas do terminal virtual TTY1, nós do VFS e filas de espera */
void tty1_init(void) {
    for (uint32_t i = 0U; i < (uint32_t)sizeof(tty1_fs_node); ++i) {
        ((uint8_t *)&tty1_fs_node)[i] = 0U;
    }

    str_copy_limit(
        tty1_fs_node.name,
        "tty1",
        sizeof(tty1_fs_node.name)
    );

    tty1_fs_node.flags = FS_FILE | FS_WRITABLE;
    tty1_fs_node.read = tty1_read;
    tty1_fs_node.write = tty1_write;
    tty1_fs_node.open = tty1_noop;
    tty1_fs_node.close = tty1_noop;

    tty1_reset_input_locked();
    tty1_drop_count = 0U;
    tty1_focus = TTY_INPUT_FOCUS_SHELL;
    tty1_foreground = 0U;
    tty1_line_mode = TTY_MODE_RAW;
    tty1_echo = 0U;
    wait_queue_init(&tty1_read_waiters);
}

/**
 * Injeta um caractere lido pela interrupção do teclado PS/2 ou Serial no buffer do TTY1.
 * Processa caracteres de edição no modo Canônico e acorda tarefas bloqueadas na leitura.
 */
void tty1_receive_char(char c) {
    uint32_t flags = tty_irq_save_disable();

    if (c == '\r') {
        c = '\n';
    }

    if (tty1_line_mode == TTY_MODE_CANONICAL) {
        /* Tratamento de Backspace no modo Canônico: remove o último caractere da linha atual */
        if (c == '\b') {
            if (tty1_current_line_len > 0U) {
                tty1_head =
                    (tty1_head + TTY1_INPUT_CAPACITY - 1U) %
                    TTY1_INPUT_CAPACITY;
                tty1_count--;
                tty1_current_line_len--;

                if (tty1_echo != 0U) {
                    tty1_echo_erase();
                }
            }

            tty_irq_restore(flags);
            return;
        }

        /* Reserva um slot para garantir a inclusão do '\n' ao fechar a linha no buffer */
        if (c != '\n' &&
            tty1_count >= (TTY1_INPUT_CAPACITY - 1U)) {
            tty1_drop_count++;
            tty_irq_restore(flags);
            return;
        }

        if (tty1_count >= TTY1_INPUT_CAPACITY) {
            tty1_drop_count++;
            tty_irq_restore(flags);
            return;
        }

        tty1_input[tty1_head] = (uint8_t)c;
        tty1_head = (tty1_head + 1U) % TTY1_INPUT_CAPACITY;
        tty1_count++;

        if (c == '\n') {
            tty1_ready_lines++;
            tty1_current_line_len = 0U;

            if (tty1_echo != 0U) {
                vga_putc('\n');
            }

            /* Acorda os processos leitoes apenas após a confirmação do caractere de nova linha */
            wait_queue_wake_all(&tty1_read_waiters);
        } else {
            tty1_current_line_len++;

            if (tty1_echo != 0U) {
                vga_putc(c);
            }
        }

        tty_irq_restore(flags);
        return;
    }

    /* Processamento de entrada no modo Raw */
    if (tty1_count >= TTY1_INPUT_CAPACITY) {
        tty1_drop_count++;
        tty_irq_restore(flags);
        return;
    }

    tty1_input[tty1_head] = (uint8_t)c;
    tty1_head = (tty1_head + 1U) % TTY1_INPUT_CAPACITY;
    tty1_count++;

    if (tty1_echo != 0U) {
        vga_putc(c);
    }

    /* Acorda todas as tarefas aguardando leitura na wait queue */
    wait_queue_wake_all(&tty1_read_waiters);
    tty_irq_restore(flags);
}

/* Limpa e descarta com segurança todo o conteúdo pendente no buffer de entrada do TTY */
void tty1_flush_input(void) {
    uint32_t flags = tty_irq_save_disable();

    tty1_reset_input_locked();

    tty_irq_restore(flags);
}

/* Define o direcionamento da entrada do teclado entre o Shell do Kernel e o TTY1 */
void tty1_set_input_focus(tty_input_focus_t focus) {
    uint32_t flags = tty_irq_save_disable();

    if (focus == TTY_INPUT_FOCUS_TTY1) {
        tty1_focus = TTY_INPUT_FOCUS_TTY1;
    } else {
        tty1_focus = TTY_INPUT_FOCUS_SHELL;
        tty1_foreground = 0U;
        wait_queue_wake_all(&tty1_read_waiters);
    }

    tty_irq_restore(flags);
}

/* Retorna a entidade que possui o foco atual de entrada */
tty_input_focus_t tty1_input_focus(void) {
    return tty1_focus;
}

/* Atribui o PID do processo em primeiro plano e transfere o foco do console para o TTY1 */
void tty1_set_foreground_pid(uint32_t pid) {
    uint32_t flags = tty_irq_save_disable();

    tty1_foreground = pid;

    if (pid != 0U) {
        tty1_focus = TTY_INPUT_FOCUS_TTY1;
    }

    wait_queue_wake_all(&tty1_read_waiters);
    tty_irq_restore(flags);
}

/* Retorna o PID do processo em primeiro plano */
uint32_t tty1_foreground_pid(void) {
    uint32_t flags = tty_irq_save_disable();
    uint32_t pid = tty1_foreground;

    tty_irq_restore(flags);
    return pid;
}

/* Libera o controle do primeiro plano se o PID fornecido for o detentor do TTY */
int tty1_release_foreground(uint32_t pid) {
    uint32_t flags;
    int released = 0;

    if (pid == 0U) {
        return 0;
    }

    flags = tty_irq_save_disable();

    if (tty1_foreground == pid) {
        tty1_foreground = 0U;
        tty1_focus = TTY_INPUT_FOCUS_SHELL;
        wait_queue_wake_all(&tty1_read_waiters);
        released = 1;
    }

    tty_irq_restore(flags);
    return released;
}

/* Modifica o modo de operação entre Raw e Canônico e limpa os buffers de dados anteriores */
void tty1_set_mode(tty_mode_t mode) {
    uint32_t flags = tty_irq_save_disable();

    tty1_line_mode =
        (mode == TTY_MODE_CANONICAL)
            ? TTY_MODE_CANONICAL
            : TTY_MODE_RAW;

    tty1_reset_input_locked();
    wait_queue_wake_all(&tty1_read_waiters);

    tty_irq_restore(flags);
}

/* Retorna o modo de operação ativo no terminal */
tty_mode_t tty1_mode(void) {
    return tty1_line_mode;
}

/* Configura o eco automático de caracteres para o monitor VGA */
void tty1_set_echo(int enabled) {
    uint32_t flags = tty_irq_save_disable();

    tty1_echo = enabled ? 1U : 0U;

    tty_irq_restore(flags);
}

/* Retorna se o eco está ativado */
int tty1_echo_enabled(void) {
    return tty1_echo != 0U;
}

/* Retorna a quantidade de bytes pendentes de leitura no buffer circular */
uint32_t tty1_pending(void) {
    uint32_t flags = tty_irq_save_disable();
    uint32_t count = tty1_count;

    tty_irq_restore(flags);
    return count;
}

/* Retorna o total de caracteres descartados por estouro do buffer circular de entrada */
uint32_t tty1_dropped(void) {
    return tty1_drop_count;
}

/* Retorna o ponteiro para o nó de arquivo (fs_node_t) do dispositivo /dev/tty1 */
fs_node_t *tty1_node(void) {
    return &tty1_fs_node;
}