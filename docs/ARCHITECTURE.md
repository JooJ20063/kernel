# Cruzeiro Kernel Architecture

This document describes the current architecture of the **Cruzeiro Kernel (CZK)**, its main implemented subsystems, and how they interact.

Primary development currently takes place on **CZK_x86**, the 32-bit implementation for the x86 architecture.

The **CZK_x86-64** path exists in an experimental state and does not yet have the same level of maturity as CZK_x86.

---

## 1. Overview

Cruzeiro Kernel is the kernel of the **Cruzeiro OS** project.

The project is developed mainly in **C and Assembly**, using its own kernel architecture and its own system call ABI.

The long-term goal is not to reproduce the internal architecture of Linux, but to build an independent system while adopting POSIX-like interfaces where they are useful for portability and software development.

CZK_x86 currently includes support for:

- boot through GRUB and Multiboot2;
- execution on physical x86 hardware through Legacy/CSM;
- GDT;
- IDT;
- ISR and IRQ infrastructure;
- PIC;
- PIT;
- PS/2 keyboard;
- VGA text mode;
- PMM;
- VMM;
- paging;
- kernel page protection;
- dynamic kernel heap;
- VFS;
- RAMFS;
- preemptive scheduler;
- task lifecycle management;
- sleep and wait queues;
- lazy FPU context management;
- TSS;
- system call interface through `int 0x80`;
- Ring 0 diagnostic shell.

**Ring 3** support is currently under development.

---

## 2. Boot Sequence

CZK_x86 uses **GRUB** as its bootloader and follows the **Multiboot2** protocol.

The basic initialization flow is:

```text
Firmware
   ↓
GRUB
   ↓
Multiboot2
   ↓
boot/boot.s
   ↓
GDT
   ↓
initial stack
   ↓
kernel_main()
   ↓
subsystem initialization
```

### 2.1 Initial Entry Point

The file:

```text
boot/boot.s
```

contains `_start`, which represents the initial execution point of the kernel.

The main sequence is:

1. interrupts are initially disabled using `cli`;
2. the kernel GDT is loaded with `lgdt`;
3. the data segment selectors are reloaded;
4. `CS` is reloaded through a far jump;
5. an initial kernel stack is configured;
6. the Multiboot2 information pointer received in `EBX` is passed to `kernel_main`;
7. main execution continues in C.

If `kernel_main()` unexpectedly returns, the bootstrap enters a `hlt` loop.

---

## 3. Multiboot2

CZK_x86 uses a Multiboot2 header so that GRUB can recognize the kernel.

The header contains the magic number:

```text
0xE85250D6
```

along with the remaining fields required by the Multiboot2 specification.

After loading the kernel, GRUB provides the bootstrap with information about the boot environment, including the address of the Multiboot2 information structure.

This structure is later used by the kernel to obtain information such as the physical memory map.

---

## 4. Global Descriptor Table — GDT

The GDT defines the segments and privilege levels used by the x86 architecture.

CZK_x86 currently uses selectors for:

```text
0x08 — Kernel Code
0x10 — Kernel Data
0x18 — User Code
0x20 — User Data
0x28 — TSS
```

The effective selectors for Ring 3 execution use RPL 3:

```text
User CS = 0x1B
User DS = 0x23
```

The GDT is loaded during bootstrap before entering `kernel_main`.

The user code and user data segments already prepare the system for future CPL3 execution.

---

## 5. Task State Segment — TSS

CZK_x86 includes a **Task State Segment** implementation to support privilege transitions.

The TSS currently uses:

```text
Selector: 0x28
Kernel SS: 0x10
```

The task register (`TR`) is loaded using:

```asm
ltr
```

and its initialization has already been validated during kernel testing.

The `esp0` field will be used to indicate which kernel stack the CPU must use when performing a transition:

```text
Ring 3
   ↓
Ring 0
```

This mechanism will be essential for:

- interrupts originating from userspace;
- exceptions originating from userspace;
- system calls;
- preemption of Ring 3 processes.

---

## 6. Interrupt Descriptor Table — IDT

The IDT defines the gates used by the CPU for:

- exceptions;
- hardware interrupts;
- software interrupts.

CZK installs:

```text
0–31     CPU Exceptions
32–47    Hardware IRQs
128      System Call Gate
129      Scheduler Yield Gate
```

Most gates use Ring 0 privilege.

The `0x80` entry uses DPL 3 so that Ring 3 code can invoke system calls.

