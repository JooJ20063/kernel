ARCH ?= x86
BUILD_DIR := build
KERNEL_X86 := czk_x86.bin
ISO_IMAGE := cruzeiro.iso

ifeq ($(ARCH),x86)
include mk/x86.mk
else
$(error Unsupported ARCH '$(ARCH)'; CZK_x86-64 remains experimental)
endif

include mk/image.mk

.PHONY: all clean check size

all: $(KERNEL_X86)

check:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsyntax-only $(C_SRCS)

size: $(KERNEL_X86)
	ls -la $(KERNEL_X86)
	size $(KERNEL_X86)

clean:
	rm -f $(KERNEL_X86) $(ISO_IMAGE)
	rm -rf $(BUILD_DIR) $(ISO_DIR)
