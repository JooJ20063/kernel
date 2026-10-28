#pragma once

#include <stdint.h>

/**
 * Estrutura exata da Task State Segment (TSS) de 32 bits especificada pela Intel (104 bytes).
 * Em kernels modernos que usam software context switching, a maioria dos campos de hardware
 * fica não utilizada — o campo crítico é 'esp0', usado nas transições User Mode -> Kernel Mode.
 */
typedef struct tss_entry {
    uint32_t prev_tss;     // Link para a TSS anterior se usasse hardware task switching (troca via CALL/JMP)
    
    /* Pilha de Kernel (Ring 0) usada quando ocorre uma interrupção/syscall a partir do Ring 3 */
    uint32_t esp0;
    uint32_t ss0;          // Seletor de segmento de dados do Kernel (geralmente 0x10)

    /* Pilhas para Ring 1 e Ring 2 (raramente utilizados no x86) */
    uint32_t esp1;
    uint32_t ss1;

    uint32_t esp2;
    uint32_t ss2;

    /* Campos de estado de execução salvos/restaurados em hardware task switching */
    uint32_t cr3;          // Endereço do Page Directory (CR3)
    uint32_t eip;
    uint32_t eflags;

    /* Registradores de uso geral */
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;

    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    /* Registradores de segmento de dados e código */
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;

    uint32_t ldt;          // Seletor de segmento da LDT (Local Descriptor Table)

    uint16_t trap;         // Bit 0: dispara exceção de debug (#DB) em trocas de tarefa por hardware
    uint16_t iomap_base;   // Offset do I/O Permission BitMap a partir do início da TSS
} __attribute__((packed)) tss_entry_t;

/* Inicializa a TSS, configura o descritor na GDT e carrega o registrador TR via 'ltr' */
void tss_init(void);

/* Atualiza o ponteiro da pilha do Kernel (ESP0) para a tarefa atualmente em execução */
void tss_set_kernel_stack(uint32_t stack_top);

/* Retorna o seletor da TSS atualmente carregado no registrador TR via 'str' */
uint16_t tss_get_selector(void);