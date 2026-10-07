#ifndef CZK_UAPI_ABI_H
#define CZK_UAPI_ABI_H

/*
 * Cruzeiro OS userspace ABI v1.
 *
 * ABI stability rules:
 * - existing syscall numbers are never renumbered or reused;
 * - existing flag bits and public structure layouts are never repurposed;
 * - compatible additions increment the minor version;
 * - incompatible changes require a new major version.
 */
#define CZK_ABI_VERSION_MAJOR 1U
#define CZK_ABI_VERSION_MINOR 0U

#define CZK_ABI_VERSION_ENCODE(major, minor) \
    ((((major) & 0xFFFFU) << 16) | ((minor) & 0xFFFFU))

#define CZK_ABI_VERSION_CURRENT \
    CZK_ABI_VERSION_ENCODE( \
        CZK_ABI_VERSION_MAJOR, \
        CZK_ABI_VERSION_MINOR \
    )

#define CZK_ABI_ARCH_I386       1U
#define CZK_ABI_WORD_BITS       32U
#define CZK_ABI_POINTER_BITS    32U
#define CZK_ABI_LITTLE_ENDIAN   1U
#define CZK_ABI_PAGE_SIZE       4096U

#define CZK_ABI_SYSCALL_VECTOR   0x80U
#define CZK_ABI_SYSCALL_MAX_ARGS 6U
#define CZK_ABI_ERRNO_MAX        4095U

#define CZK_ABI_USER_VA_MIN 0x01000000U
#define CZK_ABI_USER_VA_MAX 0xFF800000U

#define CZK_ABI_STACK_ALIGNMENT 16U

#define CZK_PATH_MAX        128U
#define CZK_NAME_MAX        128U
#define CZK_EXEC_MAX_ARGS    16U
#define CZK_EXEC_MAX_ENVS    16U
#define CZK_EXEC_MAX_STRING 128U

#define CZK_SSIZE_MAX 0x7FFFFFFF
#define CZK_OFF_MAX   0x7FFFFFFF

#define CZK_STDIN_FILENO  0
#define CZK_STDOUT_FILENO 1
#define CZK_STDERR_FILENO 2

#endif
