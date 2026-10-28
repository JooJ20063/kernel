#pragma once

/* Registra mensagens de depuração interna e rastreamento detalhado do Kernel */
void klog_debug(const char *msg);

/* Registra eventos informativos de ciclo de vida e estado normal do sistema */
void klog_info(const char *msg);

/* Registra alertas de condições anômalas não fatais que exigem atenção */
void klog_warn(const char *msg);

/* Registra erros críticos de execução, falhas de subsistemas ou acionamento de falhas */
void klog_error(const char *msg);