#include "shell_internal.h"
#include <kernel/vga.h>
#include <kernel/sched.h>
#include <kernel/task.h>

/* Comandos de diagnostico do scheduler e do ciclo de vida de tarefas. */

void shell_cmd_task(void) {
        vga_puts("current tid=");
        vga_putdec(sched_current_task());

        vga_puts(" pid=");
        vga_putdec(sched_current_pid());

        vga_puts(" tasks=");
        vga_putdec(sched_task_count());

        vga_puts(" switches=");
        vga_putdec(sched_switch_count());

        vga_puts(" demoA=\n");
        vga_putdec(sched_demo_counter_a());

        vga_puts(" demoB=");
        vga_putdec(sched_demo_counter_b());

        vga_puts(" sseA=");
        vga_putdec((uint32_t)sched_sse_value_a());

        vga_puts(" sseB=");
        vga_putdec((uint32_t)sched_sse_value_b());

        vga_puts(" sleep=");
        vga_putdec(sched_sleep_demo_counter());

        vga_puts(" evA=");
        vga_putdec(sched_event_a_counter());

        vga_puts(" evB=");
        vga_putdec(sched_event_b_counter());

        vga_puts(" wake=");
        vga_putdec(sched_event_waker_counter());

        vga_puts("\n");
    
}

void shell_cmd_ps(void) {
        task_list_tasks();
    
}

void shell_cmd_schedtest(void) {
        sched_demo_init();
        vga_puts("scheduler tests started\n");
        return;
    
}

void shell_cmd_lastexit(void) {
        vga_puts("last exit: pid=");
        vga_putdec(sched_last_exit_pid());

        vga_puts(" code=");
        vga_putdec((uint32_t)sched_last_exit_code());

        vga_puts("\n");    
    
}

void shell_cmd_waittest(void) {
        int32_t status = 0;
        int32_t pid = task_wait_child(&status);

        if (pid < 0) {
            vga_puts("wait: no zombie child\n");
        } else {
            vga_puts("wait: pid=");
            vga_putdec((uint32_t)pid);

            vga_puts(" code=");
            vga_putdec((uint32_t)status);

            vga_puts("\n");
        }
    
}
