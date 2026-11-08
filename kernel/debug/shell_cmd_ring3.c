#include "shell_internal.h"
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/sched.h>

#ifndef __x86_64__
extern void user_test_entry(void);
extern void user_fault_test_entry(void);
extern void user_ud_test_entry(void);
extern void user_gp_test_entry(void);
extern void user_aspace_test_entry(void);
extern uint8_t user_stack_top;

/* Criacao de tarefas de teste em Ring 3; implementacoes originais preservadas. */

void shell_cmd_ring3test(void) {
        int pid;

        pid = sched_create_user_task(
            "ring3-test",
            user_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 task");
            return;
        }

        vga_puts("Ring 3 task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    
}

void shell_cmd_ring3fault(void) {
        int pid;

        pid = sched_create_user_task(
            "ring3-fault",
            user_fault_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 fault task");
            return;
        }

        vga_puts("Ring 3 fault task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    
}

void shell_cmd_ring3ud(void) {
        int pid = sched_create_user_task(
            "ring3-ud",
            user_ud_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 UD task");
            return;
        }

        vga_puts("Ring 3 UD task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    
}

void shell_cmd_ring3gp(void) {
        int pid = sched_create_user_task(
            "ring3-gp",
            user_gp_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 GP task");
            return;
        }

        vga_puts("Ring 3 GP task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    
}

void shell_cmd_ring3as(void) {
        int first = sched_create_user_task(
            "ring3-as-a",
            user_aspace_test_entry,
            (uintptr_t)&user_stack_top
        );
        int second = sched_create_user_task(
            "ring3-as-b",
            user_aspace_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (first < 0 || second < 0) {
            klog_warn("failed to create address-space test tasks");
            return;
        }

        vga_puts("Ring 3 address-space tasks created pids=");
        vga_putdec((uint32_t)first);
        vga_puts(",");
        vga_putdec((uint32_t)second);
        vga_puts("\n");

}

#endif /* !__x86_64__ */
