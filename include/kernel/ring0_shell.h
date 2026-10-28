#pragma once

/* Inicializa o shell de diagnósticos do kernel executado em Ring 0 */
void ring0_shell_init(void);

/* Processa um caractere de entrada enviado pelos handlers de interrupção (teclado/serial) */
void ring0_shell_on_key(char c);

/* Restaura a captura de comandos do shell após um retorno de foco de entrada (ex: saída do tty1 via F12/Ctrl+]) */
void ring0_shell_resume_input(void);