# Cruzeiro Kernel Diagnostic Shell

The **Cruzeiro Kernel (CZK)** includes a minimal diagnostic shell used for runtime inspection, subsystem testing, and kernel development.

The shell currently runs entirely in **Ring 0** and should not be confused with the future Cruzeiro OS userspace shell.

Its primary purpose is kernel diagnostics.

---

## 1. Overview

The CZK diagnostic shell provides direct access to several kernel subsystems, including:

- timer;
- scheduler;
- tasks;
- physical memory;
- virtual memory;
- kernel heap;
- VFS;
- RAMFS;
- system calls;
- TSS;
- exception testing;
- kernel logging and diagnostics.

The current input path is:

```text
PS/2 Keyboard
   ↓
IRQ1
   ↓
keyboard handler
   ↓
scancode translation
   ↓
shell input buffer
   ↓
command parser
```

Commands are executed directly inside the kernel.

---

## 2. Purpose

The diagnostic shell exists primarily to:

- inspect kernel state at runtime;
- test newly implemented subsystems;
- validate memory management;
- inspect scheduler behavior;
- deliberately trigger controlled failures;
- test filesystem operations;
- validate system call infrastructure;
- assist bare-metal debugging.

It is therefore considered a **kernel development interface**, not a normal userspace command shell.

---

## 3. Basic Commands

### `help`

Displays the list of available commands.

```text
help
```

---

### `clear`

Clears the VGA text-mode screen and resets the cursor.

```text
clear
```

---

### `echo`

Writes text directly to the console.

Example:

```text
echo Hello from CZK
```

File redirection is also supported in the current RAMFS shell interface:

```text
echo hello > file.txt
```

---

## 4. Timer Diagnostics

### `ticks`

Displays information about the kernel timer.

Typical information includes:

- total PIT ticks;
- elapsed time;
- timer frequency.

Example:

```text
ticks
```

The current CZK_x86 timer normally operates at approximately:

```text
100 Hz
```

The PIT timer is also used by the preemptive scheduler and task sleep system.

---

## 5. Task and Scheduler Diagnostics

### `task`

Displays information about the currently executing task and scheduler state.

Depending on the current implementation, this may include:

- current task;
- task identifier;
- scheduler switches;
- timing information.

```text
task
```

---

### `ps`

Lists tasks known to the kernel.

The output can include information such as:

```text
PID
NAME
STATE
BLOCK REASON
```

Example:

```text
ps
```

Possible task states include:

```text
RUNNING
READY
BLOCKED
ZOMBIE
```

Blocking reasons currently include mechanisms such as:

```text
sleep
event
```

---

### `schedtest`

Starts the internal scheduler test workload.

```text
schedtest
```

The test exercises mechanisms such as:

- preemption;
- context switching;
- sleep;
- wakeup;
- event blocking;
- wait queues;
- `wake_one`;
- `wake_all`;
- task termination;
- zombie cleanup;
- reaper behavior.

The test workload is not started automatically during a normal kernel boot.

This allows the standard boot path to remain relatively quiet while scheduler stress tests can be explicitly enabled when required.

---

## 6. Physical Memory Diagnostics

### `pmm`

Displays the current state of the Physical Memory Manager.

Typical information includes:

```text
total frames
free frames
```

CZK currently uses physical frames of:

```text
4 KiB
```

The PMM obtains available memory regions from the Multiboot2 memory map.

The current implementation manages physical memory up to a configured maximum of approximately:

```text
512 MiB
```

---

## 7. Virtual Memory Diagnostics

### `vmm`

Displays information about the Virtual Memory Manager and paging state.

This may include:

- whether paging is enabled;
- page protection state;
- CR0.WP state.

```text
vmm
```

---

### `wp`

Displays the current state of:

```text
CR0.WP
```

Example:

```text
wp
```

When enabled, Ring 0 code must also respect read-only page mappings.

This is used by CZK to protect sections such as:

```text
.text
.rodata
```

---

### `nullguard`

Displays information related to the null-page protection mechanism.

Virtual page zero is intentionally left unmapped.

This allows invalid null pointer accesses to generate a page fault instead of silently corrupting memory.

```text
nullguard
```

---

### `unmap`

The kernel may expose an `unmap` diagnostic command for controlled virtual-memory testing.

This command is intended for kernel development and should be treated as potentially destructive.

Its exact syntax may evolve together with the VMM implementation.

---

## 8. Kernel Heap Diagnostics

### `kheap`

Displays the current state of the kernel heap.

```text
kheap
```

The output may contain statistics related to:

- allocated memory;
- mapped heap memory;
- heap usage.

---

### `kmalloc`

Allocates memory from the kernel heap for testing.

Example:

```text
kmalloc 128
```

The command can perform a write test on the allocated region and display the resulting address.

This provides a simple runtime validation path for:

```text
PMM
 ↓
VMM
 ↓
kernel heap
 ↓
kmalloc
```

---

### `kheapcheck`

Runs an integrity check over the kernel heap.

```text
kheapcheck
```

Expected successful output:

```text
kheapcheck=OK
```

This command is particularly useful after scheduler tests, task destruction, and repeated dynamic allocations.

---

## 9. VFS and RAMFS Commands

The diagnostic shell provides basic file operations through the VFS layer.

The current filesystem is RAMFS.

---

### `ls`

Lists entries available through the mounted root filesystem.

```text
ls
```

---

### `cat`

Reads a file through the VFS.

Example:

```text
cat file.txt
```

The normal path is approximately:

