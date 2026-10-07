#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

void tty1_init(void);
void tty1_receive_char(char c);
void tty1_flush_input(void);

uint32_t tty1_pending(void);
uint32_t tty1_dropped(void);

fs_node_t *tty1_node(void);
