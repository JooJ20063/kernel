#include <kernel/shell.h>
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>
#include <kernel/sched.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/serial.h>
#include <kernel/tty.h>
#include <kernel/fd.h>


#ifdef __x86_64__
#include <arch/x86_64/irq.h>
#include <arch/x86_64/regs.h>
#else
#include <arch/x86/irq.h>
#include <arch/x86/regs.h>
#include <arch/x86/tss.h>
#endif

#ifndef __x86_64__
extern void user_test_entry(void);
extern void user_fault_test_entry(void);
extern void user_ud_test_entry(void);
extern void user_gp_test_entry(void);
extern void user_aspace_test_entry(void);
extern uint8_t user_stack_top;
extern void enter_ring3(uint32_t entry, uint32_t user_stack);
#endif

#define SHELL_BUF 128
#define KHEAP_SLOTS 16

static char line[SHELL_BUF];
static uint32_t line_len;
static void *heap_slots[KHEAP_SLOTS];
static uint32_t heap_slot_sizes[KHEAP_SLOTS];

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static int str_starts(const char *a, const char *prefix) {
    while (*prefix) {
        if (*a++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static uint32_t str_len(const char *s) {
    uint32_t n = 0;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

static int str_find(const char *s, const char *pat) {
    uint32_t i = 0;

    if (pat[0] == 0) {
        return 0;
    }

    while (s[i] != 0) {
        uint32_t j = 0;
        while (pat[j] != 0 && s[i + j] != 0 && s[i + j] == pat[j]) {
            j++;
        }

        if (pat[j] == 0) {
            return (int)i;
        }

        i++;
    }

    return -1;
}

static void str_copy_range(char *dst, uint32_t dst_cap, const char *src, uint32_t start, uint32_t end) {
    uint32_t i = 0;

    if (dst_cap == 0) {
        return;
    }

    while ((start + i) < end && src[start + i] != 0 && i + 1U < dst_cap) {
        dst[i] = src[start + i];
        i++;
    }

    dst[i] = 0;
}

static uint32_t parse_u32(const char *s, int *ok) {
    uint32_t v = 0;
    *ok = 0;

    if (*s == 0) {
        return 0;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            return 0;
        }

        v = (v * 10U) + (uint32_t)(*s - '0');
        s++;
    }

    *ok = 1;
    return v;
}

static int is_space(char c) {
    return (c == ' ' || c == '\t');
}

static const char *skip_spaces(const char *s) {
    while (*s && is_space(*s)) {
        s++;
    }
    return s;
}

static uint32_t parse_hex_u32(const char *s, int *ok) {
    uint32_t v = 0;
    *ok = 0;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    if (*s == 0) {
        return 0;
    }

    while (*s) {
        char c = *s;
        uint32_t d;

        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = 10U + (uint32_t)(c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            d = 10U + (uint32_t)(c - 'A');
        } else {
            return 0;
        }

        v = (v << 4) | d;
        s++;
    }

    *ok = 1;
    return v;
}

static void shell_prompt(void) {
    vga_set_color(0x0B, 0x00);
    vga_puts("\n$ ");
    vga_set_color(0x0F, 0x00);
}

static int heap_find_free_slot(void) {
    for (int i = 0; i < KHEAP_SLOTS; ++i) {
        if (heap_slots[i] == 0) {
            return i;
        }
    }
    return -1;
}

static int heap_slot_valid(uint32_t slot) {
    return (slot < KHEAP_SLOTS && heap_slots[slot] != 0);
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
__attribute__((noinline, noreturn, disable_tail_calls))
#else
__attribute__((noinline, noreturn))
#endif
static void force_stack_overflow(uint32_t depth) {

    volatile uint8_t buffer[4096];

    buffer[0] = (uint8_t)depth;
    buffer[4095] = (uint8_t)(depth >> 8);

    asm volatile ("" : : "g"(buffer) : "memory");

    force_stack_overflow(depth + 1);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static void shell_cmd_pfault(const char *arg) {
    if (arg == 0 || *arg ==0){
        klog_warn("usage: pfault [null|readonly|invalid|stack]");
        return;
    }

    if (str_eq(arg, "null")) {
        volatile uint32_t *p = (volatile uint32_t *)0x0;
        *p = 0xDEADBEEFU;
        return;
    }

    if (str_eq(arg, "readonly")) {
        extern uint8_t _rodata_start;
        volatile uint32_t *p = (volatile uint32_t *)&_rodata_start;
        *p = 0xCAFEBABEU;
        return;
    }

    if (str_eq(arg, "invalid")) {
        volatile uint32_t *p = (volatile  uint32_t *)0xA000000U;
        *p = 0x12345678U;
        return;
    }

    if (str_eq(arg, "stack")) {
        force_stack_overflow(0);
        return;
}

    klog_warn("usage:pfault [null|readonly|invalid|stack]");
}

static void shell_cmd_panic(const char *arg) {
    if (arg == 0 || *arg == 0) {
        kernel_panic("panic command invoked", 0);
        return;
    }

    if (str_eq(arg, "int3")) {
        asm volatile ("int3");
        return;
    }

    if (str_eq(arg, "ud2")) {
        asm volatile ("ud2");
        return;
    }

    if (str_eq(arg, "div0")) {
        // Trigger divide by zero interrupt (intentionally disabled)
        // asm volatile (
        //     "xor %%edx, %%edx\n"
        //     "xor %%eax, %%eax\n"
        //     "div %%edx\n"
        //     :
        //     :
        //     : "eax", "edx");
        klog_warn("div0: disabled - use 'panic ud2' or 'panic int3' instead");
        return;
    }

    if (str_eq(arg, "null")) {
        volatile uint32_t *pnull = (volatile uint32_t *)0x0;
        *pnull = 0xDEADBEEFU;
        return;
    }

    if (str_starts(arg, "int ")) {
        int ok;
        uint32_t int_no = parse_u32(arg + 4, &ok);
        if (ok) {
            registers_t fake = {0};
            fake.int_no = int_no;
            fake.err = 0;
            kernel_panic("manual interrupt panic", &fake);
            return;
        }
    }

    klog_warn("panic usage: panic [int3|ud2|div0|null|int <n>]");
}

static void shell_cmd_ls(const char *path) {
    const char *target = (path == 0 || *path == 0) ? "/" : path;
    fs_node_t *directory = vfs_resolve(target);
    uint32_t i = 0U;

    if (directory == 0) {
        klog_warn("diretorio nao encontrado");
        return;
    }

    if ((directory->flags & FS_DIRECTORY) == 0U) {
        klog_warn("ls: caminho nao e diretorio");
        return;
    }

    vga_puts("entries ");
    vga_puts(target);
    vga_puts(":\n");

    for (;;) {
        fs_node_t *entry = readdir_fs(directory, i);

        if (entry == 0) {
            break;
        }

        vga_puts(" - ");
        vga_puts(entry->name);

        if ((entry->flags & FS_DIRECTORY) != 0U) {
            vga_puts("/");
        } else {
            vga_puts(" (");
            vga_putdec(entry->size);
            vga_puts(" bytes)");
        }

        vga_puts("\n");
        i++;
    }

    if (i == 0U) {
        vga_puts("(vazio)\n");
    }
}

static void shell_cmd_cat(const char *name) {
    fs_node_t *entry;

    if (name == 0 || *name == 0) {
        klog_warn("usage: cat <arquivo>");
        return;
    }

    entry = vfs_resolve(name);
    if (entry == 0) {
        klog_warn("arquivo nao encontrado");
        return;
    }

    if ((entry->flags & FS_FILE) == 0U) {
        klog_warn("cat: caminho nao e arquivo");
        return;
    }

    {
        uint8_t buf[64];
        uint32_t off = 0U;

        while (off < entry->size) {
            uint32_t n = read_fs(entry, off, sizeof(buf), buf);

            if (n == 0U) {
                break;
            }

            for (uint32_t j = 0U; j < n; ++j) {
                vga_putc((char)buf[j]);
            }

            off += n;
        }
    }

    vga_puts("\n");
}

static void shell_write_text_file(const char *name, const char *text) {
    fs_node_t *entry;
    uint32_t text_length;
    uint32_t n;

    if (name == 0 || *name == 0) {
        klog_warn("arquivo invalido");
        return;
    }

    entry = vfs_create(name, FS_FILE | FS_WRITABLE);
    if (entry == 0) {
        klog_warn("falha ao criar/abrir arquivo");
        return;
    }

    text_length = str_len(text);
    n = write_fs(entry, 0U, text_length, (const uint8_t *)text);

    if (n != text_length) {
        klog_warn("arquivo somente leitura ou sem memoria");
        return;
    }

    vga_puts("ok: ");
    vga_puts(name);
    vga_puts(" <= ");
    vga_putdec(n);
    vga_puts(" bytes\n");
}

static void shell_cmd_touch(const char *name) {
    fs_node_t *entry;

    if (name == 0 || *name == 0) {
        klog_warn("usage: touch <arquivo>");
        return;
    }

    entry = vfs_create(name, FS_FILE | FS_WRITABLE);
    if (entry == 0) {
        klog_warn("touch falhou");
        return;
    }

    vga_puts("touch: ");
    vga_puts(name);
    vga_puts("\n");
}

static void shell_cmd_mkdir(const char *path) {
    fs_node_t *entry;

    if (path == 0 || *path == 0) {
        klog_warn("usage: mkdir <diretorio>");
        return;
    }

    entry = vfs_create(path, FS_DIRECTORY);
    if (entry == 0 || (entry->flags & FS_DIRECTORY) == 0U) {
        klog_warn("mkdir falhou");
        return;
    }

    vga_puts("mkdir: ");
    vga_puts(path);
    vga_puts("\n");
}

static void shell_cmd_virt(const char *arg) {
    int ok;
    uintptr_t virt;
    uintptr_t phys;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: virt <hexaddr>");
        return;
    }

    virt = (uintptr_t)parse_hex_u32(arg, &ok);
    if (!ok) {
        klog_warn("endereco invalido");
        return;
    }

    phys = vmm_translate(virt);

    vga_puts("virt=");
    vga_puthex((uint32_t)virt);
    vga_puts(" phys=");
    if (phys == 0U) {
        vga_puts("UNMAPPED");
    } else {
        vga_puthex((uint32_t)phys);
    }
    vga_puts("\n");
}

static void shell_cmd_mapped(const char *arg) {
    int ok;
    uintptr_t virt;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: mapped <hexaddr>");
        return;
    }

    virt = (uintptr_t)parse_hex_u32(arg, &ok);
    if (!ok) {
        klog_warn("endereco invalido");
        return;
    }

    vga_puts("virt=");
    vga_puthex((uint32_t)virt);
    vga_puts(" mapped=");
    vga_puts(vmm_is_mapped(virt) ? "YES" : "NO");
    vga_puts("\n");
}

static void shell_cmd_unmap(const char *arg) {
    int ok;
    uintptr_t virt;
    uintptr_t page;
    int rc;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: unmap <hexaddr>");
        return;
    }

    virt = (uintptr_t)parse_hex_u32(arg, &ok);
    if (!ok) {
        klog_warn("endereco invalido");
        return;
    }

    page = virt & 0xFFFFF000U;
    rc = vmm_unmap_page(page);
    if (rc != 0) {
        klog_warn("unmap falhou");
        return;
    }

    vga_puts("unmapped ");
    vga_puthex((uint32_t)page);
    vga_puts("\n");
}

static void shell_cmd_kslots(void) {
    int found = 0;

    for (uint32_t i = 0; i < KHEAP_SLOTS; ++i) {
        if (heap_slots[i] != 0) {
            found = 1;
            vga_puts("slot=");
            vga_putdec(i);
            vga_puts(" ptr=");
            vga_puthex((uint32_t)(uintptr_t)heap_slots[i]);
            vga_puts(" size=");
            vga_putdec(heap_slot_sizes[i]);
            vga_puts("\n");
        }
    }

    if (!found) {
        vga_puts("(sem slots ocupados)\n");
    }
}

static void shell_cmd_kmalloc_slot(const char *arg) {
    int ok;
    uint32_t sz;
    int slot;
    uint32_t *ptr;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: kmalloc <bytes>");
        return;
    }

    sz = parse_u32(arg, &ok);
    if (!ok || sz == 0) {
        klog_warn("usage: kmalloc <bytes>");
        return;
    }

    slot = heap_find_free_slot();
    if (slot < 0) {
        klog_warn("sem slots livres");
        return;
    }

    ptr = (uint32_t *)kmalloc(sz);
    if (ptr == 0) {
        klog_warn("kmalloc failed");
        return;
    }

    *ptr = 0xCAFEBABEU;
    heap_slots[slot] = ptr;
    heap_slot_sizes[slot] = sz;

    vga_puts("kmalloc slot=");
    vga_putdec((uint32_t)slot);
    vga_puts(" ptr=");
    vga_puthex((uint32_t)(uintptr_t)ptr);
    vga_puts(" size=");
    vga_putdec(sz);
    vga_puts(" test=");
    vga_puthex(*ptr);
    vga_puts("\n");
}

static void shell_cmd_kfree_slot(const char *arg) {
    int ok;
    uint32_t slot;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: kfree <slot>");
        return;
    }

    slot = parse_u32(arg, &ok);
    if (!ok || slot >= KHEAP_SLOTS) {
        klog_warn("slot invalido");
        return;
    }

    if (!heap_slot_valid(slot)) {
        klog_warn("slot vazio");
        return;
    }

    kfree(heap_slots[slot]);
    heap_slots[slot] = 0;
    heap_slot_sizes[slot] = 0;

    vga_puts("kfree slot=");
    vga_putdec(slot);
    vga_puts("\n");
}

