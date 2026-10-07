#include <kernel/elf32.h>

_Static_assert(sizeof(elf32_ehdr_t) == 52U, "ELF32 header size mismatch");
_Static_assert(sizeof(elf32_phdr_t) == 32U, "ELF32 program header size mismatch");

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
        default:
            return "unknown ELF error";
    }
}
