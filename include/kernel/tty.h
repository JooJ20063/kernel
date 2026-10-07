#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

typedef enum {
    TTY_INPUT_FOCUS_SHELL = 0,
    TTY_INPUT_FOCUS_TTY1 = 1
} tty_input_focus_t;

void tty1_init(void);
void tty1_receive_char(char c);
void tty1_flush_input(void);

void tty1_set_input_focus(tty_input_focus_t focus);
tty_input_focus_t tty1_input_focus(void);

void tty1_set_foreground_pid(uint32_t pid);
uint32_t tty1_foreground_pid(void);
int tty1_release_foreground(uint32_t pid);

uint32_t tty1_pending(void);
uint32_t tty1_dropped(void);

fs_node_t *tty1_node(void);