static void shell_cmd_krealloc_slot(const char *arg) {
    int ok_slot;
    int ok_size;
    uint32_t slot;
    uint32_t new_size;
    uint32_t i = 0;
    uint32_t first_end;
    void *new_ptr;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: krealloc <slot> <bytes>");
        return;
    }

    while (arg[i] != 0 && arg[i] != ' ') {
        i++;
    }

    first_end = i;

    if (arg[i] == 0) {
        klog_warn("usage: krealloc <slot> <bytes>");
        return;
    }

    {
        char slot_buf[16];
        str_copy_range(slot_buf, sizeof(slot_buf), arg, 0, first_end);
        slot = parse_u32(slot_buf, &ok_slot);
    }

    while (arg[i] == ' ') {
        i++;
    }

    new_size = parse_u32(arg + i, &ok_size);

    if (!ok_slot || !ok_size || slot >= KHEAP_SLOTS || new_size == 0) {
        klog_warn("usage: krealloc <slot> <bytes>");
        return;
    }

    if (!heap_slot_valid(slot)) {
        klog_warn("slot vazio");
        return;
    }

    new_ptr = krealloc(heap_slots[slot], new_size);
    if (new_ptr == 0) {
        klog_warn("krealloc failed");
        return;
    }

    heap_slots[slot] = new_ptr;
    heap_slot_sizes[slot] = new_size;

    vga_puts("krealloc slot=");
    vga_putdec(slot);
    vga_puts(" ptr=");
    vga_puthex((uint32_t)(uintptr_t)new_ptr);
    vga_puts(" size=");
    vga_putdec(new_size);
    vga_puts("\n");
}

