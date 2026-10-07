#include <kernel/elf32.h>

#define ELF32_ADDR_MAX 0xFFFFFFFFU

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
        default:
            return "unknown ELF error";
    }
}
