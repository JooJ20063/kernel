#pragma once

#include <stdint.h>
#include <kernel/vfs.h>
#include <arch/x86/regs.h>

/* Limites operacionais para passagem de argumentos e variáveis de ambiente na pilha de User Mode */
#define ELF32_EXEC_MAX_ARGS   16U
#define ELF32_EXEC_MAX_ENVS   16U
#define ELF32_EXEC_MAX_STRING 128U

/* Tamanho em bytes do vetor e_ident do cabeçalho ELF */
#define ELF32_NIDENT 16U

/* Índices dentro do vetor e_ident */
#define ELF32_EI_MAG0        0U  // 0x7F
#define ELF32_EI_MAG1        1U  // 'E'
#define ELF32_EI_MAG2        2U  // 'L'
#define ELF32_EI_MAG3        3U  // 'F'
#define ELF32_EI_CLASS       4U  // Arquitetura (32-bit vs 64-bit)
#define ELF32_EI_DATA        5U  // Endianness (Little vs Big Endian)
#define ELF32_EI_VERSION     6U  // Versão do formato ELF

/* Magic Bytes de identificação ELF (\x7F ELF) */
#define ELF32_MAG0 0x7FU
#define ELF32_MAG1 'E'
#define ELF32_MAG2 'L'
#define ELF32_MAG3 'F'

/* Valores esperados para validação de binários ELF32 no x86 */
#define ELF32_CLASS_32         1U  // ELF 32-bit
#define ELF32_DATA_LSB         1U  // Little-Endian (x86)
#define ELF32_VERSION_CURRENT  1U  // Versão 1 do formato

#define ELF32_ET_EXEC 2U  // Tipo de arquivo: Executável
#define ELF32_EM_386  3U  // Arquitetura alvo: Intel 80386+

/* Tipos de Segmentos de Programa (p_type) */
#define ELF32_PT_NULL    0U  // Entrada inativa
#define ELF32_PT_LOAD    1U  // Segmento carregável na memória (Text/Data/BSS)
#define ELF32_PT_DYNAMIC 2U  // Informações de linking dinâmico (não suportado)
#define ELF32_PT_INTERP  3U  // Caminho do carregador dinâmico (ex: ld.so - não suportado)
#define ELF32_PT_NOTE    4U  // Informações auxiliares de notas
#define ELF32_PT_PHDR    6U  // Posição da própria tabela de Program Headers

/* Permissões de acesso das páginas de memória do segmento (p_flags) */
#define ELF32_PF_X 0x1U  // Executável
#define ELF32_PF_W 0x2U  // Gravável
#define ELF32_PF_R 0x4U  // Legível

/* Tipos de dados primitivos da especificação ELF32 */
typedef uint16_t elf32_half_t;
typedef uint32_t elf32_word_t;
typedef int32_t elf32_sword_t;
typedef uint32_t elf32_addr_t;
typedef uint32_t elf32_off_t;

/**
 * Cabeçalho Principal do Arquivo ELF (ELF Executable Header - 52 bytes no x86)
 * Fica localizado exatamente no offset 0 do arquivo.
 */
typedef struct {
    uint8_t e_ident[ELF32_NIDENT]; // Magic bytes e metadados de arquitetura
    elf32_half_t e_type;           // Tipo de arquivo (ET_EXEC, ET_DYN, etc.)
    elf32_half_t e_machine;        // Arquitetura de hardware exigida (EM_386)
    elf32_word_t e_version;        // Versão da especificação ELF
    elf32_addr_t e_entry;          // Endereço de memória do ponto de entrada (entry point)
    elf32_off_t e_phoff;           // Offset do arquivo onde começa a Program Header Table
    elf32_off_t e_shoff;           // Offset do arquivo onde começa a Section Header Table
    elf32_word_t e_flags;          // Flags específicas da arquitetura
    elf32_half_t e_ehsize;         // Tamanho deste cabeçalho
    elf32_half_t e_phentsize;      // Tamanho de cada entrada na Program Header Table
    elf32_half_t e_phnum;          // Número de entradas na Program Header Table
    elf32_half_t e_shentsize;      // Tamanho de cada entrada na Section Header Table
    elf32_half_t e_shnum;          // Número de entradas na Section Header Table
    elf32_half_t e_shstrndx;       // Índice da tabela de nomes de seções
} elf32_ehdr_t;

/**
 * Cabeçalho de Segmento de Programa (Program Header - 32 bytes no x86)
 * Instruções para o SO de como mapear o arquivo em segmentos de memória virtual.
 */
