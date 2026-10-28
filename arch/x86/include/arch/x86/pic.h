#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Remapeia as bases de vetores na IDT para o PIC Master (offset1) e Slave (offset2) */
void pic_remap(uint8_t offset1, uint8_t offset2);

/* Envia o sinal EOI (End of Interrupt) para os controladores autorizarem novas IRQs */
void pic_send_eoi(uint8_t irq);

/* Mascara (desativa) todas as linhas de IRQ nos dois controladores */
void pic_mask_all(void);

/* Desmascara (ativa) uma linha de IRQ específica limpando seu bit no IMR */
void pic_unmask_irq(uint8_t irq);

#endif