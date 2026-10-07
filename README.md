# Cruzeiro Kernel (CZK)

**Cruzeiro Kernel (CZK)** is the kernel of the **Cruzeiro OS** project, developed for learning, experimentation and systems programming research.

The project is written mainly in **C and Assembly**.

## Architectures

- **CZK_x86** — 32-bit x86 kernel and current supported development target.
- **CZK_x86-64** — experimental source tree, currently frozen while the process model, ABI and userspace architecture are stabilized on x86.

## Current Status

CZK_x86 currently includes:

- GRUB / Multiboot2 boot
- GDT, IDT, ISR and IRQ infrastructure
- PIC and PIT
- PS/2 keyboard
- VGA text console
- COM1 serial console
- PMM and VMM
- Paging, null guard and kernel write protection
- Kernel heap
- VFS and RAMFS
- Preemptive task scheduler
- Sleep, wait queues and zombie lifecycle
- Lazy FPU context management
- TSS
- Ring 3 groundwork
- `int 0x80` syscall ABI
- libK memory primitives
- Kernel diagnostic shell
- Logging and panic infrastructure

## Build

Build the primary x86 kernel:

```bash
make
```

Validate the freestanding C sources:

```bash
make check
```

Create a bootable ISO:

```bash
make iso
```

Run with VGA:

```bash
make run
```

Run through the serial console:

```bash
make run-serial
```

## Project Structure

```text
arch/       Architecture-owned CPU, boot, MM and headers
boot/       Bootloader configuration
drivers/    Hardware-facing drivers
include/    Kernel-private public headers
kernel/     Architecture-independent kernel subsystems
libk/       Freestanding kernel runtime library
mk/         Build fragments
tests/      Kernel and embedded Ring 3 tests
uapi/       Kernel/userspace ABI headers
docs/       Architecture, runtime and development documentation
```

The primary x86 tree lives under:

```text
arch/x86/
```

The experimental 64-bit tree lives under:

```text
arch/x86_64/
```

## Documentation

- `docs/ARCHITECTURE.md`
- `docs/RUNTIME.md`
- `docs/DEVELOPMENT.md`
- `docs/TREE.md`
- `docs/ROADMAP.md`

## Userspace Direction

Cruzeiro OS is not intended to be a Linux clone.

The project will define its own syscall ABI while adopting POSIX-like interfaces where useful for portability. Planned userspace projects include:

- **CLibC** — Cruzeiro C Library
- **CPKG** — Cruzeiro Package Manager

The public ABI shared with future userspace components lives under:

```text
uapi/include/
```

## License

See `LICENSE`.
