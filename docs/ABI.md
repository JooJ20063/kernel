# CZK x86 Userspace ABI v1.0

Status: normative for the CZK_x86 userspace interface.

The public machine-readable definitions live under `uapi/include/czk/`.
This document defines the behavior those constants and structures represent.

## 1. Compatibility policy

ABI v1 follows these rules:

- existing syscall numbers are never renumbered or reused;
- existing public flag bits are never repurposed;
- existing public structure layouts do not change in place;
- compatible additions increment the ABI minor version;
- incompatible changes require a new ABI major version;
- a new syscall is added at a new number; an old number never changes meaning;
- programs must treat unknown trailing data after documented terminators as reserved;
- programs must handle `-CZK_ENOSYS` when probing a syscall unavailable on an older kernel.

The current version is encoded as:

```text
0xMMMMmmmm

MMMM = 16-bit major
mmmm = 16-bit minor
```

ABI v1.0 is therefore `0x00010000`.

`CZK_SYS_ABI_VERSION` returns this packed value.

## 2. Target machine and C data model

ABI v1 targets 32-bit x86 only:

- architecture: i386;
- byte order: little-endian;
- byte size: 8 bits;
- machine word: 32 bits;
- pointer size: 32 bits;
- userspace page size: 4096 bytes;
- C data model: ILP32;
- ordinary userspace C code follows the i386 System V / GCC cdecl calling convention.

Kernel syscalls use the separate register ABI defined below and do not use cdecl.

Stable scalar UAPI types are defined in `czk/types.h`:

| Type | ABI v1 representation |
| --- | --- |
| `czk_pid_t` | signed 32-bit |
| `czk_ssize_t` | signed 32-bit |
| `czk_size_t` | unsigned 32-bit |
| `czk_off_t` | signed 32-bit |

Successful byte counts and file offsets exposed directly in EAX are limited to `0x7fffffff` so they cannot collide with negative errno returns.

## 3. System call entry ABI

System calls enter the kernel using:

```asm
int $0x80
```

The syscall number is placed in `EAX`. Arguments are assigned in this fixed order:

| Argument | Register |
| --- | --- |
| syscall number | EAX |
| arg1 | EBX |
| arg2 | ECX |
| arg3 | EDX |
| arg4 | ESI |
| arg5 | EDI |
| arg6 | EBP |

ABI v1 permits at most six register arguments.

Return values use EAX:

- success: syscall-specific non-negative result;
- failure: `-CZK_E*` directly in EAX;
- unknown syscall: `-CZK_ENOSYS`.

The kernel ABI does not maintain a userspace `errno` variable. A future libc is expected to translate negative kernel results into its public libc convention when appropriate.

Pointers passed to the kernel are 32-bit userspace virtual addresses. The kernel validates user accessibility before dereferencing them.

## 4. Frozen syscall table

| Number | Name | Arguments | Success |
| ---: | --- | --- | --- |
| 1 | `CZK_SYS_WRITE` | EBX=fd, ECX=buf, EDX=count | bytes written |
| 2 | `CZK_SYS_EXIT` | EBX=status | does not return |
| 3 | `CZK_SYS_GETPID` | none | current PID |
| 4 | `CZK_SYS_YIELD` | none | 0 |
| 5 | `CZK_SYS_GETPPID` | none | parent PID |
| 6 | `CZK_SYS_SLEEP_TICKS` | EBX=ticks | 0 after reschedule |
| 7 | `CZK_SYS_WAIT` | EBX=int32_t *status or NULL | reaped direct-child PID |
| 8 | `CZK_SYS_OPEN` | EBX=path, ECX=flags | file descriptor |
| 9 | `CZK_SYS_READ` | EBX=fd, ECX=buf, EDX=count | bytes read |
| 10 | `CZK_SYS_CLOSE` | EBX=fd | 0 |
| 11 | `CZK_SYS_LSEEK` | EBX=fd, ECX=signed offset, EDX=whence | new offset |
| 12 | `CZK_SYS_FSTAT` | EBX=fd, ECX=czk_stat_t * | 0 |
| 13 | `CZK_SYS_READDIR` | EBX=fd, ECX=czk_dirent_t * | 1 entry, 0 EOF |
| 14 | `CZK_SYS_EXECVE` | EBX=path, ECX=argv, EDX=envp | does not return on success |
| 15 | `CZK_SYS_ABI_VERSION` | none | packed ABI version |

### write/read

`count` must not exceed `CZK_SSIZE_MAX`. A larger count fails with `CZK_EOVERFLOW`.

A zero-length transfer succeeds with zero.

### exit

The v1 kernel stores the supplied value as a signed 32-bit process exit code.

### yield

A successful yield returns zero when the process is scheduled again. No other return value is valid.

### sleep_ticks

The argument is expressed in kernel scheduler ticks. Tick frequency is intentionally not part of ABI v1; this syscall must not be exposed by libc as POSIX `sleep(3)` without an independent time conversion mechanism.

### wait

`CZK_SYS_WAIT` waits for any direct child.

- if a direct child is still running, the caller blocks;
- when a child becomes a zombie, one child is reaped and its PID is returned;
- if `status` is non-NULL, the child's signed 32-bit exit code is copied there;
- if the process has no direct children, the call fails with `CZK_ECHILD`;
- an invalid non-NULL status pointer fails with `CZK_EFAULT` without reaping a child.

This status word is a CZK raw exit code, not the POSIX encoded wait status format.

### open

ABI v1 recognizes:

- `CZK_O_RDONLY`;
- `CZK_O_WRONLY`;
- `CZK_O_RDWR`;
- `CZK_O_CREAT`.

Unknown flag bits fail with `CZK_EINVAL`.

There is no permissions/mode argument in ABI v1.

