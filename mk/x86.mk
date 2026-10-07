CC ?= gcc
AS ?= as
LD ?= ld

CPPFLAGS := -Iinclude -Ilibk/include -Iuapi/include -Iarch/x86/include
CFLAGS ?= -m32 -ffreestanding -Wall -Wextra -Werror
override CFLAGS += -mno-sse -mno-sse2 -mno-mmx
ASFLAGS ?= --32
LDFLAGS ?= -m elf_i386 -T arch/x86/linker.ld
DEPFLAGS := -MMD -MP

USER_ELF_DIR := $(BUILD_DIR)/userspace
USER_PROGRAM_NAMES := hello execprobe abiprobe
USER_PROGRAM_OBJS := $(addprefix $(USER_ELF_DIR)/,$(addsuffix .o,$(USER_PROGRAM_NAMES)))
USER_PROGRAMS := $(addprefix $(USER_ELF_DIR)/,$(USER_PROGRAM_NAMES))
USER_CRT0_OBJ := $(USER_ELF_DIR)/crt0.o
INITRD_ROOT := $(BUILD_DIR)/initrd-root
INITRD_IMAGE := $(BUILD_DIR)/initrd.tar
USER_CPPFLAGS := -Iuapi/include
USER_CFLAGS := -m32 -ffreestanding -Wall -Wextra -Werror -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables

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
	kernel/fs/devfs.c \
	kernel/fs/tty.c \
	kernel/exec/elf32.c \
	kernel/sched/process.c \
	kernel/sched/task.c \
	kernel/debug/ring0_shell.c \
	kernel/syscall/syscall.c \
	kernel/uaccess/uaccess.c \
	kernel/abi/contract.c \
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
DEPS := $(C_OBJS:.o=.d)

$(KERNEL_X86): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -MT $@ -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(USER_CRT0_OBJ): tests/elf/crt0.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(USER_ELF_DIR)/%.o: tests/elf/%.c
	@mkdir -p $(dir $@)
	$(CC) $(USER_CPPFLAGS) $(USER_CFLAGS) -c $< -o $@

$(USER_ELF_DIR)/%: $(USER_ELF_DIR)/%.o $(USER_CRT0_OBJ) tests/elf/user.ld
	$(LD) -m elf_i386 -T tests/elf/user.ld -o $@ $(USER_CRT0_OBJ) $<

$(INITRD_IMAGE): $(USER_PROGRAMS)
	rm -rf $(INITRD_ROOT)
	mkdir -p $(INITRD_ROOT)/bin
	cp $(USER_PROGRAMS) $(INITRD_ROOT)/bin/
	tar --format=ustar -C $(INITRD_ROOT) -cf $@ $(addprefix bin/,$(USER_PROGRAM_NAMES))

-include $(DEPS)