```text
shell
   ↓
VFS
   ↓
RAMFS
```

---

### `touch`

Creates an empty file in RAMFS.

Example:

```text
touch test.txt
```

---

### Writing Files

A file can be created or overwritten using:

```text
echo hello > test.txt
```

A simplified `cat` write path may also be available:

```text
cat > test.txt hello
```

These commands are primarily used to validate VFS and RAMFS write operations.

---

## 10. TSS Diagnostics

### `tss`

Displays information related to the currently loaded Task State Segment.

```text
tss
```

During CZK_x86 development, the task register has been validated with the selector:

```text
TR = 0x28
```

The TSS will become particularly important when Ring 3 execution is introduced because it provides the Ring 0 stack used during privilege transitions.

---

## 11. System Call Diagnostics

### `syscalltest`

Tests the CZK system call path using:

```text
int 0x80
```

The test currently validates the complete path:

```text
software interrupt
      ↓
IDT vector 128
      ↓
ISR infrastructure
      ↓
system call dispatcher
      ↓
SYS_WRITE
      ↓
return through EAX
```

A successful test has produced output similar to:

```text
hello from int 0x80
return=20
```

This test currently originates from Ring 0.

Its purpose is to validate the syscall infrastructure before the first actual Ring 3 process begins using it.

---

## 12. Panic and Exception Testing

The shell contains several commands intended to deliberately trigger kernel exception paths.

These commands exist exclusively for testing.

---

### `panic`

Triggers a kernel panic directly.

```text
panic
```

---

### `panic int3`

Executes a breakpoint exception:

```text
#BP
```

Example:

```text
panic int3
```

This validates the IDT and exception handling path.

---

### `panic ud2`

Executes the undefined instruction:

```asm
ud2
```

causing:

```text
#UD — Invalid Opcode
```

Example:

```text
panic ud2
```

---

### `panic null`

Attempts an invalid access through the null page.

The expected result is:

```text
#PF — Page Fault
```

This validates the null-page guard.

---

### `panic int <n>`

Allows controlled testing of software interrupt vectors.

Example:

```text
panic int 3
```

This command should only be used during kernel testing.

---

### `pfault`

The shell may expose dedicated page-fault testing functionality through:

```text
pfault
```

This is intended to exercise the page fault handler under controlled conditions.

Its available modes may evolve as VMM protection testing expands.

---

## 13. Page Fault Diagnostics

When a page fault occurs, CZK can display information such as:

```text
CR2
error code
fault classification
EIP
ESP
EBP
general-purpose registers
CS
EFLAGS
```

The handler can classify faults including:

```text
NULL POINTER / INVALID ACCESS
WRITE TO READ-ONLY PAGE
NON-PRESENT PAGE
STACK GUARD ACCESS
RESERVED PAGE-TABLE BIT
INSTRUCTION FETCH FAULT
```

After a fatal page fault, execution normally halts.

---

## 14. Shutdown

### `shutdown`

Attempts to shut down the system.

```text
shutdown
```

The current implementation uses a QEMU-specific poweroff mechanism.

Under QEMU, this can terminate the virtual machine.

On physical hardware, this mechanism is not a real ACPI poweroff implementation.

When the QEMU-specific shutdown request is not handled, CZK disables interrupts and enters a permanent halt loop.

Therefore, on bare metal the current behavior is effectively:

```text
shutdown request
      ↓
QEMU-specific port write ignored
      ↓
CLI
      ↓
HLT loop
```

Real hardware poweroff through ACPI is planned for a future implementation.

---

## 15. Input Buffer

The shell uses a fixed-size input buffer.

Input exceeding the supported command length is not accepted beyond the configured limit.

This prevents writes beyond the shell command buffer.

The parser remains deliberately simple.

Current limitations include:

- no command history;
- no advanced line editing;
- limited quoting;
- limited escaping;
- no job control;
- no userspace processes.

These limitations are acceptable because the shell currently exists primarily as a kernel diagnostic interface.

---

## 16. Ring 0 Execution

The diagnostic shell currently executes with full kernel privileges.

This means shell commands can directly interact with:

```text
PMM
VMM
scheduler
PIC/PIT
VFS
RAMFS
kernel heap
exception infrastructure
```

This architecture is useful during kernel development but is not appropriate for normal application execution.

Cruzeiro OS will eventually provide a separate userspace shell running in Ring 3.

---

## 17. Future Role

The current diagnostic shell is not expected to become the primary Cruzeiro OS userspace shell.

Instead, the long-term model is expected to look approximately like:

```text
Cruzeiro Kernel
      │
      ├── privileged diagnostic shell
      │
      └── syscall interface
               ↓
           Ring 3
               ↓
         userspace shell
```

The kernel shell may remain available as a low-level debugging environment even after a complete userspace exists.

---

## 18. Current Diagnostic Coverage

Through the shell, CZK can currently exercise or inspect a substantial portion of its kernel architecture:

```text
PIT / timing
PIC / IRQ path
scheduler
tasks
sleep
wait queues
PMM
VMM
paging protection
kernel heap
VFS
RAMFS
TSS
system calls
exceptions
kernel panic
```

This makes the shell one of the primary integration-testing interfaces during CZK development.

---

## 19. Development Status

The shell is expected to continue evolving as CZK gains new subsystems.

Future diagnostic commands may cover:

```text
Ring 3 processes
process address spaces
ELF loading
file descriptors
persistent storage
PCI
ACPI
networking
drivers
```

User-facing commands will eventually move into the Cruzeiro OS userspace instead of being implemented inside the kernel.