#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

/* Modos de alternância do foco de entrada de teclado do terminal */
typedef enum {
    TTY_INPUT_FOCUS_SHELL = 0, // Foco direcionado ao Shell em Ring 0 do Kernel
    TTY_INPUT_FOCUS_TTY1 = 1   // Foco direcionado ao terminal virtual TTY1 (User Space)
} tty_input_focus_t;

/* Modos de operação e processamento de caracteres da TTY */
typedef enum {
    TTY_MODE_RAW = 0,      // Modo Raw: envia os bytes diretamente sem buffer de linha nem interpretação de caracteres especiais
    TTY_MODE_CANONICAL = 1 // Modo Canônico: processa linha por linha (espera \n) e suporta caracteres de edição (ex: backspace)
} tty_mode_t;

/* Inicializa o terminal virtual TTY1 e suas filas de buffer de entrada/saída */
void tty1_init(void);

/* Handler chamado pelas interrupções de teclado para injetar um caractere lido no buffer da TTY */
void tty1_receive_char(char c);

/* Descarta e limpa todos os dados pendentes no buffer de entrada da TTY1 */
void tty1_flush_input(void);

/* Configura e alterna o destino do foco de entrada do teclado (Shell do Kernel vs TTY1) */
void tty1_set_input_focus(tty_input_focus_t focus);

/* Retorna a entidade que possui o foco atual de entrada de teclado */
tty_input_focus_t tty1_input_focus(void);

/* Define o PID do processo que está executando em primeiro plano (Foreground Process) na TTY */
void tty1_set_foreground_pid(uint32_t pid);

/* Retorna o PID do processo que detém o controle do primeiro plano na TTY */
uint32_t tty1_foreground_pid(void);

/* Libera o controle do primeiro plano caso o PID informado corresponda ao processo ativo */
int tty1_release_foreground(uint32_t pid);

/* Ajusta o modo de operação do terminal entre Raw e Canônico */
void tty1_set_mode(tty_mode_t mode);

/* Retorna o modo de operação atual do terminal */
tty_mode_t tty1_mode(void);

/* Ativa ou desativa o eco automático na tela dos caracteres digitados no teclado */
void tty1_set_echo(int enabled);

/* Verifica se o recurso de eco de caracteres está ativado */
int tty1_echo_enabled(void);

/* Retorna a quantidade de bytes atualmente pendentes de leitura no buffer de entrada da TTY */
uint32_t tty1_pending(void);

/* Retorna a quantidade de caracteres descartados por estouro de capacidade do buffer (overflow) */
uint32_t tty1_dropped(void);

/* Retorna o nó de arquivo (fs_node_t) do VFS associado ao dispositivo TTY (/dev/tty1) */
fs_node_t *tty1_node(void);