typedef struct {
    elf32_word_t p_type;   // Tipo de segmento (PT_LOAD, PT_INTERP, etc.)
    elf32_off_t p_offset;  // Offset do segmento dentro do arquivo
    elf32_addr_t p_vaddr;  // Endereço virtual onde o segmento deve ser carregado
    elf32_addr_t p_paddr;  // Endereço físico (ignorado em sistemas com paginação)
    elf32_word_t p_filesz; // Tamanho dos dados do segmento presentes no arquivo
    elf32_word_t p_memsz;  // Tamanho do segmento em memória (p_memsz > p_filesz indica BSS)
    elf32_word_t p_flags;  // Permissões do segmento (PF_R, PF_W, PF_X)
    elf32_word_t p_align;  // Requisito de alinhamento em memória (múltiplo de potências de 2)
} elf32_phdr_t;

/* Metadados consolidados extraídos de uma imagem ELF sem carregá-la na memória */
typedef struct {
    elf32_addr_t entry;            // Endereço de entrada
    elf32_addr_t lowest_vaddr;     // Menor endereço virtual ocupado
    elf32_addr_t highest_vaddr;    // Maior endereço virtual ocupado
    uint32_t load_segment_count;   // Quantidade de segmentos PT_LOAD encontrados
} elf32_image_info_t;

/* Estrutura que representa um executável ELF carregado e mapeado num espaço de endereçamento (CR3) */
typedef struct {
    uint32_t cr3;                  // Endereço do Page Directory onde a imagem foi mapeada
    elf32_addr_t entry;            // Ponto de entrada
    elf32_addr_t lowest_vaddr;     // Limite inferior do espaço virtual ocupado
    elf32_addr_t highest_vaddr;    // Limite superior do espaço virtual ocupado
    uint32_t load_segment_count;   // Número de segmentos carregados
    uint32_t mapped_page_count;    // Total de páginas físicas mapeadas para este binário
} elf32_loaded_image_t;

/* Enumerado de códigos de retorno e erros detalhados de validação e carregamento */
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
    ELF32_ERR_PAGE_FLAGS = -33,
    ELF32_ERR_STACK_CONFLICT = -34,
    ELF32_ERR_STACK_MAP = -35,
    ELF32_ERR_TASK_CREATE = -36,
    ELF32_ERR_STACK_ARGS = -37,
    ELF32_ERR_EXEC_REPLACE = -38
} elf32_status_t;

/* Valida a assinatura (magic bytes), arquitetura e limites do cabeçalho principal ELF */
int elf32_validate_header(
    const elf32_ehdr_t *header,
    uint32_t file_size
);

/* Lê o cabeçalho ELF32 diretamente a partir de um nó de arquivo VFS */
int elf32_read_header(
    fs_node_t *node,
    elf32_ehdr_t *header_out
);

/* Lê uma entrada específica da Program Header Table do arquivo VFS */
int elf32_read_program_header(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    uint32_t index,
    elf32_phdr_t *program_header_out
);

/* Percorre e valida todos os Program Headers, calculando os limites virtuais da imagem */
int elf32_validate_program_headers(
    fs_node_t *node,
    const elf32_ehdr_t *header,
    elf32_image_info_t *image_out
);

/* Inspeciona um nó VFS preenchendo o cabeçalho e as métricas de alocação do binário */
int elf32_inspect(
    fs_node_t *node,
    elf32_ehdr_t *header_out,
    elf32_image_info_t *image_out
);

/* Carrega o binário ELF na memória virtual, alocando páginas e copiado os segmentos PT_LOAD */
int elf32_load_image(
    fs_node_t *node,
    elf32_loaded_image_t *loaded_out
);

/* Libera as páginas de memória física e tabelas de páginas associadas a uma imagem ELF carregada */
void elf32_unload_image(elf32_loaded_image_t *loaded);

/* Cria um novo processo/tarefa (Fork/Spawn) a partir de um arquivo ELF */
int elf32_spawn(
    fs_node_t *node,
    uint32_t *pid_out
);

/* Substitui a imagem do processo atual pelo novo executável ELF (comportamento do execve) */
int elf32_exec_current(
    fs_node_t *node,
    const char *const argv[],
    uint32_t argc,
    const char *const envp[],
    uint32_t envc,
    registers_t *regs
);

/* Converte um código de status de erro elf32_status_t para uma string legível */
const char *elf32_status_string(int status);