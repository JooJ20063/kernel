CC := gcc
AS := as
LD := ld

CFLAGS := -m32 -ffreestanding -Iinclude -Wall -Wextra -Werror
ASFLAGS := --32
LDFLAGS := -m elf_i386 -T linker.ld

BUILD_DIR := build

C_SRCS := \
	kernel/kernel.c \
	kernel/vga.c \
	kernel/vmm.c \
	kernel/pmm.c \
	kernel/kmalloc.c \
	kernel/sched.c \
	kernel/klog.c \
	kernel/panic.c \
	kernel/shell.c \
	arch/x86/idt.c \
	arch/x86/irq.c \
	arch/x86/pic.c

ASM_SRCS := \
	boot/boot.s \
	boot/gdt.s \
	boot/isr.s \
	boot/irq.s \
	boot/idt_descriptor.s

C_OBJS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o))
ASM_OBJS := $(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.s=.o))
OBJS := $(C_OBJS) $(ASM_OBJS)

ISO_DIR := iso
ISO_BOOT := $(ISO_DIR)/boot
ISO_GRUB := $(ISO_BOOT)/grub

.PHONY: all iso run clean check

all: kernel.bin

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

check:
	$(CC) $(CFLAGS) -c $(C_SRCS)
	rm -f *.o

iso: kernel.bin grub.cfg
	mkdir -p $(ISO_GRUB)
	cp kernel.bin $(ISO_BOOT)/kernel.bin
	cp grub.cfg $(ISO_GRUB)/grub.cfg
	grub-mkrescue -o kernel.iso $(ISO_DIR)

run: iso
	qemu-system-i386 -cdrom kernel.iso -boot d

clean:
	rm -f kernel.bin kernel.iso
	rm -rf $(ISO_DIR) $(BUILD_DIR)
