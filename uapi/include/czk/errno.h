#ifndef CZK_UAPI_ERRNO_H
#define CZK_UAPI_ERRNO_H

/*
 * Cruzeiro syscall ABI error numbers.
 *
 * Syscalls return -CZK_E* in EAX on failure.  Positive values and zero
 * retain their syscall-specific success meaning.
 */
#define CZK_ENOENT  2
#define CZK_E2BIG   7
#define CZK_ENOEXEC 8
#define CZK_EBADF   9
#define CZK_ENOMEM 12
#define CZK_EACCES 13
#define CZK_ECHILD 10
#define CZK_EFAULT 14
#define CZK_ENOTDIR 20
#define CZK_EINVAL 22
#define CZK_ESPIPE 29
#define CZK_EMFILE 24
#define CZK_ENAMETOOLONG 36
#define CZK_ENOSYS 38

#endif
