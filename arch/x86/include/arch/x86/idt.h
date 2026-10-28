#pragma once
#include <stdint.h>

/**
 * Estrutura de uma entrada (Interrupt Gate Descriptor) na IDT de 32 bits (8 bytes).
 * __attribute__((packed)) impede que o compilador insira padding entre os campos.
 */
struct idt_entry {
    uint16_t    offset_low;   // Bits 0..15 do endereço da rotina de tratamento (ISR)
    uint16_t    selector;     // Seletor de segmento de código na GDT (ex: Kernel Code = 0x08)
    uint8_t     zero;         // Campo reservado exigido pela Intel (deve ser sempre 0)
    uint8_t     type_attr;    // Flags: Present (1 bit), DPL (2 bits), Storage (1 bit), Gate Type (4 bits)
    uint16_t    offset_high;  // Bits 16..31 do endereço da rotina de tratamento (ISR)
} __attribute__((packed));

/**
 * Ponteiro do descritor da IDT (6 bytes) passado diretamente para a instrução 'LIDT'.
 */
struct idt_ptr {
    uint16_t    limit; // Tamanho total da IDT em bytes menos 1 (ex: sizeof(idt) - 1)
    uint32_t    base;  // Endereço de memória linear onde a tabela 'idt' está alocada
} __attribute__((packed));

/* Preenche a tabela com um handler padrão e carrega o registrador IDTR via LIDT */
void idt_init(void);

/* Instala os handlers das exceções reservadas da CPU (0..31) e syscalls (ex: 128 / 0x80) */
void idt_install_isrs(void);

/* Instala os handlers das interrupções de hardware do PIC remapeado (32..47 e yield) */
void idt_install_irqs(void);