#ifndef CZK_UAPI_SYSCALL_H
#define CZK_UAPI_SYSCALL_H

/*
 * Stable syscall numbers for CZK x86 ABI v1.
 * Existing numbers are append-only and are never reused.
 */
#define CZK_SYS_WRITE        1U
#define CZK_SYS_EXIT         2U
#define CZK_SYS_GETPID       3U
#define CZK_SYS_YIELD        4U
#define CZK_SYS_GETPPID      5U
#define CZK_SYS_SLEEP_TICKS  6U
#define CZK_SYS_WAIT         7U
#define CZK_SYS_OPEN         8U
#define CZK_SYS_READ         9U
#define CZK_SYS_CLOSE       10U
#define CZK_SYS_LSEEK       11U
#define CZK_SYS_FSTAT       12U
#define CZK_SYS_READDIR     13U
#define CZK_SYS_EXECVE      14U
#define CZK_SYS_ABI_VERSION 15U

/*
 * Compatibility aliases retained for existing kernel/tests/userspace.
 */
#define SYS_WRITE       CZK_SYS_WRITE
#define SYS_EXIT        CZK_SYS_EXIT
#define SYS_GETPID      CZK_SYS_GETPID
#define SYS_YIELD       CZK_SYS_YIELD
#define SYS_GETPPID     CZK_SYS_GETPPID
#define SYS_SLEEP       CZK_SYS_SLEEP_TICKS
#define SYS_WAIT        CZK_SYS_WAIT
#define SYS_OPEN        CZK_SYS_OPEN
#define SYS_READ        CZK_SYS_READ
#define SYS_CLOSE       CZK_SYS_CLOSE
#define SYS_LSEEK       CZK_SYS_LSEEK
#define SYS_FSTAT       CZK_SYS_FSTAT
#define SYS_READDIR     CZK_SYS_READDIR
#define SYS_EXECVE      CZK_SYS_EXECVE
#define SYS_ABI_VERSION CZK_SYS_ABI_VERSION

#endif
