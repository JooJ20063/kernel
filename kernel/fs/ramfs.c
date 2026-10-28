#include <kernel/ramfs.h>
#include <kernel/kmalloc.h>

/* Tamanho do bloco padrão da especificação do formato TAR (512 bytes) */
#define TAR_BLOCK_SIZE 512U

/* Capacidade inicial em bytes para novos arquivos criados dinamicamente no RAMFS */
#define RAMFS_NEW_FILE_CAPACITY 256U

/**
 * Layout do cabeçalho de um arquivo dentro de um arquivo de arquivo TAR (POSIX ustar).
 */
struct tar_header {
    char name[100];     // Nome ou caminho do arquivo
    char mode[8];       // Permissões em Octal
    char uid[8];        // User ID em Octal
    char gid[8];        // Group ID em Octal
    char size[12];      // Tamanho do arquivo em bytes (Octal)
    char mtime[12];     // Data de modificação (Octal)
    char checksum[8];   // Checksum do cabeçalho
    char typeflag;      // Tipo de entrada ('0': Arquivo normal, '5': Diretório)
    char linkname[100]; // Nome do link simbólico/hardlink
    char magic[6];      // Assinatura "ustar"
    char version[2];    // Versão do formato ustar
    char uname[32];     // Nome do usuário
    char gname[32];     // Nome do grupo
    char devmajor[8];   // Dispositivo Major
    char devminor[8];   // Dispositivo Minor
    char prefix[155];   // Prefixo do caminho longo
    char pad[12];       // Padding para alinhar a 512 bytes
};

/**
 * Estrutura privada estendida de um nó do RAMFS (armazenada em fs_node_t.device).
 * Mantém os ponteiros da árvore hierárquica (filhos/irmãos) e o buffer de dados.
 */
struct ramfs_entry {
    fs_node_t node;                  // Nó genérico do VFS
    uintptr_t data_ptr;             // Ponteiro para o buffer de conteúdo do arquivo em RAM
    uint32_t capacity;              // Capacidade total alocada do buffer em bytes
    uint8_t writable;               // Flag indicando se é dinâmico/gravável (1) ou Read-Only no TAR (0)
    struct ramfs_entry *parent;       // Ponteiro para o nó diretório pai
    struct ramfs_entry *first_child;  // Primeiro nó filho (se for diretório)
    struct ramfs_entry *last_child;   // Último nó filho (se for diretório)
    struct ramfs_entry *next_sibling; // Próximo nó no mesmo nível de diretório
};

/* Instância estática da raiz do RAMFS ("/") */
static struct ramfs_entry ramfs_root_entry;

/* Protótipos das funções do VFS para o RAMFS */
static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
static fs_node_t *ramfs_readdir(fs_node_t *node, uint32_t index);
static fs_node_t *ramfs_finddir(fs_node_t *node, const char *name);
static fs_node_t *ramfs_create(fs_node_t *node, const char *name, uint32_t flags);
static int ramfs_remove(fs_node_t *node, const char *name);

/* Preenche um buffer de memória com zeros */
static void mem_zero(uint8_t *dst, uint32_t size) {
    for (uint32_t i = 0U; i < size; ++i) {
        dst[i] = 0U;
    }
}

/* Copia 'size' bytes do buffer de origem para o destino */
static void mem_copy(uint8_t *dst, const uint8_t *src, uint32_t size) {
    for (uint32_t i = 0U; i < size; ++i) {
        dst[i] = src[i];
    }
}

