.section .usertext, "ax", @progbits

.set CZK_EBADF, 9
.set CZK_ECHILD, 10
.set CZK_EFAULT, 14
.set CZK_ENOSYS, 38
.set CZK_ESPIPE, 29

.set CZK_O_RDONLY, 0x0000
.set CZK_O_RDWR,   0x0002
.set CZK_O_CREAT,  0x0040

.set CZK_SEEK_SET, 0
.global user_test_entry

user_test_entry:
    # write("Hello from ring 3\n")
    mov $1, %eax
    mov $1, %ebx
    mov $user_message, %ecx
    mov $(user_message_end-user_message), %edx
    int $0x80

    # Attempt to pass a kernel-only address to write().
    # The kernel must reject it with -1 instead of faulting.
    mov $1, %eax
    mov $1, %ebx
    mov $0x00100000, %ecx
    mov $4, %edx
    int $0x80

    cmp $-CZK_EFAULT, %eax
    jne uaccess_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $uaccess_ok, %ecx
    mov $(uaccess_ok_end-uaccess_ok), %edx
    int $0x80
    jmp uaccess_test_done

uaccess_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $uaccess_fail, %ecx
    mov $(uaccess_fail_end-uaccess_fail), %edx
    int $0x80

uaccess_test_done:

    # Invalid fd must return -EBADF.
    mov $1, %eax
    mov $99, %ebx
    mov $user_message, %ecx
    mov $1, %edx
    int $0x80

    cmp $-CZK_EBADF, %eax
    jne abi_error_test_failed

    # wait(NULL) with no child must return -ECHILD.
    mov $7, %eax
    xor %ebx, %ebx
    int $0x80

    cmp $-CZK_ECHILD, %eax
    jne abi_error_test_failed

    # Unknown syscall must return -ENOSYS.
    mov $0x7FFFFFFF, %eax
    int $0x80

    cmp $-CZK_ENOSYS, %eax
    jne abi_error_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $abi_error_ok, %ecx
    mov $(abi_error_ok_end-abi_error_ok), %edx
    int $0x80
    jmp abi_error_test_done

abi_error_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $abi_error_fail, %ecx
    mov $(abi_error_fail_end-abi_error_fail), %edx
    int $0x80

abi_error_test_done:

    # fd 2 is stderr and must be writable.
    mov $1, %eax
    mov $2, %ebx
    mov $fd_stderr_message, %ecx
    mov $(fd_stderr_message_end-fd_stderr_message), %edx
    int $0x80

    cmp $(fd_stderr_message_end-fd_stderr_message), %eax
    jne fd_table_test_failed

    # fd 0 is stdin and must reject write().
    mov $1, %eax
    xor %ebx, %ebx
    mov $user_message, %ecx
    mov $1, %edx
    int $0x80

    cmp $-CZK_EBADF, %eax
    jne fd_table_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $fd_table_ok, %ecx
    mov $(fd_table_ok_end-fd_table_ok), %edx
    int $0x80
    jmp fd_table_test_done

fd_table_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $fd_table_fail, %ecx
    mov $(fd_table_fail_end-fd_table_fail), %edx
    int $0x80

fd_table_test_done:

    # open("ring3.txt", O_CREAT | O_RDWR)
    mov $8, %eax
    mov $fileio_path, %ebx
    mov $(CZK_O_CREAT | CZK_O_RDWR), %ecx
    int $0x80

    cmp $3, %eax
    jl fileio_test_failed

    # write(fd, payload, payload_len)
    mov %eax, %ebx
    mov $1, %eax
    mov $fileio_payload, %ecx
    mov $(fileio_payload_end-fileio_payload), %edx
    int $0x80

    cmp $(fileio_payload_end-fileio_payload), %eax
    jne fileio_test_failed

    # close(fd)
    mov $10, %eax
    int $0x80

    test %eax, %eax
    jne fileio_test_failed

    # reopen read-only
    mov $8, %eax
    mov $fileio_path, %ebx
    mov $CZK_O_RDONLY, %ecx
    int $0x80

    cmp $3, %eax
    jl fileio_test_failed

    # fstat(fd, &stat)
    mov %eax, %ebx
    mov $12, %eax
    mov $fileio_stat, %ecx
    int $0x80

    test %eax, %eax
    jne fileio_test_failed

    cmpl $(fileio_payload_end-fileio_payload), fileio_stat
    jne fileio_test_failed

    # read(fd, buffer, payload_len)
    mov $9, %eax
    mov $fileio_buffer, %ecx
    mov $(fileio_payload_end-fileio_payload), %edx
    int $0x80

    cmp $(fileio_payload_end-fileio_payload), %eax
    jne fileio_test_failed

    # Compare the bytes read back from RAMFS.
    cld
    mov $fileio_payload, %esi
    mov $fileio_buffer, %edi
    mov $(fileio_payload_end-fileio_payload), %ecx
    repe cmpsb
    jne fileio_test_failed

    # lseek(fd, 4, SEEK_SET)
    mov $11, %eax
    mov $4, %ecx
    mov $CZK_SEEK_SET, %edx
    int $0x80

    cmp $4, %eax
    jne fileio_test_failed

    # Read a slice after seeking and compare it with payload + 4.
    mov $9, %eax
    mov $fileio_seek_buffer, %ecx
    mov $8, %edx
    int $0x80

    cmp $8, %eax
    jne fileio_test_failed

    cld
    mov $(fileio_payload + 4), %esi
    mov $fileio_seek_buffer, %edi
    mov $8, %ecx
    repe cmpsb
    jne fileio_test_failed

    # close(fd)
    mov $10, %eax
    int $0x80

    test %eax, %eax
    jne fileio_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $fileio_ok, %ecx
    mov $(fileio_ok_end-fileio_ok), %edx
    int $0x80
    jmp fileio_test_done

