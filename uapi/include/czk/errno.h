#ifndef CZK_UAPI_ERRNO_H
#define CZK_UAPI_ERRNO_H

/*
 * Cruzeiro syscall ABI error numbers.
 *
 * Syscalls return -CZK_E* in EAX on failure.  Positive values and zero
 * retain their syscall-specific success meaning.
 */
#define CZK_EBADF   9
#define CZK_ECHILD 10
#define CZK_EFAULT 14
#define CZK_ENOSYS 38

#endif
