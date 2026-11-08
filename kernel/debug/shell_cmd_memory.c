#include "shell_internal.h"
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>

#define KHEAP_SLOTS 16

static void *heap_slots[KHEAP_SLOTS];
static uint32_t heap_slot_sizes[KHEAP_SLOTS];

void shell_memory_init(void) {
    for (uint32_t i = 0; i < KHEAP_SLOTS; ++i) {
        heap_slots[i] = 0;
        heap_slot_sizes[i] = 0;
    }
}

/* Localiza o primeiro slot livre na tabela de acompanhamento de alocações do Heap */
static int heap_find_free_slot(void) {
    for (int i = 0; i < KHEAP_SLOTS; ++i) {
        if (heap_slots[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* Valida se um determinado índice de slot do Heap contém uma alocação ativa */
static int heap_slot_valid(uint32_t slot) {
    return (slot < KHEAP_SLOTS && heap_slots[slot] != 0);
}

void shell_cmd_virt(const char *arg) {
    int ok;
    uintptr_t virt;
    uintptr_t phys;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: virt <hexaddr>");
        return;
    }

    virt = (uintptr_t)shell_parse_hex_u32(arg, &ok);
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

void shell_cmd_mapped(const char *arg) {
    int ok;
    uintptr_t virt;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: mapped <hexaddr>");
        return;
    }

    virt = (uintptr_t)shell_parse_hex_u32(arg, &ok);
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

void shell_cmd_unmap(const char *arg) {
    int ok;
    uintptr_t virt;
    uintptr_t page;
    int rc;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: unmap <hexaddr>");
        return;
    }

    virt = (uintptr_t)shell_parse_hex_u32(arg, &ok);
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

void shell_cmd_kslots(void) {
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

void shell_cmd_kmalloc_slot(const char *arg) {
    int ok;
    uint32_t sz;
    int slot;
    uint32_t *ptr;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: kmalloc <bytes>");
        return;
    }

    sz = shell_parse_u32(arg, &ok);
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

void shell_cmd_kfree_slot(const char *arg) {
    int ok;
    uint32_t slot;

    if (arg == 0 || *arg == 0) {
        klog_warn("usage: kfree <slot>");
        return;
    }

    slot = shell_parse_u32(arg, &ok);
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

void shell_cmd_krealloc_slot(const char *arg) {
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
        shell_str_copy_range(slot_buf, sizeof(slot_buf), arg, 0, first_end);
        slot = shell_parse_u32(slot_buf, &ok_slot);
    }

    while (arg[i] == ' ') {
        i++;
    }

    new_size = shell_parse_u32(arg + i, &ok_size);

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

void shell_cmd_tmptest(void) {
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

    if (vmm_copy_to_phys((uintptr_t)src_frame + 0x123U, pattern, sizeof(pattern)) != 0 ||
        vmm_copy_phys((uintptr_t)dst_frame + 0x2A5U, (uintptr_t)src_frame + 0x123U, sizeof(pattern)) != 0 ||
        vmm_copy_from_phys(readback, (uintptr_t)dst_frame + 0x2A5U, sizeof(readback)) != 0) {
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
        vmm_zero_phys((uintptr_t)dst_frame + 0x2A5U, sizeof(zero_check)) == 0 &&
        vmm_copy_from_phys(zero_check, (uintptr_t)dst_frame + 0x2A5U, sizeof(zero_check)) == 0) {
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