fileio_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $fileio_fail, %ecx
    mov $(fileio_fail_end-fileio_fail), %edx
    int $0x80

fileio_test_done:

    # open("/", O_RDONLY)
    mov $8, %eax
    mov $root_path, %ebx
    mov $CZK_O_RDONLY, %ecx
    int $0x80

    cmp $3, %eax
    jl dirent_test_failed

    mov %eax, %ebx

dirent_scan_next:
    # readdir(fd, &dirent)
    mov $13, %eax
    mov $dirent_buffer, %ecx
    int $0x80

    # EOF before finding ring3.txt is a failure.
    test %eax, %eax
    jz dirent_test_failed

    cmp $1, %eax
    jne dirent_test_failed

    # Root now contains standard directories. Scan until ring3.txt is found.
    cld
    mov $fileio_path, %esi
    mov $dirent_buffer, %edi
    mov $10, %ecx
    repe cmpsb
    jne dirent_scan_next

    # d_flags must include FS_FILE and d_size must match the payload.
    testl $1, dirent_buffer+128
    jz dirent_test_failed

    cmpl $(fileio_payload_end-fileio_payload), dirent_buffer+132
    jne dirent_test_failed

    # close(directory fd)
    mov $10, %eax
    int $0x80

    test %eax, %eax
    jne dirent_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $dirent_ok, %ecx
    mov $(dirent_ok_end-dirent_ok), %edx
    int $0x80
    jmp dirent_test_done

dirent_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $dirent_fail, %ecx
    mov $(dirent_fail_end-dirent_fail), %edx
    int $0x80

dirent_test_done:

    # stdout is not seekable.
    mov $11, %eax
    mov $1, %ebx
    xor %ecx, %ecx
    mov $CZK_SEEK_SET, %edx
    int $0x80

    cmp $-CZK_ESPIPE, %eax
    jne seek_stdio_test_failed

    mov $1, %eax
    mov $1, %ebx
    mov $seek_stat_ok, %ecx
    mov $(seek_stat_ok_end-seek_stat_ok), %edx
    int $0x80
    jmp seek_stdio_test_done

seek_stdio_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $seek_stat_fail, %ecx
    mov $(seek_stat_fail_end-seek_stat_fail), %edx
    int $0x80

seek_stdio_test_done:

    # getpid()
    mov $3, %eax
    int $0x80

    # EAX = PID
    add $'0', %al
    mov %al, user_pid_digit

    # write("pid=")
    mov $1, %eax
    mov $1, %ebx
    mov $pid_message, %ecx
    mov $(pid_message_end-pid_message), %edx
    int $0x80

    # write("Before sleep\n")
    mov $1, %eax
    mov $1, %ebx
    mov $before_sleep, %ecx
    mov $(before_sleep_end-before_sleep), %edx
    int $0x80

    # sleep(300)
    mov $6, %eax
    mov $300, %ebx
    int $0x80

    # write("After sleep\n")
    mov $1, %eax
    mov $1, %ebx
    mov $after_sleep, %ecx
    mov $(after_sleep_end-after_sleep), %edx
    int $0x80

    # write "before yield\n"
    mov $1, %eax
    mov $1, %ebx
    mov $before_yield, %ecx
    mov $(before_yield_end-before_yield), %edx
    int $0x80

    # yield
    mov $4, %eax
    int $0x80

    # write "after yield\n"
    mov $1, %eax
    mov $1, %ebx
    mov $after_yield, %ecx
    mov $(after_yield_end-after_yield), %edx
    int $0x80

    # getppid()
    mov $5, %eax
    int $0x80

    # EAX = PPID
    add $'0', %al
    mov %al, user_ppid_digit

    #write("ppid=")
    mov $1, %eax
    mov $1, %ebx
    mov $ppid_message, %ecx
    mov $(ppid_message_end-ppid_message), %edx
    int $0x80

    # exit(42)
    mov $2, %eax
    mov $42, %ebx
    int $0x80

