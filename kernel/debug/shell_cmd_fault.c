#include "shell_internal.h"
#include <kernel/klog.h>
#include <kernel/panic.h>
#ifdef __x86_64__
#include <arch/x86_64/regs.h>
#else
#include <arch/x86/regs.h>
#endif
#include <stdint.h>

/* Força recursão infinita e estouro de pilha (Stack Overflow) para teste da Guard Page */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
__attribute__((noinline, noreturn, disable_tail_calls))
#else
__attribute__((noinline, noreturn))
#endif
static void force_stack_overflow(uint32_t depth) {
    volatile uint8_t buffer[4096];

    buffer[0] = (uint8_t)depth;
    buffer[4095] = (uint8_t)(depth >> 8);

    asm volatile ("" : : "g"(buffer) : "memory");

    force_stack_overflow(depth + 1);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/* Comando: pfault — Provoca falhas de página deliberadas para validação do handler #PF */
void shell_cmd_pfault(const char *arg) {
    if (arg == 0 || *arg == 0){
        klog_warn("usage: pfault [null|readonly|invalid|stack]");
        return;
    }

    if (shell_str_eq(arg, "null")) {
        volatile uint32_t *p = (volatile uint32_t *)0x0;
        *p = 0xDEADBEEFU;
        return;
    }

    if (shell_str_eq(arg, "readonly")) {
        extern uint8_t _rodata_start;
        volatile uint32_t *p = (volatile uint32_t *)&_rodata_start;
        *p = 0xCAFEBABEU;
        return;
    }

    if (shell_str_eq(arg, "invalid")) {
        volatile uint32_t *p = (volatile uint32_t *)0xA000000U;
        *p = 0x12345678U;
        return;
    }

    if (shell_str_eq(arg, "stack")) {
        force_stack_overflow(0);
        return;
    }

    klog_warn("usage: pfault [null|readonly|invalid|stack]");
}

/* Comando: panic — Provoca parada de emergência do Kernel por diferentes causas */
void shell_cmd_panic(const char *arg) {
    if (arg == 0 || *arg == 0) {
        kernel_panic("panic command invoked", 0);
        return;
    }

    if (shell_str_eq(arg, "int3")) {
        asm volatile ("int3");
        return;
    }

    if (shell_str_eq(arg, "ud2")) {
        asm volatile ("ud2");
        return;
    }

    if (shell_str_eq(arg, "div0")) {
        klog_warn("div0: disabled - use 'panic ud2' or 'panic int3' instead");
        return;
    }

    if (shell_str_eq(arg, "null")) {
        volatile uint32_t *pnull = (volatile uint32_t *)0x0;
        *pnull = 0xDEADBEEFU;
        return;
    }

    if (shell_str_starts(arg, "int ")) {
        int ok;
        uint32_t int_no = shell_parse_u32(arg + 4, &ok);
        if (ok) {
            registers_t fake = {0};
            fake.int_no = int_no;
            fake.err = 0;
            kernel_panic("manual interrupt panic", &fake);
            return;
        }
    }

    klog_warn("panic usage: panic [int3|ud2|div0|null|int <n>]");
}
