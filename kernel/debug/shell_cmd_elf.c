#include "shell_internal.h"
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/vfs.h>
#include <kernel/elf32.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/tty.h>

void shell_cmd_elftest(const char *path) {
    const char *target = (path == 0 || *path == 0) ? "/bin/hello" : path;
    fs_node_t *node = vfs_resolve(target);
    elf32_ehdr_t header;
    elf32_image_info_t image;
    int status;

    if (node == 0) {
        klog_warn("elftest: file not found");
        return;
    }

    status = elf32_inspect(node, &header, &image);
    if (status != ELF32_OK) {
        vga_puts("elftest: FAILED status=");
        vga_putdec((uint32_t)(-status));
        vga_puts(" reason=");
        vga_puts(elf32_status_string(status));
        vga_puts("\n");
        return;
    }

    vga_puts("elftest: ");
    vga_puts(target);
    vga_puts(" ELF32/i386 OK\n");
    vga_puts(" entry=");
    vga_puthex(image.entry);
    vga_puts(" load_segments=");
    vga_putdec(image.load_segment_count);
    vga_puts(" range=");
    vga_puthex(image.lowest_vaddr);
    vga_puts("..");
    vga_puthex(image.highest_vaddr);
    vga_puts("\n");

    for (uint32_t i = 0U; i < (uint32_t)header.e_phnum; ++i) {
        elf32_phdr_t ph;

        status = elf32_read_program_header(node, &header, i, &ph);
        if (status != ELF32_OK) {
            vga_puts(" phdr read failed at index=");
            vga_putdec(i);
            vga_puts("\n");
            return;
        }

        if (ph.p_type != ELF32_PT_LOAD) {
            continue;
        }

        vga_puts(" PT_LOAD[");
        vga_putdec(i);
        vga_puts("] off=");
        vga_puthex(ph.p_offset);
        vga_puts(" vaddr=");
        vga_puthex(ph.p_vaddr);
        vga_puts(" filesz=");
        vga_puthex(ph.p_filesz);
        vga_puts(" memsz=");
        vga_puthex(ph.p_memsz);
        vga_puts(" flags=");
        vga_puthex(ph.p_flags);
        vga_puts(" align=");
        vga_puthex(ph.p_align);
        vga_puts("\n");
    }
}

static uint32_t shell_min_u32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

static int shell_verify_loaded_elf(fs_node_t *node, const elf32_loaded_image_t *loaded) {
    elf32_ehdr_t header;
    uint8_t expected[64];
    uint8_t actual[64];
    int status;

    if (node == 0 || loaded == 0 || loaded->cr3 == 0U) {
        return 0;
    }

    status = elf32_read_header(node, &header);
    if (status != ELF32_OK) {
        return 0;
    }

    for (uint32_t i = 0U; i < (uint32_t)header.e_phnum; ++i) {
        elf32_phdr_t ph;
        uint32_t remaining;
        uint32_t file_offset;
        uint32_t virt;

        status = elf32_read_program_header(node, &header, i, &ph);
        if (status != ELF32_OK) {
            return 0;
        }

        if (ph.p_type != ELF32_PT_LOAD || ph.p_memsz == 0U) {
            continue;
        }

        remaining = ph.p_filesz;
        file_offset = ph.p_offset;
        virt = ph.p_vaddr;

        while (remaining != 0U) {
            uint32_t page_remaining = VMM_PAGE_SIZE - (virt & (VMM_PAGE_SIZE - 1U));
            uint32_t chunk = shell_min_u32(remaining, shell_min_u32(page_remaining, (uint32_t)sizeof(expected)));
            uintptr_t phys = vmm_translate_address_space(loaded->cr3, virt);

            if (phys == 0U ||
                read_fs(node, file_offset, chunk, expected) != chunk ||
                vmm_copy_from_phys(actual, phys, chunk) != 0) {
                return 0;
            }

            for (uint32_t j = 0U; j < chunk; ++j) {
                if (actual[j] != expected[j]) {
                    return 0;
                }
            }

            file_offset += chunk;
            virt += chunk;
            remaining -= chunk;
        }

        remaining = ph.p_memsz - ph.p_filesz;
        virt = ph.p_vaddr + ph.p_filesz;

        while (remaining != 0U) {
            uint32_t page_remaining = VMM_PAGE_SIZE - (virt & (VMM_PAGE_SIZE - 1U));
            uint32_t chunk = shell_min_u32(remaining, shell_min_u32(page_remaining, (uint32_t)sizeof(actual)));
            uintptr_t phys = vmm_translate_address_space(loaded->cr3, virt);

            if (phys == 0U || vmm_copy_from_phys(actual, phys, chunk) != 0) {
                return 0;
            }

            for (uint32_t j = 0U; j < chunk; ++j) {
                if (actual[j] != 0U) {
                    return 0;
                }
            }

            virt += chunk;
            remaining -= chunk;
        }
    }

    return 1;
}