static void shell_cmd_tmptest(void) {
    uint32_t src_frame;
    uint32_t dst_frame;
    uint8_t pattern[32];
    uint8_t readback[32];
    uint8_t zero_check[32];
    uint8_t ok = 1U;

    if (!vmm_temp_window_is_ready()) {
        klog_warn("temporary mapping window unavailable");
        return;
    }

    src_frame = pmm_alloc_frame_above(0x01000000U);
    if (src_frame == 0U) {
        klog_warn("no free frame above 16 MiB");
        return;
    }

    dst_frame = pmm_alloc_frame_above(0x01000000U);
    if (dst_frame == 0U) {
        pmm_free_frame(src_frame);
        klog_warn("second high frame unavailable");
        return;
    }

    for (uint32_t i = 0U; i < sizeof(pattern); ++i) {
        pattern[i] = (uint8_t)(0x31U + (i * 7U));
        readback[i] = 0U;
        zero_check[i] = 0xFFU;
    }

    if (vmm_copy_to_phys(
            (uintptr_t)src_frame + 0x123U,
            pattern,
            sizeof(pattern)) != 0 ||
        vmm_copy_phys(
            (uintptr_t)dst_frame + 0x2A5U,
            (uintptr_t)src_frame + 0x123U,
            sizeof(pattern)) != 0 ||
        vmm_copy_from_phys(
            readback,
            (uintptr_t)dst_frame + 0x2A5U,
            sizeof(readback)) != 0) {
        ok = 0U;
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(pattern); ++i) {
            if (readback[i] != pattern[i]) {
                ok = 0U;
                break;
            }
        }
    }

    if (ok &&
        vmm_zero_phys(
            (uintptr_t)dst_frame + 0x2A5U,
            sizeof(zero_check)) == 0 &&
        vmm_copy_from_phys(
            zero_check,
            (uintptr_t)dst_frame + 0x2A5U,
            sizeof(zero_check)) == 0) {
        for (uint32_t i = 0U; i < sizeof(zero_check); ++i) {
            if (zero_check[i] != 0U) {
                ok = 0U;
                break;
            }
        }
    } else {
        ok = 0U;
    }

    vga_puts("tmptest: src=");
    vga_puthex(src_frame);
    vga_puts(" dst=");
    vga_puthex(dst_frame);
    vga_puts(" result=");
    vga_puts(ok ? "HIGH-FRAME OK" : "FAILED");
    vga_puts("\n");

    pmm_free_frame(dst_frame);
    pmm_free_frame(src_frame);
}