/* Comparação exata de duas strings de texto */
static int str_eq(const char *a, const char *b) {
    while (*a != 0 && *b != 0) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* Retorna a quantidade de caracteres em uma string */
static uint32_t str_len(const char *s) {
    uint32_t n = 0U;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

/* Copia uma string limitando ao número máximo de caracteres informados */
static void str_copy_limit(char *dst, const char *src, uint32_t limit) {
    uint32_t i = 0U;

    if (limit == 0U) {
        return;
    }

    while (src[i] != 0 && i + 1U < limit) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

/* Copia o nome de uma entrada TAR para o nó VFS respeitando o limite VFS_NAME_MAX */
static void tar_name_copy(char dst[VFS_NAME_MAX], const char *src, uint32_t src_limit) {
    uint32_t i = 0U;

    while (i < src_limit && src[i] != 0 && i + 1U < VFS_NAME_MAX) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

/* Converte uma representação em texto Octal (usada nos cabeçalhos TAR) para um inteiro de 32 bits */
static uint32_t oct_to_u32(const char *oct, uint32_t len) {
    uint32_t out = 0U;

    for (uint32_t i = 0U; i < len; ++i) {
        char c = oct[i];

        if (c == 0 || c == ' ') {
            break;
        }

        if (c < '0' || c > '7') {
            break;
        }

        out = (out << 3) + (uint32_t)(c - '0');
    }

    return out;
}

/* Verifica se um bloco de 512 bytes contém apenas zeros (marcação de fim do arquivo TAR) */
static uint8_t tar_is_zero_block(const uint8_t *blk) {
    for (uint32_t i = 0U; i < TAR_BLOCK_SIZE; ++i) {
        if (blk[i] != 0U) {
            return 0U;
        }
    }
    return 1U;
}

/* Extrai o próximo componente delimitado por barra ('/') de uma string de caminho */
static int path_next_component(const char **cursor, char component[VFS_NAME_MAX]) {
    const char *p;
    uint32_t length = 0U;

    if (cursor == 0 || *cursor == 0 || component == 0) {
        return -1;
    }

    p = *cursor;
    while (*p == '/') {
        p++;
    }

    if (*p == 0) {
        *cursor = p;
        component[0] = 0;
        return 0;
    }

    while (*p != 0 && *p != '/') {
        if (length + 1U >= VFS_NAME_MAX) {
            return -1;
        }
        component[length++] = *p++;
    }

    component[length] = 0;
    *cursor = p;
    return 1;
}

/* Recupera o ponteiro da estrutura ramfs_entry_t a partir do nó VFS (fs_node_t) */
static struct ramfs_entry *ramfs_entry_from_node(fs_node_t *node) {
    if (node == 0 || node->device == 0) {
        return 0;
    }
    return (struct ramfs_entry *)node->device;
}

/* Busca um nó filho com um nome específico dentro da lista de um nó diretório pai */
static struct ramfs_entry *ramfs_find_child_entry(struct ramfs_entry *parent, const char *name) {
    struct ramfs_entry *cur;

    if (parent == 0 || name == 0) {
        return 0;
    }

    cur = parent->first_child;
    while (cur != 0) {
        if (str_eq(cur->node.name, name)) {
            return cur;
        }
        cur = cur->next_sibling;
    }

    return 0;
}

/* Insere um novo nó filho no final da lista encadeada do nó diretório pai */
static void ramfs_link_child(struct ramfs_entry *parent, struct ramfs_entry *child) {
    child->parent = parent;

    if (parent->last_child == 0) {
        parent->first_child = child;
        parent->last_child = child;
        return;
    }

    parent->last_child->next_sibling = child;
    parent->last_child = child;
}

/* Remove a referência de um nó filho da lista encadeada do nó pai */
static int ramfs_unlink_child(
    struct ramfs_entry *parent,
    struct ramfs_entry *child
) {
    struct ramfs_entry *cur;
    struct ramfs_entry *prev = 0;

    if (parent == 0 || child == 0) {
        return -1;
    }

    cur = parent->first_child;
    while (cur != 0 && cur != child) {
        prev = cur;
        cur = cur->next_sibling;
    }

    if (cur == 0) {
        return -1;
    }

    if (prev == 0) {
        parent->first_child = child->next_sibling;
    } else {
        prev->next_sibling = child->next_sibling;
    }

    if (parent->last_child == child) {
        parent->last_child = prev;
    }

    child->parent = 0;
    child->next_sibling = 0;
    return 0;
}

/* Callback de abertura/fechamento nulo (no-op) para nós do RAMFS */
static void ramfs_noop(fs_node_t *node) {
    (void)node;
}

/* Aloca e inicializa um novo nó (arquivo ou diretório) na estrutura hierárquica em memória */
static struct ramfs_entry *ramfs_create_entry(
    struct ramfs_entry *parent,
    const char *name,
    uint32_t flags,
    uintptr_t data_ptr,
    uint32_t size,
    uint32_t capacity,
    uint8_t writable
) {
    struct ramfs_entry *entry;
    struct ramfs_entry *existing;
    uint32_t type = flags & (FS_FILE | FS_DIRECTORY);

    if (parent == 0 || name == 0 || *name == 0 ||
        str_len(name) >= VFS_NAME_MAX ||
        type == 0U || type == (FS_FILE | FS_DIRECTORY)) {
        return 0;
    }

    for (uint32_t i = 0U; name[i] != 0; ++i) {
        if (name[i] == '/') {
            return 0;
        }
    }

    existing = ramfs_find_child_entry(parent, name);
    if (existing != 0) {
        if ((existing->node.flags & type) == 0U) {
            return 0;
        }
        return existing;
    }

    entry = (struct ramfs_entry *)kmalloc((uint32_t)sizeof(struct ramfs_entry));
    if (entry == 0) {
        return 0;
    }

    mem_zero((uint8_t *)entry, (uint32_t)sizeof(struct ramfs_entry));
    str_copy_limit(entry->node.name, name, sizeof(entry->node.name));

    entry->node.size = size;
    entry->node.open = ramfs_noop;
    entry->node.close = ramfs_noop;
    entry->node.device = entry;

    if (type == FS_DIRECTORY) {
        entry->node.flags = FS_DIRECTORY;
        entry->node.readdir = ramfs_readdir;
        entry->node.finddir = ramfs_finddir;
        entry->node.create = ramfs_create;
        entry->node.remove = ramfs_remove;
    } else {
        entry->node.flags = FS_FILE | FS_SEEKABLE | (writable ? FS_WRITABLE : 0U);
        entry->node.read = ramfs_read;
        entry->node.write = ramfs_write;
        entry->data_ptr = data_ptr;
        entry->capacity = capacity;
        entry->writable = writable;
    }

    ramfs_link_child(parent, entry);
    return entry;
}

/* Realiza a leitura de dados do buffer em RAM do nó de arquivo */
static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    struct ramfs_entry *entry;

    if (node == 0 || buffer == 0 || node->device == 0) {
        return 0U;
    }

    if ((node->flags & FS_FILE) == 0U || offset >= node->size) {
        return 0U;
    }

    if (offset + size < offset) {
        return 0U;
    }

    if (offset + size > node->size) {
        size = node->size - offset;
    }

    entry = (struct ramfs_entry *)node->device;

    for (uint32_t i = 0U; i < size; ++i) {
        buffer[i] = *((uint8_t *)(entry->data_ptr + offset + i));
    }

    return size;
}

/* Escreve dados no nó de arquivo, expandindo dinamicamente o buffer via kmalloc caso necessário */
static uint32_t ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    struct ramfs_entry *entry;
    uint32_t need;

    if (node == 0 || buffer == 0 || node->device == 0) {
        return 0U;
    }

    if ((node->flags & FS_FILE) == 0U) {
        return 0U;
    }

    entry = (struct ramfs_entry *)node->device;
    if (entry->writable == 0U) {
        return 0U;
    }

    if (size > 0xFFFFFFFFU - offset) {
        return 0U;
    }

    need = offset + size;

    /* Expande a capacidade do buffer em potência de 2 se a área atual for insuficiente */
    if (need > entry->capacity) {
        uint32_t new_capacity = entry->capacity;
        uint8_t *new_data;

        if (new_capacity == 0U) {
            new_capacity = RAMFS_NEW_FILE_CAPACITY;
        }

        while (new_capacity < need) {
            if (new_capacity > 0x7FFFFFFFU) {
                return 0U;
            }
            new_capacity <<= 1;
        }

        new_data = (uint8_t *)kmalloc(new_capacity);
        if (new_data == 0) {
            return 0U;
        }

        mem_zero(new_data, new_capacity);

        if (node->size > 0U) {
            mem_copy(new_data, (const uint8_t *)(uintptr_t)entry->data_ptr, node->size);
        }

        if (entry->data_ptr != 0U) {
            kfree((void *)(uintptr_t)entry->data_ptr);
        }

        entry->data_ptr = (uintptr_t)new_data;
        entry->capacity = new_capacity;
    }

    mem_copy((uint8_t *)(uintptr_t)(entry->data_ptr + offset), buffer, size);

    if (need > node->size) {
        node->size = need;
    }

    return size;
}

/* Retorna a entrada N do diretório acessado */
static fs_node_t *ramfs_readdir(fs_node_t *node, uint32_t index) {
    struct ramfs_entry *entry;
    struct ramfs_entry *cur;
    uint32_t current = 0U;

    if (node == 0 || (node->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    entry = ramfs_entry_from_node(node);
    if (entry == 0) {
        return 0;
    }

    cur = entry->first_child;
    while (cur != 0) {
        if (current == index) {
            return &cur->node;
        }
        current++;
        cur = cur->next_sibling;
    }

    return 0;
}

/* Localiza um nó filho com determinado nome em um nó diretório */
static fs_node_t *ramfs_finddir(fs_node_t *node, const char *name) {
    struct ramfs_entry *entry;
    struct ramfs_entry *child;

    if (node == 0 || name == 0 || (node->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    entry = ramfs_entry_from_node(node);
    if (entry == 0) {
        return 0;
    }

    child = ramfs_find_child_entry(entry, name);
    return child != 0 ? &child->node : 0;
}

/* Callback do VFS para criação de novos arquivos/diretórios no RAMFS */
static fs_node_t *ramfs_create(fs_node_t *node, const char *name, uint32_t flags) {
    struct ramfs_entry *parent;
    struct ramfs_entry *entry;
    uint32_t type = flags & (FS_FILE | FS_DIRECTORY);

    if (node == 0 || name == 0 || (node->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    parent = ramfs_entry_from_node(node);
    if (parent == 0) {
        return 0;
    }

    if (type == FS_DIRECTORY) {
        entry = ramfs_create_entry(parent, name, FS_DIRECTORY, 0U, 0U, 0U, 0U);
    } else if (type == FS_FILE) {
        uint8_t *data = (uint8_t *)kmalloc(RAMFS_NEW_FILE_CAPACITY);

        if (data == 0) {
            return 0;
        }

        mem_zero(data, RAMFS_NEW_FILE_CAPACITY);

        entry = ramfs_create_entry(
            parent,
            name,
            FS_FILE | FS_WRITABLE,
            (uintptr_t)data,
            0U,
            RAMFS_NEW_FILE_CAPACITY,
            1U
        );

        if (entry == 0) {
            kfree(data);
        }
    } else {
        return 0;
    }

    return entry != 0 ? &entry->node : 0;
}

/* Callback do VFS para remoção de um nó do RAMFS */
static int ramfs_remove(fs_node_t *node, const char *name) {
    struct ramfs_entry *parent;
    struct ramfs_entry *child;

    if (node == 0 || name == 0 ||
        (node->flags & FS_DIRECTORY) == 0U) {
        return -1;
    }

    parent = ramfs_entry_from_node(node);
    if (parent == 0) {
        return -1;
    }

    child = ramfs_find_child_entry(parent, name);
    if (child == 0) {
        return -2;
    }

    /* Impede a remoção de diretórios não vazios */
    if ((child->node.flags & FS_DIRECTORY) != 0U &&
        child->first_child != 0) {
        return -3;
    }

    if (ramfs_unlink_child(parent, child) != 0) {
        return -4;
    }

    if (child->writable != 0U &&
        child->data_ptr != 0U) {
        kfree((void *)(uintptr_t)child->data_ptr);
    }

    kfree(child);
    return 0;
}

/* Constrói recursivamente a estrutura de diretórios e arquivos a partir dos caminhos do TAR */
static int ramfs_add_tar_path(const char *path, uint8_t directory, uintptr_t data_ptr, uint32_t size) {
    struct ramfs_entry *parent = &ramfs_root_entry;
    const char *cursor = path;
    char component[VFS_NAME_MAX];

    for (;;) {
        const char *after;
        struct ramfs_entry *child;
        int result = path_next_component(&cursor, component);

        if (result < 0) {
            return -1;
        }
        if (result == 0) {
            return 0;
        }
        if (str_eq(component, ".")) {
            continue;
        }
        if (str_eq(component, "..")) {
            return -1;
        }

        after = cursor;
        while (*after == '/') {
            after++;
        }

        child = ramfs_find_child_entry(parent, component);

        if (*after != 0) {
            if (child == 0) {
                child = ramfs_create_entry(parent, component, FS_DIRECTORY, 0U, 0U, 0U, 0U);
            }

            if (child == 0 || (child->node.flags & FS_DIRECTORY) == 0U) {
                return -1;
            }

            parent = child;
            continue;
        }

        if (child != 0) {
            return 0;
        }

        child = ramfs_create_entry(
            parent,
            component,
            directory ? FS_DIRECTORY : FS_FILE,
            directory ? 0U : data_ptr,
            directory ? 0U : size,
            directory ? 0U : size,
            0U
        );

        return child != 0 ? 0 : -1;
    }
}

/* Cria os diretórios raiz padrão da hierarquia de sistemas UNIX no VFS */
static void ramfs_create_standard_root(void) {
    static const char *const dirs[] = {
        "/bin",
        "/dev",
        "/proc",
        "/etc",
        "/lib",
        "/tmp"
    };

    for (uint32_t i = 0U; i < (uint32_t)(sizeof(dirs) / sizeof(dirs[0])); ++i) {
        (void)vfs_create(dirs[i], FS_DIRECTORY);
    }
}

/**
 * Inicializa o RAMFS, define o nó raiz ("/") no VFS, monta a estrutura de diretórios padrão
 * e analisa (parse) a imagem TAR em memória delimitada por 'start' e 'end'.
 */
void init_ramfs(uintptr_t start, uintptr_t end) {
    uintptr_t p = start;

    mem_zero((uint8_t *)&ramfs_root_entry, (uint32_t)sizeof(ramfs_root_entry));
    str_copy_limit(ramfs_root_entry.node.name, "/", sizeof(ramfs_root_entry.node.name));

    ramfs_root_entry.node.flags = FS_DIRECTORY;
    ramfs_root_entry.node.readdir = ramfs_readdir;
    ramfs_root_entry.node.finddir = ramfs_finddir;
    ramfs_root_entry.node.create = ramfs_create;
    ramfs_root_entry.node.remove = ramfs_remove;
    ramfs_root_entry.node.open = ramfs_noop;
    ramfs_root_entry.node.close = ramfs_noop;
    ramfs_root_entry.node.device = &ramfs_root_entry;

    vfs_set_root(&ramfs_root_entry.node);
    ramfs_create_standard_root();

    /* Varre a imagem TAR pré-carregada e extrai os cabeçalhos de arquivo */
    while (p + TAR_BLOCK_SIZE <= end) {
        struct tar_header *hdr = (struct tar_header *)(uintptr_t)p;
        uint32_t file_size;
        uint32_t blocks;
        char path[VFS_NAME_MAX];

        if (tar_is_zero_block((const uint8_t *)hdr)) {
            break;
        }

        file_size = oct_to_u32(hdr->size, sizeof(hdr->size));
        blocks = (file_size + TAR_BLOCK_SIZE - 1U) / TAR_BLOCK_SIZE;
        tar_name_copy(path, hdr->name, sizeof(hdr->name));

        if (path[0] != 0) {
            (void)ramfs_add_tar_path(
                path,
                (uint8_t)(hdr->typeflag == '5'),
                p + TAR_BLOCK_SIZE,
                file_size
            );
        }

        p += TAR_BLOCK_SIZE + ((uintptr_t)blocks * TAR_BLOCK_SIZE);
    }
}

/* Retorna o nó VFS (fs_node_t) do diretório raiz do RAMFS */
fs_node_t *ramfs_root(void) {
    return &ramfs_root_entry.node;
}

/* Resolve um caminho no VFS a partir do RAMFS */
fs_node_t *ramfs_find(const char *path) {
    return vfs_resolve(path);
}

/* Cria ou abre um arquivo gravável no RAMFS no caminho especificado */
fs_node_t *ramfs_touch(const char *path) {
    return vfs_create(path, FS_FILE | FS_WRITABLE);
}

/* Cria um novo nó diretório no caminho especificado */
fs_node_t *ramfs_mkdir(const char *path) {
    return vfs_create(path, FS_DIRECTORY);
}