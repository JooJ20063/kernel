#include <stdint.h>
#include <kernel/klog.h>
#include <kernel/vga.h>

/**
 * Imprime o prefixo do nível de log delimitado por colchetes [LVL] com a cor especificada
 * e restaura a cor padrão do terminal VGA (Texto Branco/Fundo Preto: 0x0F, 0x00).
 */
static void klog_prefix(const char *lvl, uint8_t fg, uint8_t bg) {
    vga_set_color(fg, bg);
    vga_puts("[");
    vga_puts(lvl);
    vga_puts("] ");
    vga_set_color(0x0F, 0x00); // Restaura atributos de texto padrão
}

/* Registra mensagem de depuração com prefixo [DBG] em Ciano Claro (0x0B) */
void klog_debug(const char *msg) {
    klog_prefix("DBG", 0x0B, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}

/* Registra mensagem informativa com prefixo [INF] em Verde Claro (0x0A) */
void klog_info(const char *msg) {
    klog_prefix("INF", 0x0A, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}

/* Registra mensagem de alerta com prefixo [WRN] em Amarelo Claro (0x0E) */
void klog_warn(const char *msg) {
    klog_prefix("WRN", 0x0E, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}

/* Registra mensagem de erro com prefixo [ERR] em Vermelho Claro (0x0C) */
void klog_error(const char *msg) {
    klog_prefix("ERR", 0x0C, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}