static void shell_cmd_mounttest(void) {
    fs_node_t *source;
    fs_node_t *target;
    fs_node_t *probe;
    fs_node_t *mounted_root;
    fs_node_t *mounted_probe;
    uint32_t baseline_mounts = vfs_mount_count();
    uint8_t ok = 1U;

    source = vfs_resolve("/tmp");
    if (source == 0) {
        klog_warn("mounttest: /tmp unavailable");
        return;
    }

    target = vfs_create("/mnttest", FS_DIRECTORY);
    if (target == 0) {
        klog_warn("mounttest: failed to create /mnttest");
        return;
    }

    probe = vfs_create(
        "/tmp/__mount_probe",
        FS_FILE | FS_WRITABLE
    );
    if (probe == 0) {
        (void)vfs_remove("/mnttest");
        klog_warn("mounttest: failed to create probe");
        return;
    }

    if (vfs_mount("/mnttest", source) != 0) {
        (void)vfs_remove("/tmp/__mount_probe");
        (void)vfs_remove("/mnttest");
        klog_warn("mounttest: mount failed");
        return;
    }

    mounted_root = vfs_resolve("/mnttest");
    mounted_probe =
        vfs_resolve("/mnttest/__mount_probe");

    if (mounted_root != source ||
        mounted_probe != probe ||
        vfs_mount_count() != baseline_mounts + 1U) {
        ok = 0U;
    }

    if (vfs_unmount("/mnttest") != 0) {
        ok = 0U;
    }

    if (vfs_resolve("/mnttest") != target ||
        vfs_resolve("/mnttest/__mount_probe") != 0 ||
        vfs_mount_count() != baseline_mounts) {
        ok = 0U;
    }

    if (vfs_remove("/tmp/__mount_probe") != 0 ||
        vfs_remove("/mnttest") != 0) {
        ok = 0U;
    }

    if (vfs_resolve("/tmp/__mount_probe") != 0 ||
        vfs_resolve("/mnttest") != 0) {
        ok = 0U;
    }

    vga_puts("mounttest: result=");
    vga_puts(ok ? "MOUNT OK" : "FAILED");
    vga_puts("\n");
}

