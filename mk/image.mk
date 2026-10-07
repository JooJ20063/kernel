GRUB_CFG := boot/grub/grub.cfg
ISO_DIR := iso
ISO_BOOT := $(ISO_DIR)/boot
ISO_GRUB := $(ISO_BOOT)/grub

.PHONY: iso run run-serial

iso: $(KERNEL_X86) $(INITRD_IMAGE) $(GRUB_CFG)
	mkdir -p $(ISO_GRUB)
	cp $(KERNEL_X86) $(ISO_BOOT)/$(KERNEL_X86)
	cp $(INITRD_IMAGE) $(ISO_BOOT)/initrd.tar
	cp $(GRUB_CFG) $(ISO_GRUB)/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)

run: iso
	qemu-system-i386 -cdrom $(ISO_IMAGE) -boot d

run-serial: iso
	qemu-system-i386 -cdrom $(ISO_IMAGE) -boot d -m 128M -nographic
