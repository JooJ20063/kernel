CC := gcc
AS := as
LD := ld


CFLAGS := -m32 -ffreestanding -Iinclude -Wall -Wextra -Werror
ASFLAGS := --32
LDFLAGS := -m elf_i386 -T linker.ld


BUILD_DIR := build


KERNEL_X86 := czk_x86.bin
KERNEL_X86_64 := czk_x86_64.bin
KERNEL_X86_64_FULL := czk_x86_64_full.bin
ISO_IMAGE := cruzeiro.iso


C_SRCS := \
	kernel/kernel.c \
	kernel/vga.c \
	kernel/vmm.c \
	kernel/pmm.c \
	kernel/kmalloc.c \
	kernel/vfs.c \
	kernel/ramfs.c \
	kernel/task.c \
	kernel/klog.c \
	kernel/panic.c \
	kernel/shell.c \
	arch/x86/idt.c \
	arch/x86/irq.c \
	arch/x86/pic.c \
	arch/x86/fpu.c \
	arch/x86/tss.c \
	kernel/syscall.c


ASM_SRCS := \
	boot/boot.s \
	boot/gdt.s \
	boot/isr.s \
	boot/irq.s \
	boot/idt_descriptor.s \
	user/user_test.s \
	user/enter_ring3.s 


C_OBJS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o))
ASM_OBJS := $(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.s=.o))
OBJS := $(C_OBJS) $(ASM_OBJS)


ISO_DIR := iso
ISO_BOOT := $(ISO_DIR)/boot
ISO_GRUB := $(ISO_BOOT)/grub


CFLAGS64 := -m64 -ffreestanding -Iinclude -Wall -Wextra -Werror
ASFLAGS64 := --64
LDFLAGS64 := -m elf_x86_64 -T linker64.ld


.PHONY: all kernel64 kernel64_full iso iso64 run run64 clean check check64


all: $(KERNEL_X86)


$(KERNEL_X86): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)


kernel64: $(KERNEL_X86_64)


$(KERNEL_X86_64):
	@mkdir -p $(BUILD_DIR)/kernel64 $(BUILD_DIR)/arch/x86_64 $(BUILD_DIR)/boot/x86_64
	$(CC) $(CFLAGS64) -c kernel/x86_64/kernel.c -o $(BUILD_DIR)/kernel64/kernel.o
	$(CC) $(CFLAGS64) -c kernel/vga.c -o $(BUILD_DIR)/kernel64/vga.o
	$(CC) $(CFLAGS64) -c kernel/pmm.c -o $(BUILD_DIR)/kernel64/pmm.o
	$(CC) $(CFLAGS64) -c kernel/x86_64/vmm.c -o $(BUILD_DIR)/kernel64/vmm.o
	$(CC) $(CFLAGS64) -c kernel/kmalloc.c -o $(BUILD_DIR)/kernel64/kmalloc.o
	$(CC) $(CFLAGS64) -c kernel/vfs.c -o $(BUILD_DIR)/kernel64/vfs.o
	$(CC) $(CFLAGS64) -c kernel/ramfs.c -o $(BUILD_DIR)/kernel64/ramfs.o
	$(CC) $(CFLAGS64) -c kernel/sched.c -o $(BUILD_DIR)/kernel64/sched.o
	$(CC) $(CFLAGS64) -c kernel/task.c -o $(BUILD_DIR)/kernel64/task.o
	$(CC) $(CFLAGS64) -c kernel/klog.c -o $(BUILD_DIR)/kernel64/klog.o
	$(CC) $(CFLAGS64) -c kernel/panic.c -o $(BUILD_DIR)/kernel64/panic.o
	$(CC) $(CFLAGS64) -c kernel/shell.c -o $(BUILD_DIR)/kernel64/shell.o
	$(CC) $(CFLAGS64) -c arch/x86_64/idt.c -o $(BUILD_DIR)/arch/x86_64/idt.o
	$(CC) $(CFLAGS64) -c arch/x86_64/irq.c -o $(BUILD_DIR)/arch/x86_64/irq.o
	$(CC) $(CFLAGS64) -c arch/x86_64/pic.c -o $(BUILD_DIR)/arch/x86_64/pic.o
	$(CC) $(CFLAGS64) -c arch/x86_64/fpu.c -o $(BUILD_DIR)/arch/x86_64/fpu.o
	$(AS) $(ASFLAGS64) boot/x86_64/gdt.s -o $(BUILD_DIR)/boot/x86_64/gdt.o
	$(AS) $(ASFLAGS64) boot/x86_64/isr.s -o $(BUILD_DIR)/boot/x86_64/isr.o
	$(AS) $(ASFLAGS64) boot/x86_64/boot.s -o $(BUILD_DIR)/boot/x86_64/boot.o
	$(LD) $(LDFLAGS64) -o $(KERNEL_X86_64) \
		$(BUILD_DIR)/kernel64/kernel.o \
		$(BUILD_DIR)/kernel64/vga.o \
		$(BUILD_DIR)/kernel64/pmm.o \
		$(BUILD_DIR)/kernel64/vmm.o \
		$(BUILD_DIR)/kernel64/kmalloc.o \
		$(BUILD_DIR)/kernel64/vfs.o \
		$(BUILD_DIR)/kernel64/ramfs.o \
		$(BUILD_DIR)/kernel64/sched.o \
		$(BUILD_DIR)/kernel64/klog.o \
		$(BUILD_DIR)/kernel64/panic.o \
		$(BUILD_DIR)/kernel64/shell.o \
		$(BUILD_DIR)/arch/x86_64/idt.o \
		$(BUILD_DIR)/arch/x86_64/irq.o \
		$(BUILD_DIR)/arch/x86_64/pic.o \
		$(BUILD_DIR)/boot/x86_64/gdt.o \
		$(BUILD_DIR)/boot/x86_64/isr.o \
		$(BUILD_DIR)/boot/x86_64/boot.o


