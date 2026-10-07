CC ?= gcc
AS ?= as
LD ?= ld

CPPFLAGS := -Iinclude -Ilibk/include -Iuapi/include -Iarch/x86/include
CFLAGS ?= -m32 -ffreestanding -Wall -Wextra -Werror
override CFLAGS += -mno-sse -mno-sse2 -mno-mmx
ASFLAGS ?= --32
LDFLAGS ?= -m elf_i386 -T arch/x86/linker.ld

C_SRCS := \
	libk/memory.c \
	drivers/console/serial_16550.c \
	drivers/console/vga_text.c \
	kernel/core/main.c \
	kernel/core/klog.c \
	kernel/core/panic.c \
	kernel/mm/vmm.c \
	kernel/mm/pmm.c \
	kernel/mm/kmalloc.c \
	kernel/fs/vfs.c \
	kernel/fs/fd.c \
	kernel/fs/ramfs.c \
	kernel/sched/process.c \
	kernel/sched/task.c \
	kernel/debug/shell.c \
	kernel/syscall/syscall.c \
	kernel/uaccess/uaccess.c \
	arch/x86/cpu/idt.c \
	arch/x86/cpu/irq.c \
	arch/x86/cpu/pic.c \
	arch/x86/cpu/fpu.c \
	arch/x86/cpu/tss.c

ASM_SRCS := \
	arch/x86/boot/boot.s \
	arch/x86/boot/gdt.s \
	arch/x86/boot/isr.s \
	arch/x86/boot/irq.s \
	arch/x86/boot/idt_descriptor.s \
	tests/ring3/user_test.s \
	tests/ring3/enter_ring3.s

C_OBJS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o))
ASM_OBJS := $(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.s=.o))
OBJS := $(C_OBJS) $(ASM_OBJS)

$(KERNEL_X86): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