The internal scheduler yield vector remains restricted to the kernel.

---

## 7. ISR and IRQ

Low-level stubs are implemented in Assembly.

Main files:

```text
boot/isr.s
boot/irq.s
```

The stubs perform:

1. CPU context preservation;
2. interrupt frame normalization;
3. call into the C handler;
4. state restoration;
5. return through `iret`.

The saved context layout is represented by `registers_t`.

This structure contains:

```text
GS
FS
ES
DS

EDI
ESI
EBP
ESP
EBX
EDX
ECX
EAX

Interrupt Number
Error Code

EIP
CS
EFLAGS
User ESP
User SS
```

The frame format allows the scheduler to use the interrupt context itself to perform context switches.

---

## 8. PIC — Programmable Interrupt Controller

CZK currently uses the legacy **8259 PIC**.

During boot, the PICs are remapped to:

```text
Master PIC → 0x20
Slave PIC  → 0x28
```

to avoid conflicts with CPU exception vectors.

All IRQs are initially masked, and only the required devices are enabled.

The main IRQs currently in use are:

```text
IRQ0 — PIT
IRQ1 — PS/2 keyboard
```

The kernel also sends the appropriate EOI commands after interrupt processing.

---

## 9. PIT — Programmable Interval Timer

The **8253/8254 PIT** currently provides the main timer source for CZK_x86.

The timer operates at approximately:

```text
100 Hz
```

Each IRQ0:

- increments the kernel tick counter;
- updates timing state;
- participates in the sleep mechanism;
- may trigger preemption;
- allows the scheduler to select another task.

The PIT is therefore the temporal foundation of the current preemptive scheduler.

---

## 10. PS/2 Keyboard

The keyboard uses IRQ1.

The input flow is:

```text
Keyboard
   ↓
PS/2 Controller
   ↓
IRQ1
   ↓
kernel handler
   ↓
scancode read
   ↓
translation
   ↓
shell
```

The current implementation includes basic ABNT2 keyboard translation, including modifiers such as:

- Shift;
- Caps Lock.

The shell currently runs directly in Ring 0.

---

## 11. VGA Text Mode

The current console uses VGA text mode.

The video memory address is:

```text
0xB8000
```

The driver implements:

- character output;
- colors;
- cursor control;
- screen clearing;
- scrolling.

This path currently works both in QEMU and on physical hardware through Legacy/CSM boot.

In the future, the system is expected to gain framebuffer support for modern environments without relying on VGA text mode.

---

## 12. Physical Memory Manager — PMM

The PMM manages physical frames of:

```text
4 KiB
```

Allocation state is tracked through a bitmap.

During initialization:

1. the kernel receives the Multiboot2 information pointer;
2. it searches for the memory map tag;
3. it walks the reported memory regions;
4. regions marked as available are released for allocation;
5. sensitive regions are reserved again.

Reserved regions include:

```text
0 – 1 MiB
kernel image
Multiboot2 structure
```

The PMM currently manages physical memory up to:

```text
512 MiB
```

A future expansion is expected to allow a much larger portion of the 32-bit physical address space to be managed.

---

## 13. Virtual Memory Manager — VMM

The VMM manages:

- page directory;
- page tables;
- mapping;
- unmapping;
- protection flags.

CZK_x86 currently uses:

```text
4 KiB pages
```

Available flags include:

```text
Present
Read/Write
User
```

The kernel initially uses identity mapping for the regions required during bootstrap.

Virtual page zero remains unmapped as protection against null pointer dereferences.

---

## 14. Memory Protection

After paging initialization, the kernel protects code and read-only data regions.

The sections:

```text
.text
.rodata
```

are mapped without write permission.

The:

```text
CR0.WP
```

bit also remains enabled.

This means that even code executing in Ring 0 must respect write permissions on protected pages.

Invalid writes can therefore be detected through page faults.

---

## 15. Page Fault Handler

The exception:

```text
#PF — Vector 14
```

has dedicated handling.

The kernel reads:

```text
CR2
```

to identify the address that triggered the fault.

The error code is used to classify situations such as:

- non-present page;
- write to read-only page;
- user access;
- reserved bit violation;
- instruction fetch fault;
- null pointer access;
- possible stack overflow.

CPU state is displayed before the kernel enters a halt state.

---

## 16. Kernel Heap

The kernel heap provides dynamic allocation through:

```c
kmalloc()
```

