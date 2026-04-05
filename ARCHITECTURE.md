# Architecture Documentation

## x86_64 Support

### 64-bit Boot Sequence
The 64-bit boot sequence involves...

### IDT64 Initialization
The initialization of the Interrupt Descriptor Table (IDT64) is crucial...

### Syscalls Infrastructure
The infrastructure for syscalls has been improved to support...

### Timer Hardening against Divide-by-Zero
Timely hardening measures are taken against divide-by-zero...

### New Shell Commands
New shell commands have been introduced:
- **arch**: Display architecture information.
- **shutdown**: Safely shutdown the system.

### Dual-Architecture Support
Support for dual architecture is enabled via:
- **GRUB**: Bootloader configuration for compatibility.
- **Makefile Targets**: Targets for building both architectures are defined in the Makefile.