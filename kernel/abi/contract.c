#include <stddef.h>
#include <stdint.h>

#include <czk/abi.h>
#include <czk/types.h>
#include <czk/syscall.h>
#include <czk/errno.h>
#include <czk/fcntl.h>
#include <czk/seek.h>
#include <czk/fs.h>
#include <czk/stat.h>
#include <czk/dirent.h>

#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/vmm.h>

_Static_assert(CZK_ABI_VERSION_CURRENT == 0x00010000U,
               "ABI v1.0 encoding changed");
_Static_assert(CZK_ABI_SYSCALL_VECTOR == 0x80U,
               "syscall vector changed");
_Static_assert(CZK_ABI_WORD_BITS == 32U,
               "word size changed");
_Static_assert(CZK_ABI_POINTER_BITS == 32U,
               "pointer size changed");
_Static_assert(CZK_ABI_STACK_ALIGNMENT == 16U,
               "startup stack alignment changed");
_Static_assert(CZK_ABI_STACK_TOP == 0xFF800000U,
               "ABI v1 stack top changed");
_Static_assert(CZK_ABI_STACK_BOTTOM == 0xFF000000U,
               "ABI v1 stack reserve changed");
_Static_assert(CZK_ABI_STACK_INITIAL_PAGES == 4U,
               "ABI v1 initial stack mapping changed");

_Static_assert(sizeof(void *) == 4U,
               "CZK x86 ABI requires 32-bit pointers");
_Static_assert(sizeof(czk_pid_t) == 4U,
               "czk_pid_t width changed");
_Static_assert(sizeof(czk_ssize_t) == 4U,
               "czk_ssize_t width changed");
_Static_assert(sizeof(czk_size_t) == 4U,
               "czk_size_t width changed");
_Static_assert(sizeof(czk_off_t) == 4U,
               "czk_off_t width changed");

_Static_assert(CZK_SYS_WRITE == 1U, "SYS_WRITE renumbered");
_Static_assert(CZK_SYS_EXIT == 2U, "SYS_EXIT renumbered");
_Static_assert(CZK_SYS_GETPID == 3U, "SYS_GETPID renumbered");
_Static_assert(CZK_SYS_YIELD == 4U, "SYS_YIELD renumbered");
_Static_assert(CZK_SYS_GETPPID == 5U, "SYS_GETPPID renumbered");
_Static_assert(CZK_SYS_SLEEP_TICKS == 6U, "SYS_SLEEP_TICKS renumbered");
_Static_assert(CZK_SYS_WAIT == 7U, "SYS_WAIT renumbered");
_Static_assert(CZK_SYS_OPEN == 8U, "SYS_OPEN renumbered");
_Static_assert(CZK_SYS_READ == 9U, "SYS_READ renumbered");
_Static_assert(CZK_SYS_CLOSE == 10U, "SYS_CLOSE renumbered");
_Static_assert(CZK_SYS_LSEEK == 11U, "SYS_LSEEK renumbered");
_Static_assert(CZK_SYS_FSTAT == 12U, "SYS_FSTAT renumbered");
_Static_assert(CZK_SYS_READDIR == 13U, "SYS_READDIR renumbered");
_Static_assert(CZK_SYS_EXECVE == 14U, "SYS_EXECVE renumbered");
_Static_assert(CZK_SYS_ABI_VERSION == 15U, "SYS_ABI_VERSION renumbered");

_Static_assert(sizeof(czk_stat_t) == 8U,
               "czk_stat_t ABI layout changed");
_Static_assert(offsetof(czk_stat_t, st_size) == 0U,
               "czk_stat_t.st_size moved");
_Static_assert(offsetof(czk_stat_t, st_flags) == 4U,
               "czk_stat_t.st_flags moved");

_Static_assert(sizeof(czk_dirent_t) == 136U,
               "czk_dirent_t ABI layout changed");
_Static_assert(offsetof(czk_dirent_t, d_name) == 0U,
               "czk_dirent_t.d_name moved");
_Static_assert(offsetof(czk_dirent_t, d_flags) == 128U,
               "czk_dirent_t.d_flags moved");
_Static_assert(offsetof(czk_dirent_t, d_size) == 132U,
               "czk_dirent_t.d_size moved");

_Static_assert(FD_TABLE_MAX == CZK_OPEN_MAX,
               "kernel FD table diverged from ABI v1");
_Static_assert(VFS_NAME_MAX == CZK_NAME_MAX,
               "kernel VFS name limit diverged from ABI v1");
_Static_assert(VMM_USER_MIN_ADDR == CZK_ABI_USER_VA_MIN,
               "kernel user VA minimum diverged from ABI v1");
_Static_assert(VMM_USER_MAX_ADDR == CZK_ABI_USER_VA_MAX,
               "kernel user VA maximum diverged from ABI v1");