Path strings passed through the syscall ABI have a maximum storage size of `CZK_PATH_MAX`, including the terminating NUL. ABI v1 programs should use absolute paths; relative-path semantics are reserved for a future working-directory extension.

### lseek

The offset argument and successful result use the signed `czk_off_t` range. Results above `CZK_OFF_MAX` fail with `CZK_EOVERFLOW`.

Whence values are frozen in `czk/seek.h`.

### readdir

Each successful call advances the directory descriptor by one entry and returns 1. End-of-directory returns 0.

### execve

On success, `execve` replaces the current process image and does not return to the old image.

The following are preserved:

- PID;
- PPID;
- the current descriptor table, including offsets;
- foreground TTY ownership associated with the PID.

There is no close-on-exec flag in ABI v1, so every open descriptor survives a successful exec.

Limits:

- at most `CZK_EXEC_MAX_ARGS` argv entries;
- at most `CZK_EXEC_MAX_ENVS` envp entries;
- each copied argument/environment string occupies at most `CZK_EXEC_MAX_STRING` bytes including NUL.

Both vectors are terminated by a NULL pointer. A NULL vector pointer is treated as an empty vector.

## 5. File descriptor ABI

A new userspace process begins with:

| FD | Meaning |
| ---: | --- |
| 0 | stdin, readable |
| 1 | stdout, writable |
| 2 | stderr, writable |

The current descriptor-table capacity is frozen as `CZK_OPEN_MAX` entries for ABI v1.

Ordinary `open` allocations use descriptors at or above 3. Programs must not assume a particular descriptor number beyond the standard descriptors.

## 6. Public filesystem structures

`czk_stat_t` is permanently 8 bytes in ABI v1:

```c
struct czk_stat {
    uint32_t st_size;   /* offset 0 */
    uint32_t st_flags;  /* offset 4 */
};
```

`czk_dirent_t` is permanently 136 bytes:

```c
struct czk_dirent {
    char     d_name[128]; /* offset 0 */
    uint32_t d_flags;     /* offset 128 */
    uint32_t d_size;      /* offset 132 */
};
```

The stable `st_flags` / `d_flags` representation is defined by `czk/fs.h`:

- `CZK_FS_FILE`;
- `CZK_FS_DIRECTORY`;
- `CZK_FS_WRITABLE`;
- `CZK_FS_SEEKABLE`.

These bits are UAPI values. Userspace must not depend on the kernel's internal `fs_node` flag representation.

If more metadata is required later, ABI v1 structures are not extended in place. A new syscall or versioned structure must be introduced.

## 7. Executable ABI

The v1 native executable format is static ELF32 for i386:

- ELF class: 32-bit;
- data encoding: little-endian;
- ELF version: current;
- type: `ET_EXEC`;
- machine: `EM_386`;
- program headers are required;
- at least one non-empty `PT_LOAD` is required;
- `PT_INTERP` is unsupported;
- `PT_DYNAMIC` is unsupported;
- relocations and PIE/`ET_DYN` are unsupported;
- the entry point must lie inside a `PF_X` load segment;
- `p_filesz <= p_memsz`;
- bytes in `p_memsz - p_filesz` are zero-filled;
- `p_align > 1` must be a power of two and satisfy ELF offset/address congruence.

Loadable virtual memory must fit entirely inside:

```text
CZK_ABI_USER_VA_MIN <= PT_LOAD < CZK_ABI_USER_VA_MAX
0x01000000                         0xFF800000
```

The upper bound is exclusive.

ABI v1 does not provide hardware NX enforcement on CZK_x86; ELF execute flags are nevertheless validated and retained as executable-format semantics.

## 8. Initial process stack

At ELF entry:

- EIP equals the ELF `e_entry`;
- ESP is aligned to `CZK_ABI_STACK_ALIGNMENT` (16 bytes);
- general-purpose register values other than ESP are unspecified;
- valid Ring 3 code/data segments are installed.

The stack beginning at ESP is:

```text
ESP ->  argc
        argv[0]
        argv[1]
        ...
        argv[argc-1]
        NULL
        envp[0]
        envp[1]
        ...
        NULL
        [reserved/padding/future data]
        copied strings
```

Each pointer is a 32-bit userspace virtual address.

Programs must stop argv and envp at their NULL terminators and must not assume that the memory after the envp terminator is absent. A future compatible minor ABI may place auxiliary-vector style data there.

The supplied `crt0` decodes this stack and calls:

```c
int main(int argc, char **argv, char **envp);
```

using the i386 cdecl convention. The return value of `main` is passed to `CZK_SYS_EXIT`.

## 9. ABI and libc boundary

This document defines the kernel/userspace ABI, not the final C library API.

A libc may expose familiar functions such as `write`, `read`, `open`, `execve`, `wait`, and `exit`, but it is responsible for adapting libc types and errno behavior to this kernel contract.

Future libc work must not silently redefine the machine ABI documented here.

## 10. ABI v1 extension rules

Compatible v1.x additions may include:

- new syscall numbers appended after the current table;
- new errno values;
- new open/feature flag bits where old kernels already reject unknown bits;
- new standalone public structures;
- optional startup data after the envp NULL terminator;
- new executable capabilities that do not invalidate existing static ELF32 binaries.

The following require an ABI major-version change:

- changing syscall number meanings;
- changing the syscall register convention;
- changing existing public structure sizes or field offsets;
- changing the interpretation of existing flag bits;
- changing the process-entry stack prefix;
- making a previously valid v1 static ELF32 image invalid solely because of a new loader convention.

The kernel compile-time contract in `kernel/abi/contract.c` guards the frozen constants and layouts. The userspace `/bin/abiprobe` provides a runtime smoke test of the active ABI.