The subsystem includes testing and integrity checking mechanisms.

The shell provides commands to:

- test allocations;
- check for corruption;
- inspect heap state.

The heap is used by several CZK subsystems, including tasks and internal kernel structures.

---

## 17. Tasks

CZK provides its own task abstraction.

Each task stores information such as:

- PID;
- name;
- state;
- stack;
- execution context;
- wake tick;
- FPU state;
- wait queue information.

Current task states include:

```text
RUNNING
READY
BLOCKED
ZOMBIE
```

Tasks may block for different reasons, including:

```text
sleep
event
```

---

## 18. Scheduler

CZK_x86 currently uses a preemptive scheduler.

Task selection is based on round-robin scheduling.

The scheduler directly interacts with the interrupt frame received through the timer.

During a context switch, the kernel may return from the IRQ using the execution context of another task.

The system currently supports:

- preemption;
- yield;
- sleep;
- wakeup;
- blocking;
- task exit;
- zombie tasks;
- reaper.

A dedicated software interrupt is used for yield:

```text
int 0x81
```

This vector remains internal to the kernel.

---

## 19. Sleep

Tasks may request temporary suspension based on kernel ticks.

A sleeping task follows the transition:

```text
RUNNING
   ↓
BLOCKED
   ↓
timer advances
   ↓
wake_tick reached
   ↓
READY
```

This avoids busy waiting and allows other tasks to use the CPU while the sleeping task waits.

---

## 20. Wait Queues

CZK includes wait queues for event-based synchronization.

The main operations include:

```text
task_wait()
task_wake()
wake_one()
wake_all()
```

The implementation uses a dedicated queue of waiters.

The mechanism was designed to avoid lost wakeups by correctly managing interrupt state.

Wait queues are expected to become useful for:

- blocking I/O;
- pipes;
- sockets;
- processes;
- drivers;
- IPC.

---

## 21. Task Lifecycle

Tasks have their own lifecycle.

When a task terminates:

```text
RUNNING
   ↓
ZOMBIE
   ↓
reaper
   ↓
resources released
```

The reaper is responsible for releasing resources such as:

- kernel stack;
- FPU state;
- task structure.

This model will later support the evolution toward real processes and mechanisms similar to `wait()`.

---

## 22. FPU and Lazy Context Switching

CZK includes FPU/SSE support using a lazy context switching mechanism.

Instead of saving and restoring the FPU state on every context switch, the kernel uses:

```text
CR0.TS
```

When a task attempts to use the FPU without owning the currently active FPU context, the CPU raises:

```text
#NM — Device Not Available
```

The `#NM` handler identifies the current FPU state owner and performs the required context transition.

This reduces unnecessary FPU context operations.

---

## 23. System Calls

CZK currently provides an initial system call interface through:

```text
int 0x80
```

The gate uses DPL 3 to allow future calls originating from Ring 3.

The initial ABI uses registers as follows:

```text
EAX — syscall number
EBX — arg1
ECX — arg2
EDX — arg3
```

The return value also uses:

```text
EAX
```

The complete path has already been validated through a test call:

```text
int 0x80
   ↓
IDT
   ↓
ISR
   ↓
syscall dispatcher
   ↓
return
```

The interface will expand as real processes and userspace support are implemented.

---

## 24. VFS

The Virtual File System provides a common abstraction for file operations.

The current interface includes conceptually POSIX-like operations such as:

```text
open
close
read
write
readdir
```

The goal is to allow different filesystems to use the same access layer.

This API belongs to Cruzeiro OS and does not attempt to reproduce the internal Linux ABI.

---

## 25. RAMFS

RAMFS is currently the filesystem available during kernel execution.

It supports:

- file creation;
- reading;
- writing;
- directory listing.

Shell commands use the VFS layer to access RAMFS.

RAMFS is expected to remain useful even after persistent filesystems are introduced.

---

## 26. Kernel Shell

CZK currently includes an internal shell used mainly for:

- debugging;
- testing;
- inspection;
- subsystem validation.

The shell currently executes in Ring 0.

Available functionality includes tests related to:

- timer;
- scheduler;
- tasks;
- PMM;
- VMM;
- heap;
- filesystem;
- panic;
- system calls;
- TSS.

The normal user interface is expected to move to Ring 3 in the future.

The kernel shell may remain available as a privileged diagnostic tool.

---

## 27. Logging

The internal logging system uses:

```text
klog
```

to record important kernel events and state transitions.