1:
    jmp 1b


.global user_tty_block_test_entry
user_tty_block_test_entry:
    # Announce before blocking on stdin.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_block_waiting, %ecx
    mov $(tty_block_waiting_end-tty_block_waiting), %edx
    int $0x80

    # read(0, &tty_block_byte, 1) must sleep until the waker injects input.
    mov $9, %eax
    xor %ebx, %ebx
    mov $tty_block_byte, %ecx
    mov $1, %edx
    int $0x80

    cmp $1, %eax
    jne tty_block_failed

    cmpb $'Z', tty_block_byte
    jne tty_block_failed

    mov $1, %eax
    mov $1, %ebx
    mov $tty_block_ok, %ecx
    mov $(tty_block_ok_end-tty_block_ok), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

tty_block_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $tty_block_fail, %ecx
    mov $(tty_block_fail_end-tty_block_fail), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_tty_foreground_test_entry
user_tty_foreground_test_entry:
    # Manual foreground test: any single input byte is valid.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_waiting, %ecx
    mov $(tty_fg_waiting_end-tty_fg_waiting), %edx
    int $0x80

    mov $9, %eax
    xor %ebx, %ebx
    mov $tty_fg_byte, %ecx
    mov $1, %edx
    int $0x80

    cmp $1, %eax
    jne tty_fg_failed

    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_prefix, %ecx
    mov $(tty_fg_prefix_end-tty_fg_prefix), %edx
    int $0x80

    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_byte, %ecx
    mov $1, %edx
    int $0x80

    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_suffix, %ecx
    mov $(tty_fg_suffix_end-tty_fg_suffix), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

tty_fg_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $tty_fg_fail, %ecx
    mov $(tty_fg_fail_end-tty_fg_fail), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_tty_canonical_test_entry
user_tty_canonical_test_entry:
    # Canonical read must not return until Enter commits the line.
    mov $1, %eax
    mov $1, %ebx
    mov $tty_canon_waiting, %ecx
    mov $(tty_canon_waiting_end-tty_canon_waiting), %edx
    int $0x80

    mov $9, %eax
    xor %ebx, %ebx
    mov $tty_canon_buffer, %ecx
    mov $32, %edx
    int $0x80

    cmp $6, %eax
    jne tty_canon_failed

    cmpb user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

tty_canon_waiting:
    .ascii "ttycanon: waiting for line\n"
tty_canon_waiting_end:

tty_canon_ok:
    .ascii "ttycanon: canonical read/backspace OK\n"
tty_canon_ok_end:

tty_canon_fail:
    .ascii "ttycanon: FAILED\n"
tty_canon_fail_end:

.align 16
tty_canon_buffer:
    .skip 32

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
h', tty_canon_buffer+0
    jne tty_canon_failed
    cmpb user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
e', tty_canon_buffer+1
    jne tty_canon_failed
    cmpb user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
l', tty_canon_buffer+2
    jne tty_canon_failed
    cmpb user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
l', tty_canon_buffer+3
    jne tty_canon_failed
    cmpb user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
o', tty_canon_buffer+4
    jne tty_canon_failed
    cmpb user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
\n', tty_canon_buffer+5
    jne tty_canon_failed

    mov $1, %eax
    mov $1, %ebx
    mov $tty_canon_ok, %ecx
    mov $(tty_canon_ok_end-tty_canon_ok), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

tty_canon_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $tty_canon_fail, %ecx
    mov $(tty_canon_fail_end-tty_canon_fail), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_ud_test_entry
user_ud_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $ud_test_message, %ecx
    mov $(ud_test_message_end-ud_test_message), %edx
    int $0x80

    ud2

    mov $2, %eax
    mov $98, %ebx
    int $0x80

1:
    jmp 1b


.global user_gp_test_entry
user_gp_test_entry:
    mov $1, %eax
    mov $1, %ebx
    mov $gp_test_message, %ecx
    mov $(gp_test_message_end-gp_test_message), %edx
    int $0x80

    cli

    mov $2, %eax
    mov $97, %ebx
    int $0x80

1:
    jmp 1b


.global user_aspace_test_entry
user_aspace_test_entry:
    cmpl $0, aspace_private_word
    jne aspace_test_failed

    movl $0xA5A5A5A5, aspace_private_word

    mov $1, %eax
    mov $1, %ebx
    mov $aspace_ok_message, %ecx
    mov $(aspace_ok_message_end-aspace_ok_message), %edx
    int $0x80

    mov $2, %eax
    xor %ebx, %ebx
    int $0x80

aspace_test_failed:
    mov $1, %eax
    mov $1, %ebx
    mov $aspace_fail_message, %ecx
    mov $(aspace_fail_message_end-aspace_fail_message), %edx
    int $0x80

    mov $2, %eax
    mov $1, %ebx
    int $0x80

