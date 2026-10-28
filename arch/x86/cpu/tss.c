#include <arch/x86/tss.h>

extern uint64_t gdt_tss; // Aponta para a entrada da TSS dentro da GDT
static tss_entry_t tss;

/* Zeramento de memória auxiliar (substituto para memset no contexto do kernel) */
static void mem_zero(void *ptr, uint32_t size) {
    uint8_t *p = (uint8_t *)ptr;

    for (uint32_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

/**
 * Constrói o descritor de segmento de 64 bits da TSS e o instala na GDT.
 * Empacota a base de 32 bits e o limite nos campos fragmentados do padrão de descritores da Intel.
 */
static void tss_install_descriptor(void) {
    uint32_t base = (uint32_t)(uintptr_t)&tss;
    uint32_t limit = sizeof(tss) - 1U;
    uint64_t desc = 0;

    desc |= (uint64_t)(limit & 0xFFFFU);                      // Limit 0..15
    desc |= (uint64_t)(base & 0xFFFFU) << 16;                 // Base 0..15
    desc |= (uint64_t)((base >> 16) & 0xFFU) << 32;           // Base 16..23

    /* 0x89 = P (Present=1), DPL=00 (Ring 0), Type=1001 (Available 32-bit TSS) */
    desc |= (uint64_t)0x89U << 40;

    desc |= (uint64_t)((limit >> 16) & 0x0FU) << 48;          // Limit 16..19
    desc |= (uint64_t)((base >> 24) & 0xFFU) << 56;           // Base 24..31

    gdt_tss = desc;
}

/* Lê o seletor da TSS carregado no registrador TR (Task Register) usando a instrução 'str' */
uint16_t tss_get_selector(void) {
    uint16_t selector;

    asm volatile (
        "str %0"
        : "=r"(selector)
    );

    return selector;
}

/**
 * Atualiza a pilha de Kernel (ESP0) usada pelo hardware durante interrupções/syscalls
 * quando a CPU realiza a transição de User Mode (Ring 3) para Kernel Mode (Ring 0).
 */
void tss_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}

/**
 * Inicializa a TSS de software, configura os seletores do Kernel e
 * carrega o registrador TR da CPU (Task Register).
 */
void tss_init(void) {
    mem_zero(&tss, sizeof(tss_entry_t));

    tss.ss0 = 0x10; // Segmento de Dados do Kernel na GDT (0x10 = Offset do Kernel Data)
    tss.esp0 = 0;   // Será preenchido dinamicamente na troca de tarefas (tss_set_kernel_stack)
    
    // Desativa a I/O Permission Bitmap apontando o deslocamento para fora dos limites da TSS
    tss.iomap_base = sizeof(tss_entry_t);

    tss_install_descriptor();

    /* 
     * Carrega o registrador TR (Task Register) com o seletor 0x28 (offset do descritor da TSS na GDT).
     * A instrução LTR valida o descritor e marca a TSS como Busy.
     */
    asm volatile(
        "mov $0x28, %%ax\n"
        "ltr %%ax\n"
        :
        :
        : "ax", "memory"
    );
}