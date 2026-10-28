#include <arch/x86/fpu.h>
#include <kernel/kmalloc.h>
#include <kernel/task.h>
#include <kernel/vga.h>

/* Ponteiro para a tarefa, que, atualmente detem o estado ativo do hardware de FPU */
static task_t *fpu_owner = 0;

/* Alinhamento de endereço para múltiplos de 16 bytes (Requisito do FXSAVE/FXRSTOR)*/
static uintptr_t align16(uintptr_t addr) {
    return (addr + 15) & ~15U;
}

static uint32_t read_cr0(void) {
    uint32_t value;
    asm volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static void write_cr0(uint32_t value) {
    asm volatile ("mov %0, %%cr0" :: "r"(value));
}

static uint32_t read_cr4(void) {
    uint32_t value;
    asm volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

static void write_cr4(uint32_t value) {
    asm volatile ("mov %0, %%cr4" :: "r"(value));
}

/* CLTS: Limpa o bit CR0.TS para permitir uso direto da FPU sem disparar uma exceção indesejada*/
void fpu_clear_ts(void) {
    asm volatile ("clts");
}

/* Define CR0.TS (Task Switched) para forçar exceção #NM no próximo uso*/
void fpu_set_ts(void) {
    uint32_t cr0 = read_cr0();

    cr0 |= (1U << 3); // CR0.TS
    
    write_cr0(cr0);
}

/* Configura registradores do processador para suporte a FPU de hardware e extensões SSE*/
void fpu_init(void) {
    uint32_t cr0 = read_cr0();
    cr0 &= ~(1U << 2); // CR0.EM = 0: não emular FPU
    cr0 |=  (1U << 1); // CR0.MP = 1
    write_cr0(cr0);

    uint32_t cr4 = read_cr4();

    cr4 |= (1U << 9); // CR4.OSFXSR
    cr4 |= (1U << 10); // CR4.OSXMMEEXCPT
    write_cr4(cr4);

    fpu_clear_ts();

    asm volatile ("fninit"); // Reseta a FPU para estado inicial

    fpu_owner = 0;
}

/* Alocação de Context_Buffer da FPU com margem de alinhamento definido*/
void fpu_init_task(task_t *task) {
    if (task == 0) {
        return;
    }

    task->fpu_storage = kmalloc(FPU_SAVE_AREA_SIZE + FPU_SAVE_AREA_ALIGN);

    if (task->fpu_storage == 0) {
        task->fpu_area = 0;
        task->fpu_initialized = 0;
        return;
    }

    task->fpu_area = (uint8_t *)align16((uintptr_t)task->fpu_storage);
    task->fpu_initialized = 0;

}

void fpu_reset_task(task_t *task) {
    if (task == 0) {
        return;
    }

    if (fpu_owner == task) {
        fpu_owner = 0;
    }

    task->fpu_initialized = 0;
    fpu_set_ts();
}

void fpu_free_task(task_t *task) {
    if (task == 0) {
        return;
    }

    if (task->fpu_storage != 0) {
        kfree(task->fpu_storage);
    }
    
    task->fpu_storage = 0;
    task->fpu_area = 0;
    task->fpu_initialized = 0;
}

/**
 * Handler de Exceção #NM (INT 7)
 * Implementação de lazy FPUI CS: adia a troca de registradores
 * de FPU/SSE até que a nova tarefa tente ativamente usar uma instrução da FPU
 */

void fpu_handle_nm(void) {
    task_t *current = sched_current_task_ptr();

    fpu_clear_ts();

    if (current == 0) {
        asm volatile ("fninit");
        return;
    }

    if (fpu_owner == current) {
        return;
    } 
    
    // Salvamento de estado da tarefa anterior (se existir)
    if (fpu_owner != 0 && fpu_owner->fpu_initialized && fpu_owner->fpu_area != 0) {
        asm volatile ("fxsave (%0)" :: "r"(fpu_owner->fpu_area) : "memory");
    } 
    
    // Alocação sob demanda (Lazy) se a tarefa nunca usou 
    if (current->fpu_area == 0) {
        fpu_init_task(current);
    } 
    
    // Fallback caso ocorra falha de alocação
    if (current->fpu_area == 0) {
        vga_puts("[FPU] failed to allocate FPU area\n");
        asm volatile ("fninit");
        fpu_owner = current;
        return;
    } 
    
    // Restauração do estado salvo ou inicialização da FPU para o primeiro uso
    if (current->fpu_initialized)  {
        asm volatile ("fxrstor (%0)" :: "r"(current->fpu_area) : "memory");
    } else {
        asm volatile ("fninit");
        current->fpu_initialized = 1;
    }

    fpu_owner = current;
}

