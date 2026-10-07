#include <kernel/elf32.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/task.h>

#define ELF32_ADDR_MAX 0xFFFFFFFFU
#define ELF32_USER_STACK_PAGES 4U
#define ELF32_USER_STACK_TOP VMM_USER_MAX_ADDR

_Static_assert(sizeof(elf32_ehdr_t) == 52U, "ELF32 header size mismatch");
_Static_assert(sizeof(elf32_phdr_t) == 32U, "ELF32 program header size mismatch");

static int elf32_is_power_of_two(uint32_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

static int elf32_read_program_header_unchecked(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    uint32_t index,
    elf32_phdr_t *program_header_out
) {
    uint32_t offset;
    uint32_t bytes_read;

    offset =
        header->e_phoff +
        index * (uint32_t)header->e_phentsize;

    bytes_read = read_fs(
        node,
        offset,
        sizeof(elf32_phdr_t),
        (uint8_t *)program_header_out
    );

    if (bytes_read != sizeof(elf32_phdr_t)) {
        return ELF32_ERR_PHDR_IO;
    }

    return ELF32_OK;
}

static int elf32_validate_load_segment(
    const elf32_phdr_t *program_header,
    uint32_t file_size,
    uint32_t *segment_end_out
) {
    uint32_t segment_end;

    if (program_header->p_filesz > program_header->p_memsz) {
        return ELF32_ERR_SEGMENT_SIZE;
    }

    if (program_header->p_offset > file_size ||
        program_header->p_filesz >
            file_size - program_header->p_offset) {
        return ELF32_ERR_SEGMENT_FILE_BOUNDS;
    }

    if (program_header->p_memsz >
        ELF32_ADDR_MAX - program_header->p_vaddr) {
        return ELF32_ERR_SEGMENT_VADDR_OVERFLOW;
    }

    if (program_header->p_align > 1U) {
        uint32_t align_mask;

        if (!elf32_is_power_of_two(program_header->p_align)) {
            return ELF32_ERR_SEGMENT_ALIGN;
        }

        align_mask = program_header->p_align - 1U;

        if ((program_header->p_vaddr & align_mask) !=
            (program_header->p_offset & align_mask)) {
            return ELF32_ERR_SEGMENT_ALIGN;
        }
    }

    segment_end =
        program_header->p_vaddr + program_header->p_memsz;

    if (segment_end_out != 0) {
        *segment_end_out = segment_end;
    }

    return ELF32_OK;
}

static uint32_t elf32_min_u32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

static int elf32_validate_user_load_range(
    const elf32_phdr_t *program_header,
    uint32_t *page_start_out,
    uint32_t *page_end_out
) {
    uint32_t segment_end;
    uint32_t page_start;
    uint32_t page_end;

    if (program_header == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    if (program_header->p_memsz == 0U) {
        if (page_start_out != 0) {
            *page_start_out = 0U;
        }
        if (page_end_out != 0) {
            *page_end_out = 0U;
        }
        return ELF32_OK;
    }

    segment_end =
        program_header->p_vaddr + program_header->p_memsz;

    if (program_header->p_vaddr < VMM_USER_MIN_ADDR ||
        segment_end > VMM_USER_MAX_ADDR) {
        return ELF32_ERR_SEGMENT_USER_RANGE;
    }

    page_start =
        program_header->p_vaddr &
        ~(uint32_t)(VMM_PAGE_SIZE - 1U);

    page_end =
        (segment_end + VMM_PAGE_SIZE - 1U) &
        ~(uint32_t)(VMM_PAGE_SIZE - 1U);

    if (page_start < VMM_USER_MIN_ADDR ||
        page_end > VMM_USER_MAX_ADDR ||
        page_start >= page_end) {
        return ELF32_ERR_SEGMENT_USER_RANGE;
    }

    if (page_start_out != 0) {
        *page_start_out = page_start;
    }

    if (page_end_out != 0) {
        *page_end_out = page_end;
    }

    return ELF32_OK;
}

static int elf32_map_load_segment_pages(
    uint32_t cr3,
    const elf32_phdr_t *program_header,
    uint32_t *mapped_page_count
) {
    uint32_t page_start;
    uint32_t page_end;
    uint32_t map_flags;
    int status;

    status = elf32_validate_user_load_range(
        program_header,
        &page_start,
        &page_end
    );
    if (status != ELF32_OK) {
        return status;
    }

    if (program_header->p_memsz == 0U) {
        return ELF32_OK;
    }

    map_flags =
        ((program_header->p_flags & ELF32_PF_W) != 0U)
            ? VMM_PAGE_RW
            : 0U;

    for (uint32_t addr = page_start;
         addr < page_end;
         addr += VMM_PAGE_SIZE) {
        uintptr_t existing =
            vmm_translate_address_space(cr3, addr);

        if (existing == 0U) {
            uint32_t frame = pmm_alloc_frame();

            if (frame == 0U) {
                return ELF32_ERR_NO_MEMORY;
            }

            if (vmm_zero_phys(frame, VMM_PAGE_SIZE) != 0) {
                pmm_free_frame(frame);
                return ELF32_ERR_SEGMENT_ZERO;
            }

            if (vmm_map_user_page(
                    cr3,
                    addr,
                    frame,
                    map_flags) != 0) {
                pmm_free_frame(frame);
                return ELF32_ERR_MAP_FAILED;
            }

            if (mapped_page_count != 0) {
                (*mapped_page_count)++;
            }
        } else {
            uint32_t current_flags = 0U;

            if (vmm_get_page_flags_address_space(
                    cr3,
                    addr,
                    &current_flags) != 0) {
                return ELF32_ERR_PAGE_FLAGS;
            }

            if ((current_flags & VMM_PAGE_USER) == 0U) {
                return ELF32_ERR_ADDRESS_CONFLICT;
            }

            if ((map_flags & VMM_PAGE_RW) != 0U &&
                (current_flags & VMM_PAGE_RW) == 0U) {
                if (vmm_set_user_page_flags(
                        cr3,
                        addr,
                        VMM_PAGE_RW) != 0) {
                    return ELF32_ERR_PAGE_FLAGS;
                }
            }
        }
    }

    return ELF32_OK;
}

static int elf32_copy_virtual_range(
    fs_node_t *node,
    uint32_t cr3,
    uint32_t file_offset,
    uint32_t virt_addr,
    uint32_t size
) {
    uint8_t scratch[256];
    uint32_t remaining = size;
    uint32_t source_offset = file_offset;
    uint32_t target = virt_addr;

    while (remaining != 0U) {
        uint32_t page_remaining =
            VMM_PAGE_SIZE -
            (target & (VMM_PAGE_SIZE - 1U));
        uint32_t chunk =
            elf32_min_u32(
                remaining,
                elf32_min_u32(
                    page_remaining,
                    (uint32_t)sizeof(scratch)
                )
            );
        uintptr_t phys =
            vmm_translate_address_space(cr3, target);

        if (phys == 0U) {
            return ELF32_ERR_MAP_FAILED;
        }

        if (read_fs(
                node,
                source_offset,
                chunk,
                scratch) != chunk) {
            return ELF32_ERR_SEGMENT_READ;
        }

        if (vmm_copy_to_phys(
                phys,
                scratch,
                chunk) != 0) {
            return ELF32_ERR_SEGMENT_COPY;
        }

        source_offset += chunk;
        target += chunk;
        remaining -= chunk;
    }

    return ELF32_OK;
}

static int elf32_zero_virtual_range(
    uint32_t cr3,
    uint32_t virt_addr,
    uint32_t size
) {
    uint32_t remaining = size;
    uint32_t target = virt_addr;

    while (remaining != 0U) {
        uint32_t page_remaining =
            VMM_PAGE_SIZE -
            (target & (VMM_PAGE_SIZE - 1U));
        uint32_t chunk =
            elf32_min_u32(remaining, page_remaining);
        uintptr_t phys =
            vmm_translate_address_space(cr3, target);

        if (phys == 0U) {
            return ELF32_ERR_MAP_FAILED;
        }

        if (vmm_zero_phys(phys, chunk) != 0) {
            return ELF32_ERR_SEGMENT_ZERO;
        }

        target += chunk;
        remaining -= chunk;
    }

    return ELF32_OK;
}

static int elf32_populate_load_segment(
    fs_node_t *node,
    uint32_t cr3,
    const elf32_phdr_t *program_header
) {
    int status;

    if (program_header->p_filesz != 0U) {
        status = elf32_copy_virtual_range(
            node,
            cr3,
            program_header->p_offset,
            program_header->p_vaddr,
            program_header->p_filesz
        );
        if (status != ELF32_OK) {
            return status;
        }
    }

    if (program_header->p_memsz > program_header->p_filesz) {
        status = elf32_zero_virtual_range(
            cr3,
            program_header->p_vaddr +
                program_header->p_filesz,
            program_header->p_memsz -
                program_header->p_filesz
        );
        if (status != ELF32_OK) {
            return status;
        }
    }

    return ELF32_OK;
}

int elf32_validate_header(
    const elf32_ehdr_t *header,
    uint32_t file_size
) {
    uint32_t ph_table_bytes;

    if (header == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    if (file_size < sizeof(elf32_ehdr_t)) {
        return ELF32_ERR_TOO_SMALL;
    }

    if (header->e_ident[ELF32_EI_MAG0] != ELF32_MAG0 ||
        header->e_ident[ELF32_EI_MAG1] != ELF32_MAG1 ||
        header->e_ident[ELF32_EI_MAG2] != ELF32_MAG2 ||
        header->e_ident[ELF32_EI_MAG3] != ELF32_MAG3) {
        return ELF32_ERR_MAGIC;
    }

    if (header->e_ident[ELF32_EI_CLASS] != ELF32_CLASS_32) {
        return ELF32_ERR_CLASS;
    }

    if (header->e_ident[ELF32_EI_DATA] != ELF32_DATA_LSB) {
        return ELF32_ERR_ENDIAN;
    }

    if (header->e_ident[ELF32_EI_VERSION] != ELF32_VERSION_CURRENT) {
        return ELF32_ERR_IDENT_VERSION;
    }

    if (header->e_type != ELF32_ET_EXEC) {
        return ELF32_ERR_TYPE;
    }

    if (header->e_machine != ELF32_EM_386) {
        return ELF32_ERR_MACHINE;
    }

    if (header->e_version != ELF32_VERSION_CURRENT) {
        return ELF32_ERR_VERSION;
    }

    if (header->e_ehsize != sizeof(elf32_ehdr_t)) {
        return ELF32_ERR_EHSIZE;
    }

    if (header->e_phnum == 0U) {
        return ELF32_ERR_NO_PHDRS;
    }

    if (header->e_phentsize != sizeof(elf32_phdr_t)) {
        return ELF32_ERR_PHENTSIZE;
    }

    if (header->e_phoff < header->e_ehsize ||
        header->e_phoff > file_size) {
        return ELF32_ERR_PHDR_BOUNDS;
    }

    ph_table_bytes =
        (uint32_t)header->e_phnum * (uint32_t)header->e_phentsize;

    if (ph_table_bytes > file_size - header->e_phoff) {
        return ELF32_ERR_PHDR_BOUNDS;
    }

    return ELF32_OK;
}

int elf32_read_header(
    fs_node_t *node,
    elf32_ehdr_t *header_out
) {
    uint32_t bytes_read;

    if (node == 0 || header_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    if ((node->flags & FS_FILE) == 0U) {
        return ELF32_ERR_NOT_FILE;
    }

    if (node->size < sizeof(elf32_ehdr_t)) {
        return ELF32_ERR_TOO_SMALL;
    }

    if (node->read == 0) {
        return ELF32_ERR_IO;
    }

    bytes_read = read_fs(
        node,
        0U,
        sizeof(elf32_ehdr_t),
        (uint8_t *)header_out
    );

    if (bytes_read != sizeof(elf32_ehdr_t)) {
        return ELF32_ERR_IO;
    }

    return elf32_validate_header(header_out, node->size);
}

int elf32_read_program_header(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    uint32_t index,
    elf32_phdr_t *program_header_out
) {
    int status;

    if (node == 0 ||
        header == 0 ||
        program_header_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    if ((node->flags & FS_FILE) == 0U) {
        return ELF32_ERR_NOT_FILE;
    }

    if (node->read == 0) {
        return ELF32_ERR_IO;
    }

    status = elf32_validate_header(header, node->size);
    if (status != ELF32_OK) {
        return status;
    }

    if (index >= (uint32_t)header->e_phnum) {
        return ELF32_ERR_PHDR_INDEX;
    }

    return elf32_read_program_header_unchecked(
        node,
        header,
        index,
        program_header_out
    );
}

int elf32_validate_program_headers(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    elf32_image_info_t *image_out
) {
    elf32_image_info_t image;
    uint8_t entry_is_executable = 0U;
    int status;

    if (node == 0 || header == 0 || image_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    if ((node->flags & FS_FILE) == 0U) {
        return ELF32_ERR_NOT_FILE;
    }

    if (node->read == 0) {
        return ELF32_ERR_IO;
    }

    status = elf32_validate_header(header, node->size);
    if (status != ELF32_OK) {
        return status;
    }

    image.entry = header->e_entry;
    image.lowest_vaddr = ELF32_ADDR_MAX;
    image.highest_vaddr = 0U;
    image.load_segment_count = 0U;

    for (uint32_t i = 0U;
         i < (uint32_t)header->e_phnum;
         ++i) {
        elf32_phdr_t program_header;

        status = elf32_read_program_header_unchecked(
            node,
            header,
            i,
            &program_header
        );
        if (status != ELF32_OK) {
            return status;
        }

        if (program_header.p_type == ELF32_PT_INTERP) {
            return ELF32_ERR_INTERP_UNSUPPORTED;
        }

        if (program_header.p_type == ELF32_PT_DYNAMIC) {
            return ELF32_ERR_DYNAMIC_UNSUPPORTED;
        }

        if (program_header.p_type == ELF32_PT_LOAD) {
            uint32_t segment_end;

            status = elf32_validate_load_segment(
                &program_header,
                node->size,
                &segment_end
            );
            if (status != ELF32_OK) {
                return status;
            }

            if (program_header.p_memsz == 0U) {
                continue;
            }

            if (image.load_segment_count == 0U ||
                program_header.p_vaddr < image.lowest_vaddr) {
                image.lowest_vaddr = program_header.p_vaddr;
            }

            if (image.load_segment_count == 0U ||
                segment_end > image.highest_vaddr) {
                image.highest_vaddr = segment_end;
            }

            image.load_segment_count++;

            if ((program_header.p_flags & ELF32_PF_X) != 0U &&
                header->e_entry >= program_header.p_vaddr &&
                header->e_entry < segment_end) {
                entry_is_executable = 1U;
            }
        }
    }

    if (image.load_segment_count == 0U) {
        return ELF32_ERR_NO_LOAD_SEGMENTS;
    }

    if (entry_is_executable == 0U) {
        return ELF32_ERR_ENTRY_NOT_EXECUTABLE;
    }

    *image_out = image;
    return ELF32_OK;
}

int elf32_inspect(
    fs_node_t *node,
    elf32_ehdr_t *header_out,
    elf32_image_info_t *image_out
) {
    int status;

    if (node == 0 || header_out == 0 || image_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    status = elf32_read_header(node, header_out);
    if (status != ELF32_OK) {
        return status;
    }

    return elf32_validate_program_headers(
        node,
        header_out,
        image_out
    );
}

int elf32_load_image(
    fs_node_t *node,
    elf32_loaded_image_t *loaded_out
) {
    elf32_ehdr_t header;
    elf32_image_info_t image;
    elf32_loaded_image_t loaded;
    int status;

    if (node == 0 || loaded_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    status = elf32_inspect(node, &header, &image);
    if (status != ELF32_OK) {
        return status;
    }

    for (uint32_t i = 0U;
         i < (uint32_t)header.e_phnum;
         ++i) {
        elf32_phdr_t program_header;

        status = elf32_read_program_header_unchecked(
            node,
            &header,
            i,
            &program_header
        );
        if (status != ELF32_OK) {
            return status;
        }

        if (program_header.p_type != ELF32_PT_LOAD ||
            program_header.p_memsz == 0U) {
            continue;
        }

        status = elf32_validate_user_load_range(
            &program_header,
            0,
            0
        );
        if (status != ELF32_OK) {
            return status;
        }
    }

    loaded.cr3 = vmm_create_address_space();
    if (loaded.cr3 == 0U) {
        return ELF32_ERR_NO_MEMORY;
    }

    loaded.entry = image.entry;
    loaded.lowest_vaddr = image.lowest_vaddr;
    loaded.highest_vaddr = image.highest_vaddr;
    loaded.load_segment_count = image.load_segment_count;
    loaded.mapped_page_count = 0U;

    for (uint32_t i = 0U;
         i < (uint32_t)header.e_phnum;
         ++i) {
        elf32_phdr_t program_header;

        status = elf32_read_program_header_unchecked(
            node,
            &header,
            i,
            &program_header
        );
        if (status != ELF32_OK) {
            goto fail;
        }

        if (program_header.p_type != ELF32_PT_LOAD ||
            program_header.p_memsz == 0U) {
            continue;
        }

        status = elf32_map_load_segment_pages(
            loaded.cr3,
            &program_header,
            &loaded.mapped_page_count
        );
        if (status != ELF32_OK) {
            goto fail;
        }
    }

    for (uint32_t i = 0U;
         i < (uint32_t)header.e_phnum;
         ++i) {
        elf32_phdr_t program_header;

        status = elf32_read_program_header_unchecked(
            node,
            &header,
            i,
            &program_header
        );
        if (status != ELF32_OK) {
            goto fail;
        }

        if (program_header.p_type != ELF32_PT_LOAD ||
            program_header.p_memsz == 0U) {
            continue;
        }

        status = elf32_populate_load_segment(
            node,
            loaded.cr3,
            &program_header
        );
        if (status != ELF32_OK) {
            goto fail;
        }
    }

    *loaded_out = loaded;
    return ELF32_OK;

fail:
    vmm_destroy_address_space(loaded.cr3);
    return status;
}

static int elf32_write_user_virtual(
    uint32_t cr3,
    uintptr_t virt_addr,
    const void *source,
    uint32_t size
) {
    const uint8_t *src = (const uint8_t *)source;
    uint32_t remaining = size;
    uintptr_t target = virt_addr;

    if (source == 0 && size != 0U) {
        return ELF32_ERR_ARGUMENT;
    }

    while (remaining != 0U) {
        uint32_t page_remaining =
            VMM_PAGE_SIZE -
            (uint32_t)(target & (VMM_PAGE_SIZE - 1U));
        uint32_t chunk =
            elf32_min_u32(remaining, page_remaining);
        uintptr_t phys =
            vmm_translate_address_space(cr3, target);

        if (phys == 0U) {
            return ELF32_ERR_STACK_MAP;
        }

        if (vmm_copy_to_phys(phys, src, chunk) != 0) {
            return ELF32_ERR_SEGMENT_COPY;
        }

        src += chunk;
        target += chunk;
        remaining -= chunk;
    }

    return ELF32_OK;
}

static int elf32_string_size(
    const char *value,
    uint32_t *size_out
) {
    if (value == 0 || size_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    for (uint32_t i = 0U;
         i < CZK_EXEC_MAX_STRING;
         ++i) {
        if (value[i] == 0) {
            *size_out = i + 1U;
            return ELF32_OK;
        }
    }

    return ELF32_ERR_STACK_ARGS;
}

static int elf32_push_u32(
    uint32_t cr3,
    uintptr_t stack_bottom,
    uintptr_t *stack_pointer,
    uint32_t value
) {
    uintptr_t sp;

    if (stack_pointer == 0 || *stack_pointer < stack_bottom + 4U) {
        return ELF32_ERR_STACK_ARGS;
    }

    sp = *stack_pointer - 4U;

    if (elf32_write_user_virtual(
            cr3,
            sp,
            &value,
            sizeof(value)) != ELF32_OK) {
        return ELF32_ERR_STACK_MAP;
    }

    *stack_pointer = sp;
    return ELF32_OK;
}

static int elf32_map_user_stack(
    uint32_t cr3,
    uintptr_t *stack_top_out
) {
    uintptr_t stack_top = ELF32_USER_STACK_TOP;
    uintptr_t stack_bottom =
        stack_top -
        (ELF32_USER_STACK_PAGES * VMM_PAGE_SIZE);

    if (stack_bottom < VMM_USER_MIN_ADDR ||
        stack_top > VMM_USER_MAX_ADDR) {
        return ELF32_ERR_STACK_MAP;
    }

    for (uintptr_t addr = stack_bottom;
         addr < stack_top;
         addr += VMM_PAGE_SIZE) {
        uint32_t frame;

        if (vmm_translate_address_space(cr3, addr) != 0U) {
            return ELF32_ERR_STACK_CONFLICT;
        }

        frame = pmm_alloc_frame();
        if (frame == 0U) {
            return ELF32_ERR_NO_MEMORY;
        }

        if (vmm_zero_phys(frame, VMM_PAGE_SIZE) != 0) {
            pmm_free_frame(frame);
            return ELF32_ERR_SEGMENT_ZERO;
        }

        if (vmm_map_user_page(
                cr3,
                addr,
                frame,
                VMM_PAGE_RW) != 0) {
            pmm_free_frame(frame);
            return ELF32_ERR_STACK_MAP;
        }
    }

    if (stack_top_out != 0) {
        *stack_top_out = stack_top;
    }

    return ELF32_OK;
}

static int elf32_build_initial_stack(
    uint32_t cr3,
    const char *const argv[],
    uint32_t argc,
    const char *const envp[],
    uint32_t envc,
    uintptr_t *stack_pointer_out
) {
    uintptr_t stack_top = 0U;
    uintptr_t stack_bottom;
    uintptr_t sp;
    uint32_t argv_addrs[CZK_EXEC_MAX_ARGS];
    uint32_t envp_addrs[CZK_EXEC_MAX_ENVS];
    int status;

    if (stack_pointer_out == 0 ||
        argc > CZK_EXEC_MAX_ARGS ||
        envc > CZK_EXEC_MAX_ENVS ||
        (argc != 0U && argv == 0) ||
        (envc != 0U && envp == 0)) {
        return ELF32_ERR_STACK_ARGS;
    }

    status = elf32_map_user_stack(cr3, &stack_top);
    if (status != ELF32_OK) {
        return status;
    }

    stack_bottom =
        stack_top -
        (ELF32_USER_STACK_PAGES * VMM_PAGE_SIZE);
    sp = stack_top;

    for (uint32_t i = envc; i > 0U; --i) {
        uint32_t index = i - 1U;
        uint32_t string_size;

        status = elf32_string_size(
            envp[index],
            &string_size
        );
        if (status != ELF32_OK ||
            sp < stack_bottom + string_size) {
            return ELF32_ERR_STACK_ARGS;
        }

        sp -= string_size;

        status = elf32_write_user_virtual(
            cr3,
            sp,
            envp[index],
            string_size
        );
        if (status != ELF32_OK) {
            return status;
        }

        envp_addrs[index] = (uint32_t)sp;
    }

    for (uint32_t i = argc; i > 0U; --i) {
        uint32_t index = i - 1U;
        uint32_t string_size;

        status = elf32_string_size(
            argv[index],
            &string_size
        );
        if (status != ELF32_OK ||
            sp < stack_bottom + string_size) {
            return ELF32_ERR_STACK_ARGS;
        }

        sp -= string_size;

        status = elf32_write_user_virtual(
            cr3,
            sp,
            argv[index],
            string_size
        );
        if (status != ELF32_OK) {
            return status;
        }

        argv_addrs[index] = (uint32_t)sp;
    }

    /*
     * ABI v1 requires the entry stack pointer to be 16-byte aligned.
     * Padding lives between the pointer vectors and copied strings, so
     * argc remains exactly at [ESP].
     */
    sp &= ~(uintptr_t)0x3U;

    {
        uint32_t vector_bytes =
            (argc + envc + 3U) * (uint32_t)sizeof(uint32_t);
        uintptr_t required_mod =
            (uintptr_t)(vector_bytes & (CZK_ABI_STACK_ALIGNMENT - 1U));

        while ((sp & (CZK_ABI_STACK_ALIGNMENT - 1U)) != required_mod) {
            if (sp < stack_bottom + sizeof(uint32_t)) {
                return ELF32_ERR_STACK_ARGS;
            }
            sp -= sizeof(uint32_t);
        }
    }

    status = elf32_push_u32(cr3, stack_bottom, &sp, 0U);
    if (status != ELF32_OK) {
        return status;
    }

    for (uint32_t i = envc; i > 0U; --i) {
        status = elf32_push_u32(
            cr3,
            stack_bottom,
            &sp,
            envp_addrs[i - 1U]
        );
        if (status != ELF32_OK) {
            return status;
        }
    }

    status = elf32_push_u32(cr3, stack_bottom, &sp, 0U);
    if (status != ELF32_OK) {
        return status;
    }

    for (uint32_t i = argc; i > 0U; --i) {
        status = elf32_push_u32(
            cr3,
            stack_bottom,
            &sp,
            argv_addrs[i - 1U]
        );
        if (status != ELF32_OK) {
            return status;
        }
    }

    status = elf32_push_u32(
        cr3,
        stack_bottom,
        &sp,
        argc
    );
    if (status != ELF32_OK) {
        return status;
    }

    *stack_pointer_out = sp;
    return ELF32_OK;
}

int elf32_spawn(
    fs_node_t *node,
    uint32_t *pid_out
) {
    elf32_loaded_image_t loaded = {0};
    const char *argv[1];
    uintptr_t user_stack_pointer = 0U;
    int status;
    int pid;

    if (node == 0 || pid_out == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    status = elf32_load_image(node, &loaded);
    if (status != ELF32_OK) {
        return status;
    }

    argv[0] = node->name;

    status = elf32_build_initial_stack(
        loaded.cr3,
        argv,
        1U,
        0,
        0U,
        &user_stack_pointer
    );
    if (status != ELF32_OK) {
        elf32_unload_image(&loaded);
        return status;
    }

    pid = sched_create_user_task_in_address_space(
        node->name,
        (uintptr_t)loaded.entry,
        user_stack_pointer,
        loaded.cr3
    );

    if (pid < 0) {
        elf32_unload_image(&loaded);
        return ELF32_ERR_TASK_CREATE;
    }

    /*
     * Ownership of the address space now belongs to process_t and will be
     * released by process_destroy() after the task becomes reapable.
     */
    loaded.cr3 = 0U;
    *pid_out = (uint32_t)pid;
    return ELF32_OK;
}

int elf32_exec_current(
    fs_node_t *node,
    const char *const argv[],
    uint32_t argc,
    const char *const envp[],
    uint32_t envc,
    registers_t *regs
) {
    elf32_loaded_image_t loaded = {0};
    uintptr_t user_stack_pointer = 0U;
    int status;

    if (node == 0 || regs == 0) {
        return ELF32_ERR_ARGUMENT;
    }

    status = elf32_load_image(node, &loaded);
    if (status != ELF32_OK) {
        return status;
    }

    status = elf32_build_initial_stack(
        loaded.cr3,
        argv,
        argc,
        envp,
        envc,
        &user_stack_pointer
    );
    if (status != ELF32_OK) {
        elf32_unload_image(&loaded);
        return status;
    }

    if (sched_exec_current_address_space(
            node->name,
            (uintptr_t)loaded.entry,
            user_stack_pointer,
            loaded.cr3,
            regs) != 0) {
        elf32_unload_image(&loaded);
        return ELF32_ERR_EXEC_REPLACE;
    }

    /*
     * The current process now owns loaded.cr3. The scheduler already
     * switched to it and released the old process image.
     */
    loaded.cr3 = 0U;
    return ELF32_OK;
}

void elf32_unload_image(elf32_loaded_image_t *loaded) {
    if (loaded == 0) {
        return;
    }

    if (loaded->cr3 != 0U) {
        vmm_destroy_address_space(loaded->cr3);
    }

    loaded->cr3 = 0U;
    loaded->entry = 0U;
    loaded->lowest_vaddr = 0U;
    loaded->highest_vaddr = 0U;
    loaded->load_segment_count = 0U;
    loaded->mapped_page_count = 0U;
}

const char *elf32_status_string(int status) {
    switch (status) {
        case ELF32_OK:
            return "ok";
        case ELF32_ERR_ARGUMENT:
            return "invalid argument";
        case ELF32_ERR_NOT_FILE:
            return "not a regular file";
        case ELF32_ERR_TOO_SMALL:
            return "file too small";
        case ELF32_ERR_IO:
            return "header read failed";
        case ELF32_ERR_MAGIC:
            return "invalid ELF magic";
        case ELF32_ERR_CLASS:
            return "unsupported ELF class";
        case ELF32_ERR_ENDIAN:
            return "unsupported ELF endianness";
        case ELF32_ERR_IDENT_VERSION:
            return "unsupported ELF ident version";
        case ELF32_ERR_TYPE:
            return "unsupported ELF type";
        case ELF32_ERR_MACHINE:
            return "unsupported ELF machine";
        case ELF32_ERR_VERSION:
            return "unsupported ELF version";
        case ELF32_ERR_EHSIZE:
            return "invalid ELF header size";
        case ELF32_ERR_PHENTSIZE:
            return "invalid program header size";
        case ELF32_ERR_NO_PHDRS:
            return "ELF has no program headers";
        case ELF32_ERR_PHDR_BOUNDS:
            return "program header table outside file";
        case ELF32_ERR_PHDR_INDEX:
            return "program header index out of range";
        case ELF32_ERR_PHDR_IO:
            return "program header read failed";
        case ELF32_ERR_SEGMENT_SIZE:
            return "segment file size exceeds memory size";
        case ELF32_ERR_SEGMENT_FILE_BOUNDS:
            return "segment file range outside file";
        case ELF32_ERR_SEGMENT_VADDR_OVERFLOW:
            return "segment virtual address overflows";
        case ELF32_ERR_SEGMENT_ALIGN:
            return "invalid segment alignment";
        case ELF32_ERR_NO_LOAD_SEGMENTS:
            return "ELF has no non-empty load segments";
        case ELF32_ERR_INTERP_UNSUPPORTED:
            return "ELF interpreter unsupported";
        case ELF32_ERR_DYNAMIC_UNSUPPORTED:
            return "dynamic ELF unsupported";
        case ELF32_ERR_ENTRY_NOT_EXECUTABLE:
            return "entry point is outside executable load segments";
        case ELF32_ERR_SEGMENT_USER_RANGE:
            return "load segment outside userspace range";
        case ELF32_ERR_ADDRESS_CONFLICT:
            return "load segment conflicts with existing mapping";
        case ELF32_ERR_NO_MEMORY:
            return "out of memory while loading ELF";
        case ELF32_ERR_MAP_FAILED:
            return "failed to map ELF page";
        case ELF32_ERR_SEGMENT_READ:
            return "failed to read ELF segment";
        case ELF32_ERR_SEGMENT_COPY:
            return "failed to copy ELF segment";
        case ELF32_ERR_SEGMENT_ZERO:
            return "failed to zero ELF memory";
        case ELF32_ERR_PAGE_FLAGS:
            return "failed to inspect or update ELF page flags";
        case ELF32_ERR_STACK_CONFLICT:
            return "userspace stack conflicts with existing mapping";
        case ELF32_ERR_STACK_MAP:
            return "failed to map userspace stack";
        case ELF32_ERR_TASK_CREATE:
            return "failed to create ELF userspace task";
        case ELF32_ERR_STACK_ARGS:
            return "arguments do not fit ELF startup stack";
        case ELF32_ERR_EXEC_REPLACE:
            return "failed to replace current process image";
        default:
            return "unknown ELF error";
    }
}