void shell_cmd_elfloadtest(const char *path) {
    const char *target = (path == 0 || *path == 0) ? "/bin/hello" : path;
    fs_node_t *node = vfs_resolve(target);
    elf32_loaded_image_t loaded = {0};
    uint32_t frames_before;
    uint32_t spaces_before;
    uint32_t entry_flags = 0U;
    uintptr_t entry_phys;
    uint8_t content_ok;
    uint8_t cleanup_ok;
    int status;

    if (node == 0) {
        klog_warn("elfloadtest: file not found");
        return;
    }

    frames_before = pmm_free_frame_count();
    spaces_before = vmm_address_space_count();

    status = elf32_load_image(node, &loaded);
    if (status != ELF32_OK) {
        vga_puts("elfloadtest: FAILED status=");
        vga_putdec((uint32_t)(-status));
        vga_puts(" reason=");
        vga_puts(elf32_status_string(status));
        vga_puts("\n");
        return;
    }

    entry_phys = vmm_translate_address_space(loaded.cr3, loaded.entry);

    content_ok = (uint8_t)(
        entry_phys != 0U &&
        vmm_get_page_flags_address_space(loaded.cr3, loaded.entry, &entry_flags) == 0 &&
        (entry_flags & VMM_PAGE_USER) != 0U &&
        shell_verify_loaded_elf(node, &loaded)
    );

    vga_puts("elfloadtest: ");
    vga_puts(target);
    vga_puts(content_ok ? " LOAD OK\n" : " VERIFY FAILED\n");
    vga_puts(" cr3=");
    vga_puthex(loaded.cr3);
    vga_puts(" entry=");
    vga_puthex(loaded.entry);
    vga_puts(" entry_phys=");
    vga_puthex((uint32_t)entry_phys);
    vga_puts(" pages=");
    vga_putdec(loaded.mapped_page_count);
    vga_puts(" segments=");
    vga_putdec(loaded.load_segment_count);
    vga_puts("\n");

    elf32_unload_image(&loaded);

    cleanup_ok = (uint8_t)(
        vmm_address_space_count() == spaces_before &&
        pmm_free_frame_count() == frames_before
    );

    vga_puts("elfloadtest: content=");
    vga_puts(content_ok ? "OK" : "FAILED");
    vga_puts(" cleanup=");
    vga_puts(cleanup_ok ? "OK" : "LEAK");
    vga_puts("\n");
}

void shell_cmd_elfrun(const char *path) {
    const char *target = (path == 0 || *path == 0) ? "/bin/hello" : path;
    fs_node_t *node = vfs_resolve(target);
    uint32_t pid = 0U;
    int status;

    if (node == 0) {
        klog_warn("elfrun: file not found");
        return;
    }

    status = elf32_spawn(node, &pid);
    if (status != ELF32_OK) {
        vga_puts("elfrun: FAILED status=");
        vga_putdec((uint32_t)(-status));
        vga_puts(" reason=");
        vga_puts(elf32_status_string(status));
        vga_puts("\n");
        return;
    }

    vga_puts("elfrun: ");
    vga_puts(target);
    vga_puts(" pid=");
    vga_putdec(pid);
    vga_puts(" entry scheduled in Ring 3\n");

    /* Transfere o foco de entrada de teclado para o processo recém-criado */
    tty1_flush_input();
    tty1_set_foreground_pid(pid);
}
