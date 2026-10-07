#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

#define ELF32_NIDENT 16U

#define ELF32_EI_MAG0        0U
#define ELF32_EI_MAG1        1U
#define ELF32_EI_MAG2        2U
#define ELF32_EI_MAG3        3U
#define ELF32_EI_CLASS       4U
#define ELF32_EI_DATA        5U
#define ELF32_EI_VERSION     6U

#define ELF32_MAG0 0x7FU
#define ELF32_MAG1 'E'
#define ELF32_MAG2 'L'
#define ELF32_MAG3 'F'

#define ELF32_CLASS_32         1U
#define ELF32_DATA_LSB         1U
#define ELF32_VERSION_CURRENT  1U

#define ELF32_ET_EXEC 2U
#define ELF32_EM_386  3U

#define ELF32_PT_NULL    0U
#define ELF32_PT_LOAD    1U
#define ELF32_PT_DYNAMIC 2U
#define ELF32_PT_INTERP  3U
#define ELF32_PT_NOTE    4U
#define ELF32_PT_PHDR    6U

#define ELF32_PF_X 0x1U
#define ELF32_PF_W 0x2U
#define ELF32_PF_R 0x4U

typedef uint16_t elf32_half_t;
typedef uint32_t elf32_word_t;
typedef int32_t elf32_sword_t;
typedef uint32_t elf32_addr_t;
typedef uint32_t elf32_off_t;

typedef struct {
    uint8_t e_ident[ELF32_NIDENT];
    elf32_half_t e_type;
    elf32_half_t e_machine;
    elf32_word_t e_version;
    elf32_addr_t e_entry;
    elf32_off_t e_phoff;
    elf32_off_t e_shoff;
    elf32_word_t e_flags;
    elf32_half_t e_ehsize;
    elf32_half_t e_phentsize;
    elf32_half_t e_phnum;
    elf32_half_t e_shentsize;
    elf32_half_t e_shnum;
    elf32_half_t e_shstrndx;
} elf32_ehdr_t;

typedef struct {
    elf32_word_t p_type;
    elf32_off_t p_offset;
    elf32_addr_t p_vaddr;
    elf32_addr_t p_paddr;
    elf32_word_t p_filesz;
    elf32_word_t p_memsz;
    elf32_word_t p_flags;
    elf32_word_t p_align;
} elf32_phdr_t;

typedef struct {
    elf32_addr_t entry;
    elf32_addr_t lowest_vaddr;
    elf32_addr_t highest_vaddr;
    uint32_t load_segment_count;
} elf32_image_info_t;

typedef struct {
    uint32_t cr3;
    elf32_addr_t entry;
    elf32_addr_t lowest_vaddr;
    elf32_addr_t highest_vaddr;
    uint32_t load_segment_count;
    uint32_t mapped_page_count;
} elf32_loaded_image_t;

typedef enum {
    ELF32_OK = 0,
    ELF32_ERR_ARGUMENT = -1,
    ELF32_ERR_NOT_FILE = -2,
    ELF32_ERR_TOO_SMALL = -3,
    ELF32_ERR_IO = -4,
    ELF32_ERR_MAGIC = -5,
    ELF32_ERR_CLASS = -6,
    ELF32_ERR_ENDIAN = -7,
    ELF32_ERR_IDENT_VERSION = -8,
    ELF32_ERR_TYPE = -9,
    ELF32_ERR_MACHINE = -10,
    ELF32_ERR_VERSION = -11,
    ELF32_ERR_EHSIZE = -12,
    ELF32_ERR_PHENTSIZE = -13,
    ELF32_ERR_NO_PHDRS = -14,
    ELF32_ERR_PHDR_BOUNDS = -15,
    ELF32_ERR_PHDR_INDEX = -16,
    ELF32_ERR_PHDR_IO = -17,
    ELF32_ERR_SEGMENT_SIZE = -18,
    ELF32_ERR_SEGMENT_FILE_BOUNDS = -19,
    ELF32_ERR_SEGMENT_VADDR_OVERFLOW = -20,
    ELF32_ERR_SEGMENT_ALIGN = -21,
    ELF32_ERR_NO_LOAD_SEGMENTS = -22,
    ELF32_ERR_INTERP_UNSUPPORTED = -23,
    ELF32_ERR_DYNAMIC_UNSUPPORTED = -24,
    ELF32_ERR_ENTRY_NOT_EXECUTABLE = -25,
    ELF32_ERR_SEGMENT_USER_RANGE = -26,
    ELF32_ERR_ADDRESS_CONFLICT = -27,
    ELF32_ERR_NO_MEMORY = -28,
    ELF32_ERR_MAP_FAILED = -29,
    ELF32_ERR_SEGMENT_READ = -30,
    ELF32_ERR_SEGMENT_COPY = -31,
    ELF32_ERR_SEGMENT_ZERO = -32,
    ELF32_ERR_PAGE_FLAGS = -33
} elf32_status_t;

int elf32_validate_header(
    const elf32_ehdr_t *header,
    uint32_t file_size
);

int elf32_read_header(
    fs_node_t *node,
    elf32_ehdr_t *header_out
);

int elf32_read_program_header(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    uint32_t index,
    elf32_phdr_t *program_header_out
);

int elf32_validate_program_headers(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    elf32_image_info_t *image_out
);

int elf32_inspect(
    fs_node_t *node,
    elf32_ehdr_t *header_out,
    elf32_image_info_t *image_out
);

int elf32_load_image(
    fs_node_t *node,
    elf32_loaded_image_t *loaded_out
);

void elf32_unload_image(elf32_loaded_image_t *loaded);

const char *elf32_status_string(int status);