static void shell_cmd_devtest(void) {
    fs_node_t *null_node = vfs_resolve("/dev/null");
    fs_node_t *zero_node = vfs_resolve("/dev/zero");
    uint8_t zero_buf[32];
    static const uint8_t payload[] = "cruzeiro";
    uint8_t ok = 1U;

    if (null_node == 0 || zero_node == 0) {
        ok = 0U;
    }

    if (ok &&
        read_fs(
            null_node,
            0U,
            sizeof(zero_buf),
            zero_buf) != 0U) {
        ok = 0U;
    }

    if (ok &&
        write_fs(
            null_node,
            0U,
            (uint32_t)(sizeof(payload) - 1U),
            payload) !=
            (uint32_t)(sizeof(payload) - 1U)) {
        ok = 0U;
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(zero_buf); ++i) {
            zero_buf[i] = 0xA5U;
        }

        if (read_fs(
                zero_node,
                0U,
                sizeof(zero_buf),
                zero_buf) != sizeof(zero_buf)) {
            ok = 0U;
        }
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(zero_buf); ++i) {
            if (zero_buf[i] != 0U) {
                ok = 0U;
                break;
            }
        }
    }

    vga_puts("devtest: result=");
    vga_puts(ok ? "DEVFS OK" : "FAILED");
    vga_puts("\n");
}

static void shell_cmd_ttytest(void) {
    fs_node_t *tty = vfs_resolve("/dev/tty1");
    static const uint8_t sample[] = {'t', 't', 'y', '1', '\n'};
    static const uint8_t output[] = "[tty1] output path OK\n";
    uint8_t readback[sizeof(sample)];
    uint8_t ok = 1U;

    if (tty == 0) {
        klog_warn("ttytest: /dev/tty1 unavailable");
        return;
    }

    tty1_flush_input();

    for (uint32_t i = 0U; i < sizeof(sample); ++i) {
        tty1_receive_char((char)sample[i]);
    }

    if (tty1_pending() != sizeof(sample)) {
        ok = 0U;
    }

    if (ok &&
        read_fs(
            tty,
            0U,
            sizeof(readback),
            readback) != sizeof(readback)) {
        ok = 0U;
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(sample); ++i) {
            if (readback[i] != sample[i]) {
                ok = 0U;
                break;
            }
        }
    }

    if (tty1_pending() != 0U) {
        ok = 0U;
    }

    if (write_fs(
            tty,
            0U,
            (uint32_t)sizeof(output) - 1U,
            output) !=
        (uint32_t)sizeof(output) - 1U) {
        ok = 0U;
    }

    vga_puts("ttytest: result=");
    vga_puts(ok ? "TTY1 OK" : "FAILED");
    vga_puts(" dropped=");
    vga_putdec(tty1_dropped());
    vga_puts("\n");
}

