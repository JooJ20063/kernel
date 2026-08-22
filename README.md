# Cruzeiro Kernel (CZK)

**Cruzeiro Kernel (CZK)** is the kernel of the **Cruzeiro OS** project, an operating system developed primarily for learning, experimentation and systems programming research.

The project is written mainly in **C and Assembly** and currently targets the x86 family.

## Architectures

- **CZK_x86** — 32-bit x86 kernel, currently the primary development target.
- **CZK_x86-64** — experimental 64-bit x86 kernel path.

## Current Status

CZK_x86 currently includes:

- Multiboot2 boot support through GRUB
- Bare-metal boot on x86 hardware through Legacy/CSM
- Global Descriptor Table (GDT)
- Interrupt Descriptor Table (IDT)
- ISR and IRQ infrastructure
- PIC and PIT support
- PS/2 keyboard input
- VGA text-mode output
- Physical Memory Manager (PMM)
- Virtual Memory Manager (VMM)
- Paging and kernel write protection
- Kernel heap (`kmalloc`)
- Virtual File System (VFS)
- RAMFS
- Preemptive task scheduler
- Task lifecycle management
- Sleep and wait queues
- Lazy FPU context management
- Task State Segment (TSS)
- `int 0x80` syscall interface
- Kernel diagnostic shell
- Kernel logging and panic infrastructure

Ring 3 userspace support is currently under development.

## Project Philosophy

Cruzeiro OS is not intended to be a Linux clone.

The long-term goal is to provide a system with its own kernel architecture and syscall ABI while adopting POSIX-like interfaces where they are useful for portability and software development.

Future userspace components are expected to include:

- **CLibC** — Cruzeiro C Library
- **CPKG** — Cruzeiro Package Manager

## Boot

The x86 kernel uses **Multiboot2** and is loaded by GRUB.

The current 32-bit path has also been validated on physical x86 hardware.

## Build

Build the primary x86 kernel with:

```bash
make
```

Create a bootable ISO with:

```bash
make iso
```

Run it under QEMU with:

```bash
make run
```

## Documentation

Technical documentation is available under:

```text
docs/
```

The architecture overview can be found in:

```text
docs/ARCHITECTURE.md
```

## Project Structure

```text
arch/       Architecture-specific C code
boot/       Low-level boot and Assembly code
docs/       Technical documentation
include/    Kernel headers
kernel/     Core kernel subsystems
```

## License

See `LICENSE`.