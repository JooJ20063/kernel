#include <kernel/shell.h>
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>
#include <kernel/sched.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include <kernel/ramfs.h>

#ifdef __x86_64__
#include <arch/x86_64/irq.h>
#include <arch/x86_64/regs.h>
#else
#include <arch/x86/irq.h>
#include <arch/x86/regs.h>
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

static void shell_cmd_ls(void) {
    fs_node_t *root = ramfs_root();
    uint32_t i = 0;

    vga_puts("ramfs entries:\n");

    for (;;) {
        fs_node_t *entry = readdir_fs(root, i);
        if (entry == 0) {
            break;
        }

        vga_puts(" - ");
        vga_puts(entry->name);
        vga_puts(" (");
        vga_putdec(entry->size);
        vga_puts(" bytes)\n");
        i++;
    }

    if (i == 0) {
        vga_puts("(vazio)\n");
    }
}

static void shell_cmd_cat(const char *name) {
    fs_node_t *entry;

    if (name == 0 || *name == 0) {
        klog_warn("usage: cat <arquivo>");
        return;
    }

    entry = ramfs_find(name);
    if (entry == 0) {
        klog_warn("arquivo nao encontrado");
        return;
    }

    {
        uint8_t buf[64];
        uint32_t off = 0;

        while (off < entry->size) {
            uint32_t n = read_fs(entry, off, sizeof(buf), buf);
            if (n == 0) {
                break;
            }

            for (uint32_t j = 0; j < n; ++j) {
                vga_putc((char)buf[j]);
            }

            off += n;
        }
    }

    vga_puts("\n");
}

static void shell_write_text_file(const char *name, const char *text) {
    fs_node_t *entry;
    uint32_t n;

    if (name == 0 || *name == 0) {
        klog_warn("arquivo invalido");
        return;
    }

    entry = ramfs_touch(name);
    if (entry == 0) {
        klog_warn("falha ao criar/abrir arquivo");
        return;
    }

    n = write_fs(entry, 0, str_len(text), (const uint8_t *)text);
    if (n != str_len(text)) {
        klog_warn("arquivo somente leitura ou sem memoria");
        return;
    }

    entry->size = n;
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

    entry = ramfs_touch(name);
    if (entry == 0) {
        klog_warn("touch falhou");
        return;
    }

    vga_puts("touch: ");
    vga_puts(entry->name);
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

static void shell_run_command(const char *cmd) {
    if (str_eq(cmd, "help")) {
        vga_puts("cmds: help clear ticks task pmm vmm wp nullguard kmalloc kfree krealloc kslots kheap kheapcheck ls cat touch echo panic shutdown arch virt mapped unmap\n");
        vga_puts("write: echo <texto> > <arquivo> | cat > <arquivo> <texto>\n");
        vga_puts("panic modes: panic int3 | panic ud2 | panic div0 | panic null | panic int <n>\n");
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
        vga_puts("task=");
        vga_putdec(sched_current_task());
        vga_puts(" switches=");
        vga_putdec(sched_switch_count());
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
        vga_puts("\n");
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
        shell_cmd_ls();
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
    } 
      else if (str_eq(cmd, "kheapcheck")) {
    vga_puts("kheapcheck=");
    vga_puts(kheap_check() ? "OK" : "CORRUPT");
    vga_puts("\n"); 
    }
      else if (str_eq(cmd, "shutdown")) {
        vga_puts("Shutting down...\n");
        /* Signal QEMU to shutdown */
        asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
        asm volatile ("cli");
        for (;;) {
            asm volatile ("hlt");
        }
    }
      else if (cmd[0] != 0) {
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
                vga_set_cursor_pos(p - 1);
                vga_putc(' ');
                vga_set_cursor_pos(p - 1);
            }
        }
        return;
    }

    if (line_len < (SHELL_BUF - 1)) {
        line[line_len++] = c;
        vga_putc(c);
    }
}