1:
    jmp 1b


.global user_fault_test_entry
user_fault_test_entry:
    # Announce the deliberate fault through the normal syscall path.
    mov $1, %eax
    mov $1, %ebx
    mov $fault_test_message, %ecx
    mov $(fault_test_message_end-fault_test_message), %edx
    int $0x80

    # Deliberate Ring 3 write to the unmapped null page.
    movl $0xDEADBEEF, 0x00000000

    # This path must never execute.
    mov $2, %eax
    mov $99, %ebx
    int $0x80

1:
    jmp 1b


.section .userdata, "aw", @progbits
.align 16

.global syscall_test_message
.global syscall_test_message_end
syscall_test_message:
    .ascii "hello from int 0x80\n"
syscall_test_message_end:

.align 16
user_stack_bottom:
    .skip 4096

.global user_stack_top
user_stack_top:

user_message:
    .ascii "Hello from ring 3\n"
user_message_end:

fault_test_message:
    .ascii "ring3fault: touching null page\n"
fault_test_message_end:

ud_test_message:
    .ascii "ring3ud: executing UD2\n"
ud_test_message_end:

gp_test_message:
    .ascii "ring3gp: executing privileged CLI\n"
gp_test_message_end:

aspace_ok_message:
    .ascii "aspace: private userdata ok\n"
aspace_ok_message_end:

aspace_fail_message:
    .ascii "aspace: private userdata FAILED\n"
aspace_fail_message_end:

.align 4
aspace_private_word:
    .long 0

uaccess_ok:
    .ascii "uaccess: kernel pointer rejected\n"
uaccess_ok_end:

uaccess_fail:
    .ascii "uaccess: kernel pointer ACCEPTED\n"
uaccess_fail_end:

abi_error_ok:
    .ascii "abi: typed syscall errors ok\n"
abi_error_ok_end:

abi_error_fail:
    .ascii "abi: typed syscall errors FAILED\n"
abi_error_fail_end:

fd_stderr_message:
    .ascii "fdtable: hello from stderr\n"
fd_stderr_message_end:

fd_table_ok:
    .ascii "fdtable: stdio routing ok\n"
fd_table_ok_end:

fd_table_fail:
    .ascii "fdtable: stdio routing FAILED\n"
fd_table_fail_end:

tty_block_waiting:
    .ascii "ttyblock: waiting for stdin\n"
tty_block_waiting_end:

tty_block_ok:
    .ascii "ttyblock: read woke with Z\n"
tty_block_ok_end:

tty_block_fail:
    .ascii "ttyblock: FAILED\n"
tty_block_fail_end:

tty_block_byte:
    .byte 0

tty_fg_waiting:
    .ascii "ttyfg: waiting for one input byte\n"
tty_fg_waiting_end:

tty_fg_prefix:
    .ascii "ttyfg: read byte='"
tty_fg_prefix_end:

tty_fg_suffix:
    .ascii "' OK\n"
tty_fg_suffix_end:

tty_fg_fail:
    .ascii "ttyfg: read FAILED\n"
tty_fg_fail_end:

tty_fg_byte:
    .byte 0

fileio_path:
    .asciz "ring3.txt"

fileio_payload:
    .ascii "CZK userspace file I/O works\n"
fileio_payload_end:

fileio_ok:
    .ascii "fileio: roundtrip ok\n"
fileio_ok_end:

fileio_fail:
    .ascii "fileio: roundtrip FAILED\n"
fileio_fail_end:

seek_stat_ok:
    .ascii "fileio: lseek/fstat ok\n"
seek_stat_ok_end:

seek_stat_fail:
    .ascii "fileio: lseek/fstat FAILED\n"
seek_stat_fail_end:

dirent_ok:
    .ascii "fileio: readdir ok\n"
dirent_ok_end:

dirent_fail:
    .ascii "fileio: readdir FAILED\n"
dirent_fail_end:

root_path:
    .asciz "/"

.align 16
dirent_buffer:
    .skip 136

.align 16
fileio_buffer:
    .skip 64

.align 4
fileio_stat:
    .skip 8

.align 16
fileio_seek_buffer:
    .skip 16

pid_message:
    .ascii "pid="
user_pid_digit:
    .byte '0'
    .ascii "\n"
pid_message_end:

ppid_message:
    .ascii "ppid="
user_ppid_digit:
    .byte '0'
    .ascii "\n"
ppid_message_end:

before_yield:
    .ascii "before yield\n"
before_yield_end:
after_yield:
    .ascii "after yield\n"
after_yield_end:

before_sleep:
    .ascii "Before sleep\n"
before_sleep_end:
after_sleep:
    .ascii "After sleep\n"
after_sleep_end:

.section .note.GNU-stack,"",@progbits