static void shell_cmd_stdiotest(void) {
    process_t *process = sched_current_process_ptr();
    fs_node_t *tty = vfs_resolve("/dev/tty1");
    uint8_t ok = 1U;

    if (process == 0 || tty == 0) {
        ok = 0U;
    }

    if (ok) {
        fd_entry_t *stdin_entry = &process->fds.entries[0];
        fd_entry_t *stdout_entry = &process->fds.entries[1];
        fd_entry_t *stderr_entry = &process->fds.entries[2];

        if (stdin_entry->kind != FD_KIND_VFS ||
            stdout_entry->kind != FD_KIND_VFS ||
            stderr_entry->kind != FD_KIND_VFS ||
            stdin_entry->node != tty ||
            stdout_entry->node != tty ||
            stderr_entry->node != tty ||
            stdin_entry->access != FD_ACCESS_READ ||
            stdout_entry->access != FD_ACCESS_WRITE ||
            stderr_entry->access != FD_ACCESS_WRITE) {
            ok = 0U;
        }
    }

    if (ok) {
        uint32_t new_offset = 0U;

        if (fd_seek(
                &process->fds,
                1U,
                0,
                0U,
                &new_offset) != -2) {
            ok = 0U;
        }
    }

    vga_puts("stdiotest: result=");
    vga_puts(ok ? "STDIO->TTY1 OK" : "FAILED");
    vga_puts("\n");
}