Logs are particularly useful during:

- boot;
- subsystem initialization;
- diagnostics;
- testing.

---

## 28. Kernel Panic

Fatal failures use a centralized panic routine.

The panic system can display:

- reason;
- registers;
- interrupt vector;
- error code.

After the dump, the kernel disables interrupts and enters a halt state.

The shell also includes mechanisms to deliberately trigger controlled exceptions during testing.

---

## 29. Bare-Metal Execution

CZK_x86 has already been validated on physical x86 hardware.

The boot path used:

```text
GRUB
Multiboot2
Legacy/CSM
```

During bare-metal testing, the following components were successfully validated:

- boot;
- VGA;
- keyboard;
- PIT;
- PIC;
- scheduler;
- context switching;
- wait queues;
- sleep;
- heap;
- TSS;
- `int 0x80`.

Native UEFI boot is not yet supported.

---

## 30. Ring 3

Ring 3 support is currently under development.

The project already includes:

- user code segments;
- user data segments;
- TSS;
- DPL 3 system call gate;
- USER flag in the VMM;
- sections reserved for userspace code and data.

The next steps include:

```text
USER page mappings
   ↓
user stack
   ↓
iret to CPL3
   ↓
first Ring 3 code
   ↓
userspace syscall
   ↓
preemption
   ↓
correct return to Ring 3
```

---

## 31. Processes — Near-Term Roadmap

After Ring 3 becomes stable, the architecture is expected to evolve from internal tasks toward complete processes.

Planned goals include:

```text
process address spaces
FD tables
process lifecycle
fork-like process creation
exec
wait
```

Each process is expected to have its own virtual memory context and associated resources.

---

## 32. ELF Loader

An ELF loader is planned to allow external program loading.

The future execution flow is expected to be approximately:

```text
filesystem
   ↓
ELF file
   ↓
ELF parser
   ↓
segment mapping
   ↓
process stack
   ↓
entry point
   ↓
Ring 3
```

This will allow applications to be compiled specifically for the Cruzeiro OS ABI.

---

## 33. Future Userspace

Cruzeiro OS is expected to provide its own userspace components.

Planned projects include:

```text
CLibC
Cruzeiro C Library

CPKG
Cruzeiro Package Manager
```

The goal is to provide POSIX-like APIs where useful while remaining independent from the Linux ABI and Linux internal architecture.

---

## 34. Persistent Storage

After processes and userspace are established, support for physical storage will be required.

The future roadmap includes:

```text
storage driver
   ↓
block layer
   ↓
persistent filesystem
   ↓
mounting
   ↓
program loading
```

ATA/AHCI drivers are possible candidates for the first implementations.

---

## 35. Future Hardware Support

Other planned subsystems include:

- PCI enumeration;
- ACPI;
- APIC;
- USB;
- framebuffer;
- storage devices;
- network drivers;
- SMP.

These components will be implemented progressively as the core infrastructure matures.

---

## 36. Networking — Long-Term Goal

A custom networking stack is a long-term objective.

A possible progression is:

```text
NIC driver
   ↓
Ethernet
   ↓
ARP
   ↓
IPv4
   ↓
ICMP
   ↓
UDP
   ↓
TCP
   ↓
sockets
```

---

## 37. Architectures

### CZK_x86

Current primary implementation.

Characteristics:

```text
32-bit
protected mode
paging
Multiboot2
GRUB
Legacy/CSM bare metal
```

### CZK_x86-64

Experimental implementation.

The 64-bit path does not yet have feature parity with CZK_x86 and will be developed progressively.

---

## 38. Architectural Philosophy

Cruzeiro Kernel is not intended to be a reimplementation of Linux.

The current philosophy can be summarized as:

```text
POSIX-like source interfaces
+
independent ABI
+
independent kernel architecture
+
independent userspace
```

Existing interfaces will be adopted when they are technically useful and well established.

Differences may also be introduced when there is a clear architectural reason to do so.

The priority is to build a coherent, understandable system that progressively becomes capable of running real software.

---

## 39. Current State

The main CZK_x86 foundation currently includes:

```text
boot
interrupts
memory management
dynamic memory
task scheduling
blocking
filesystem abstraction
system calls
privilege infrastructure
bare-metal execution
```

The next major architectural milestone is stable **Ring 3** execution.

That milestone will open the path toward real processes, ELF loading, userspace, CLibC, and eventually a complete operating system built around Cruzeiro Kernel.