kernel64_full: $(KERNEL_X86_64_FULL)


$(KERNEL_X86_64_FULL):
	@mkdir -p $(BUILD_DIR)/kernel/x86_64 $(BUILD_DIR)/kernel $(BUILD_DIR)/arch/x86_64 $(BUILD_DIR)/boot
	$(CC) $(CFLAGS64) -c kernel/x86_64/kernel.c -o build/kernel/x86_64/kernel.o
	$(CC) $(CFLAGS64) -c kernel/vga.c -o build/kernel/vga64.o
	$(CC) $(CFLAGS64) -c kernel/klog.c -o build/kernel/klog64.o
	$(CC) $(CFLAGS64) -c kernel/panic.c -o build/kernel/panic64.o
	$(CC) $(CFLAGS64) -c kernel/shell.c -o build/kernel/shell64.o
	$(CC) $(CFLAGS64) -c kernel/pmm.c -o build/kernel/pmm64.o
	$(CC) $(CFLAGS64) -c kernel/vmm64_stub.c -o build/kernel/vmm64.o
	$(CC) $(CFLAGS64) -c kernel/kmalloc.c -o build/kernel/kmalloc64.o
	$(CC) $(CFLAGS64) -c kernel/sched.c -o build/kernel/sched64.o
	$(CC) $(CFLAGS64) -c kernel/vfs.c -o build/kernel/vfs64.o
	$(CC) $(CFLAGS64) -c kernel/ramfs.c -o build/kernel/ramfs64.o
	$(CC) $(CFLAGS64) -c arch/x86_64/idt.c -o build/arch/x86_64/idt64_full.o
	$(CC) $(CFLAGS64) -c arch/x86_64/irq.c -o build/arch/x86_64/irq64_full.o
	$(CC) $(CFLAGS64) -c arch/x86_64/pic.c -o build/arch/x86_64/pic64_full.o
	$(AS) $(ASFLAGS64) boot/isr64.s -o build/boot/isr64.o
	$(AS) $(ASFLAGS64) boot/irq64.s -o build/boot/irq64.o
	$(AS) $(ASFLAGS64) boot/gdt64.s -o build/boot/gdt64.o
	$(AS) $(ASFLAGS64) boot/boot64.s -o build/boot/boot64.o
	$(LD) $(LDFLAGS64) -o $(KERNEL_X86_64_FULL) \
		build/kernel/x86_64/kernel.o \
		build/kernel/vga64.o \
		build/kernel/klog64.o \
		build/kernel/panic64.o \
		build/kernel/shell64.o \
		build/kernel/pmm64.o \
		build/kernel/vmm64.o \
		build/kernel/kmalloc64.o \
		build/kernel/sched64.o \
		build/kernel/vfs64.o \
		build/kernel/ramfs64.o \
		build/arch/x86_64/idt64_full.o \
		build/arch/x86_64/irq64_full.o \
		build/arch/x86_64/pic64_full.o \
		build/boot/isr64.o \
		build/boot/irq64.o \
		build/boot/gdt64.o \
		build/boot/boot64.o


$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@


$(BUILD_DIR)/boot/boot64.o: boot/boot64.s
	@mkdir -p $(dir $@)
	$(AS) --64 $< -o $@


check:
	$(CC) $(CFLAGS) -c $(C_SRCS)
	rm -f *.o


check64:
	$(CC) $(CFLAGS64) -c \
		kernel/x86_64/kernel.c \
		arch/x86_64/idt.c \
		arch/x86_64/irq.c \
		arch/x86_64/pic.c \
		kernel/shell.c \
		kernel/panic.c \
		kernel/vga.c \
		kernel/klog.c \
		kernel/sched.c \
		kernel/pmm.c \
		kernel/vmm64_stub.c \
		kernel/kmalloc.c \
		kernel/vfs.c \
		kernel/ramfs.c
	rm -f *.o


iso: $(KERNEL_X86) grub.cfg
	mkdir -p $(ISO_GRUB)
	cp $(KERNEL_X86) $(ISO_BOOT)/$(KERNEL_X86)
	cp grub.cfg $(ISO_GRUB)/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)


iso64: $(KERNEL_X86_64) grub.cfg
	mkdir -p $(ISO_GRUB)
	cp $(KERNEL_X86_64) $(ISO_BOOT)/$(KERNEL_X86_64)
	cp grub.cfg $(ISO_GRUB)/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)


run: iso
	qemu-system-i386 -cdrom $(ISO_IMAGE) -boot d


run64: iso64
	qemu-system-x86_64 -cdrom $(ISO_IMAGE) -boot d -vga std

size: czk_x86.bin
	ls -la czk_x86.bin
	size czk_x86.bin
clean:
	rm -f \
		$(KERNEL_X86) \
		$(KERNEL_X86_64) \
		$(KERNEL_X86_64_FULL) \
		$(ISO_IMAGE) \
		kernel.bin \
		kernel64.bin \
		kernel64_full.bin \
		kernel.iso
	rm -rf $(ISO_DIR) $(BUILD_DIR)