static void shell_run_command(const char *cmd) {
    if (str_eq(cmd, "help")) {
        vga_puts("cmds: help clear ticks task ps pmm vmm tmptest mounttest devtest ttytest stdiotest wp nullguard pfault kmalloc kfree krealloc kslots kheap kheapcheck ls mkdir cat touch echo panic shutdown arch virt mapped unmap schedtest tss syscalltest ring3test ring3fault ring3ud ring3gp ring3as lastexit waittest\n");
        vga_puts("write: echo <texto> > <arquivo> | cat > <arquivo> <texto>\n");
        vga_puts("panic modes: panic int3 | panic ud2 | panic div0(disabled) | panic null | panic int <n>\n");
        vga_puts("vmm dbg: virt <hex> | mapped <hex> | unmap <hex>\n");
        vga_puts("heap dbg: kmalloc <bytes> | kfree <slot> | krealloc <slot> <bytes> | kslots | kheapcheck\n");
    } else if (str_eq(cmd, "arch")) {
        if (sizeof(void*) == 8) {
            vga_puts("architecture: x86_64\n");
        } else {
            vga_puts("architecture: x86_32\n");
        }
    } else if (str_eq(cmd, "clear")) {
        vga_clear();
    } else if (str_eq(cmd, "ticks")) {
        vga_puts("ticks=");
        vga_putdec(irq_timer_ticks());
        vga_puts(" secs=");
        vga_putdec(irq_timer_seconds());
        vga_puts(" hz=");
        vga_putdec(irq_timer_hz());
        vga_puts("\n");
    } else if (str_eq(cmd, "task")) {
        vga_puts("current tid=");
        vga_putdec(sched_current_task());

        vga_puts(" pid=");
        vga_putdec(sched_current_pid());

        vga_puts(" tasks=");
        vga_putdec(sched_task_count());

        vga_puts(" switches=");
        vga_putdec(sched_switch_count());

        vga_puts(" demoA=\n");
        vga_putdec(sched_demo_counter_a());

        vga_puts(" demoB=");
        vga_putdec(sched_demo_counter_b());

        vga_puts(" sseA=");
        vga_putdec((uint32_t)sched_sse_value_a());

        vga_puts(" sseB=");
        vga_putdec((uint32_t)sched_sse_value_b());

        vga_puts(" sleep=");
        vga_putdec(sched_sleep_demo_counter());

        vga_puts(" evA=");
        vga_putdec(sched_event_a_counter());

        vga_puts(" evB=");
        vga_putdec(sched_event_b_counter());

        vga_puts(" wake=");
        vga_putdec(sched_event_waker_counter());

        vga_puts("\n");
    } else if (str_eq(cmd, "pmm")) {
        vga_puts("frames total=");
        vga_putdec(pmm_total_frame_count());
        vga_puts(" free=");
        vga_putdec(pmm_free_frame_count());
        vga_puts("\n");
    } else if (str_eq(cmd, "vmm")) {
        vga_puts("paging=");
        vga_puts(vmm_is_enabled() ? "ON" : "OFF");
        vga_puts(" wp=");
        vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
        vga_puts(" cr3=");
        vga_puthex(vmm_current_cr3());
        vga_puts(" kernel=");
        vga_puthex(vmm_kernel_cr3());
        vga_puts(" spaces=");
        vga_putdec(vmm_address_space_count());
        vga_puts(" temp=");
        vga_puts(vmm_temp_window_is_ready() ? "ON" : "OFF");
        vga_puts("\n");
    } else if (str_eq(cmd, "tmptest")) {
        shell_cmd_tmptest();
    } else if (str_eq(cmd, "mounttest")) {
        shell_cmd_mounttest();
    } else if (str_eq(cmd, "devtest")) {
        shell_cmd_devtest();
    } else if (str_eq(cmd, "ttytest")) {
        shell_cmd_ttytest();
    } else if (str_eq(cmd, "stdiotest")) {
        shell_cmd_stdiotest();
    } else if (str_eq(cmd, "wp")) {
        vga_puts("CR0.WP=");
        vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
        vga_puts("\n");
    } else if (str_eq(cmd, "nullguard")) {
        vga_puts("null-page guard ativo (0x0 sem mapeamento). Teste com: panic null\n");
    } else if (str_eq(cmd, "kheap")) {
        vga_puts("kheap used=");
        vga_putdec(kmalloc_bytes_used());
        vga_puts(" free=");
        vga_putdec(kmalloc_bytes_free());
        vga_puts(" mapped=");
        vga_putdec(kmalloc_bytes_mapped());
        vga_puts(" blocks=");
        vga_putdec(kmalloc_block_count());
        vga_puts("\n");
    } else if (str_eq(cmd, "kslots")) {
        shell_cmd_kslots();
    } else if (str_starts(cmd, "kmalloc ")) {
        shell_cmd_kmalloc_slot(skip_spaces(cmd + 8));
    } else if (str_starts(cmd, "kfree ")) {
        shell_cmd_kfree_slot(skip_spaces(cmd + 6));
    } else if (str_starts(cmd, "krealloc ")) {
        shell_cmd_krealloc_slot(skip_spaces(cmd + 9));
    } else if (str_eq(cmd, "ls")) {
        shell_cmd_ls("/");
    } else if (str_starts(cmd, "ls ")) {
        shell_cmd_ls(skip_spaces(cmd + 3));
    } else if (str_starts(cmd, "mkdir ")) {
        shell_cmd_mkdir(skip_spaces(cmd + 6));
    } else if (str_starts(cmd, "touch ")) {
        shell_cmd_touch(cmd + 6);
    } else if (str_starts(cmd, "cat > ")) {
        uint32_t i = 6;
        uint32_t file_start = i;
        char name[128];

        while (cmd[i] != 0 && cmd[i] != ' ') {
            i++;
        }

        str_copy_range(name, sizeof(name), cmd, file_start, i);

        if (cmd[i] == ' ') {
            i++;
        }

        shell_write_text_file(name, cmd + i);
    } else if (str_starts(cmd, "cat ")) {
        shell_cmd_cat(cmd + 4);
    } else if (str_starts(cmd, "echo ")) {
        int sep = str_find(cmd + 5, " > ");

        if (sep >= 0) {
            char text[128];
            char file[128];
            uint32_t base = 5U;
            uint32_t split = base + (uint32_t)sep;
            uint32_t total = str_len(cmd);

            str_copy_range(text, sizeof(text), cmd, base, split);
            str_copy_range(file, sizeof(file), cmd, split + 3U, total);
            shell_write_text_file(file, text);
        } else {
            vga_puts(cmd + 5);
            vga_puts("\n");
        }
    } else if (str_starts(cmd, "virt ")) {
        shell_cmd_virt(skip_spaces(cmd + 5));
    } else if (str_starts(cmd, "mapped ")) {
        shell_cmd_mapped(skip_spaces(cmd + 7));
    } else if (str_starts(cmd, "unmap ")) {
        shell_cmd_unmap(skip_spaces(cmd + 6));
    } else if (str_eq(cmd, "panic") || str_starts(cmd, "panic ")) {
        shell_cmd_panic(cmd[5] ? cmd + 6 : 0);
    } else if (str_eq(cmd, "pfault") || str_starts(cmd, "pfault ")) {
        shell_cmd_pfault(cmd[6] ? skip_spaces(cmd + 7) : 0);
    } else if (str_eq(cmd, "kheapcheck")) {
        vga_puts("kheapcheck=");
        vga_puts(kheap_check() ? "OK" : "CORRUPT");
        vga_puts("\n");
    } else if (str_eq(cmd, "ps")) {
        task_list_tasks();
    } else if (str_eq(cmd, "shutdown")) {
        vga_puts("Shutting down...\n");

        /* Signal QEMU to shutdown */
        asm volatile (
            "outw %0, %1"
            :
            : "a"((uint16_t)0x2000),
              "Nd"((uint16_t)0x604)
        );

        asm volatile ("cli");

        for (;;) {
            asm volatile ("hlt");
        }
    } else if (str_eq(cmd, "schedtest")) {
        sched_demo_init();
        vga_puts("scheduler tests started\n");
        return;
    } else if (str_eq(cmd, "tss")) {
        vga_puts("TR=");
        vga_puthex((uint32_t)tss_get_selector());
        vga_puts("\n");
    } else if (str_eq(cmd, "syscalltest")) {
        uint32_t ret = syscall_test_write();

        vga_puts("return=");
        vga_putdec(ret);
        vga_puts("\n");

#ifndef __x86_64__
    } else if (str_eq(cmd, "ring3test")) {
        int pid;

        pid = sched_create_user_task(
            "ring3-test",
            user_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 task");
            return;
        }

        vga_puts("Ring 3 task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    } else if (str_eq(cmd, "ring3fault")) {
        int pid;

        pid = sched_create_user_task(
            "ring3-fault",
            user_fault_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 fault task");
            return;
        }

        vga_puts("Ring 3 fault task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    } else if (str_eq(cmd, "ring3ud")) {
        int pid = sched_create_user_task(
            "ring3-ud",
            user_ud_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 UD task");
            return;
        }

        vga_puts("Ring 3 UD task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    } else if (str_eq(cmd, "ring3gp")) {
        int pid = sched_create_user_task(
            "ring3-gp",
            user_gp_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (pid < 0) {
            klog_warn("failed to create Ring 3 GP task");
            return;
        }

        vga_puts("Ring 3 GP task created pid=");
        vga_putdec((uint32_t)pid);
        vga_puts("\n");
    } else if (str_eq(cmd, "ring3as")) {
        int first = sched_create_user_task(
            "ring3-as-a",
            user_aspace_test_entry,
            (uintptr_t)&user_stack_top
        );
        int second = sched_create_user_task(
            "ring3-as-b",
            user_aspace_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (first < 0 || second < 0) {
            klog_warn("failed to create address-space test tasks");
            return;
        }

        vga_puts("Ring 3 address-space tasks created pids=");
        vga_putdec((uint32_t)first);
        vga_puts(",");
        vga_putdec((uint32_t)second);
        vga_puts("\n");
#endif
    } else if (str_eq(cmd, "lastexit")) {
        vga_puts("last exit: pid=");
        vga_putdec(sched_last_exit_pid());

        vga_puts(" code=");
        vga_putdec((uint32_t)sched_last_exit_code());

        vga_puts("\n");    
    } else if (str_eq(cmd, "waittest")) {
    int32_t status = 0;
    int32_t pid = task_wait_child(&status);

    if (pid < 0) {
        vga_puts("wait: no zombie child\n");
    } else {
        vga_puts("wait: pid=");
        vga_putdec((uint32_t)pid);

        vga_puts(" code=");
        vga_putdec((uint32_t)status);

        vga_puts("\n");
    } } else if (cmd[0] != 0) {
        klog_warn("unknown command");
    }
}

void shell_init(void) {
    line_len = 0;

    for (uint32_t i = 0; i < KHEAP_SLOTS; ++i) {
        heap_slots[i] = 0;
        heap_slot_sizes[i] = 0;
    }

    klog_info("shell ready (type 'help')");
    shell_prompt();
}

void shell_on_key(char c) {
    if (c == '\r') {
        return;
    }

    if (c == '\n') {
        line[line_len] = 0;
        shell_run_command(line);
        line_len = 0;
        shell_prompt();
        return;
    }

    if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            uint16_t p = vga_get_cursor_pos();

            if (p > 0) {
                vga_set_serial_mirror(0);
                vga_set_cursor_pos(p - 1);
                vga_putc(' ');
                vga_set_cursor_pos(p - 1);
                vga_set_serial_mirror(1);
            }

            if (serial_is_available()) {
                serial_puts("\b \b");
            }
        }
        return;
    }

    if (line_len < (SHELL_BUF - 1)) {
        line[line_len++] = c;
        vga_putc(c);
    }
}
