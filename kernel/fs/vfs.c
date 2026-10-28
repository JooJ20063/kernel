#include <kernel/vfs.h>

/* Limite máximo de pontos de montagem simultâneos suportados pela tabela interna do VFS */
#define VFS_MAX_MOUNTS 16U

/* Estrutura de mapeamento para pontos de montagem (Mount Points) */
struct vfs_mount {
    fs_node_t *mountpoint; // Nó original do VFS sobre o qual o sistema foi montado
    fs_node_t *root;       // Nó raiz do novo sistema de arquivos montado
};

/* Estado global da tabela de montagem do VFS e nó raiz principal ("/") */
static fs_node_t *vfs_root_node;
static struct vfs_mount vfs_mounts[VFS_MAX_MOUNTS];
static uint32_t vfs_mounts_used;

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

/* Extrai o próximo componente delimitado por barras ('/') na string de caminho */
static int vfs_next_component(
    const char **cursor,
    char component[VFS_NAME_MAX]
) {
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

/**
 * Segue recursivamente a cadeia de pontos de montagem para um dado nó VFS.
 * Previne loops infinitos limitando a resolução ao número máximo de montagens (VFS_MAX_MOUNTS).
 */
static fs_node_t *vfs_follow_mount(fs_node_t *node) {
    uint32_t hops = 0U;

    while (node != 0 && hops < VFS_MAX_MOUNTS) {
        fs_node_t *next = 0;

        for (uint32_t i = 0U; i < VFS_MAX_MOUNTS; ++i) {
            if (vfs_mounts[i].mountpoint == node &&
                vfs_mounts[i].root != 0) {
                next = vfs_mounts[i].root;
                break;
            }
        }

        if (next == 0) {
            return node;
        }

        node = next;
        hops++;
    }

    return hops < VFS_MAX_MOUNTS ? node : 0;
}

/**
 * Resolução interna de caminho (path lookup).
 * Varre componente por componente a partir do nó raiz, resolvendo redirecionamentos de montagem.
 * A flag 'follow_final_mount' indica se o nó final deve retornar a raiz do sistema montado ou o nó de montagem base.
 */
static fs_node_t *vfs_resolve_internal(
    const char *path,
    uint8_t follow_final_mount
) {
    fs_node_t *node = vfs_root_node;
    const char *cursor = path;
    char component[VFS_NAME_MAX];

    if (node == 0 || path == 0 || *path == 0) {
        return 0;
    }

    for (;;) {
        int result = vfs_next_component(&cursor, component);

        if (result < 0) {
            return 0;
        }

        if (result == 0) {
            return follow_final_mount ?
                vfs_follow_mount(node) :
                node;
        }

        if (str_eq(component, ".")) {
            continue;
        }

        if (str_eq(component, "..")) {
            return 0;
        }

        node = vfs_follow_mount(node);
        if (node == 0 ||
            (node->flags & FS_DIRECTORY) == 0U) {
            return 0;
        }

        node = finddir_fs(node, component);
        if (node == 0) {
            return 0;
        }
    }
}

/* Invocadores de abstração da VTable (fs_node_t ops) */

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node == 0 || node->read == 0) {
        return 0;
    }
    return node->read(node, offset, size, buffer);
}

uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    if (node == 0 || node->write == 0) {
        return 0;
    }
    return node->write(node, offset, size, buffer);
}

void open_fs(fs_node_t *node) {
    if (node != 0 && node->open != 0) {
        node->open(node);
    }
}

void close_fs(fs_node_t *node) {
    if (node != 0 && node->close != 0) {
        node->close(node);
    }
}

fs_node_t *readdir_fs(fs_node_t *node, uint32_t index) {
    if (node == 0 || node->readdir == 0) {
        return 0;
    }
    return node->readdir(node, index);
}

fs_node_t *finddir_fs(fs_node_t *node, const char *name) {
    if (node == 0 || name == 0 || node->finddir == 0) {
        return 0;
    }
    return node->finddir(node, name);
}

fs_node_t *create_fs(fs_node_t *node, const char *name, uint32_t flags) {
    if (node == 0 || name == 0 || node->create == 0) {
        return 0;
    }
    return node->create(node, name, flags);
}

int remove_fs(fs_node_t *node, const char *name) {
    if (node == 0 || name == 0 || node->remove == 0) {
        return -1;
    }
    return node->remove(node, name);
}

/* Configura o nó raiz global do VFS ("/") e limpa os pontos de montagem existentes */
void vfs_set_root(fs_node_t *root) {
    vfs_root_node = root;
    vfs_mounts_used = 0U;

    for (uint32_t i = 0U; i < VFS_MAX_MOUNTS; ++i) {
        vfs_mounts[i].mountpoint = 0;
        vfs_mounts[i].root = 0;
    }
}

/* Retorna o nó raiz efetivo do VFS (resolvendo se houver montagem sobre o nó raiz) */
fs_node_t *vfs_root(void) {
    return vfs_follow_mount(vfs_root_node);
}

/* Resolve uma string de caminho completa e retorna o nó VFS correspondente */
fs_node_t *vfs_resolve(const char *path) {
    return vfs_resolve_internal(path, 1U);
}

/* Decompõe um caminho em nó do diretório pai e nome do elemento final */
static int vfs_resolve_parent(
    const char *path,
    fs_node_t **parent_out,
    char name_out[VFS_NAME_MAX]
) {
    fs_node_t *node = vfs_root_node;
    const char *cursor = path;
    char component[VFS_NAME_MAX];

    if (node == 0 || path == 0 || *path == 0 ||
        parent_out == 0 || name_out == 0) {
        return -1;
    }

    for (;;) {
        const char *after;
        int result = vfs_next_component(&cursor, component);

        if (result <= 0) {
            return -1;
        }

        after = cursor;
        while (*after == '/') {
            after++;
        }

        /* Se for o último componente do caminho, extrai o nó pai e o nome do arquivo/diretório */
        if (*after == 0) {
            uint32_t i = 0U;

            if (str_eq(component, ".") || str_eq(component, "..")) {
                return -1;
            }

            node = vfs_follow_mount(node);
            if (node == 0) {
                return -1;
            }

            while (component[i] != 0) {
                name_out[i] = component[i];
                i++;
            }
            name_out[i] = 0;
            *parent_out = node;
            return 0;
        }

        if (str_eq(component, ".")) {
            continue;
        }

        if (str_eq(component, "..")) {
            return -1;
        }

        node = vfs_follow_mount(node);
        if (node == 0 ||
            (node->flags & FS_DIRECTORY) == 0U) {
            return -1;
        }

        node = finddir_fs(node, component);
        if (node == 0) {
            return -1;
        }
    }
}

/* Cria um novo arquivo ou diretório no VFS no caminho especificado caso ainda não exista */
fs_node_t *vfs_create(const char *path, uint32_t flags) {
    fs_node_t *existing;
    fs_node_t *parent;
    char name[VFS_NAME_MAX];
    uint32_t type = flags & (FS_FILE | FS_DIRECTORY);

    if (type == 0U || type == (FS_FILE | FS_DIRECTORY)) {
        return 0;
    }

    /* Se o arquivo/diretório já existir e for do mesmo tipo, retorna a instância ativa */
    existing = vfs_resolve(path);
    if (existing != 0) {
        if ((existing->flags & type) == 0U) {
            return 0;
        }
        return existing;
    }

    if (vfs_resolve_parent(path, &parent, name) != 0) {
        return 0;
    }

    if ((parent->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    return create_fs(parent, name, flags);
}

/* Remove um arquivo ou diretório do VFS no caminho informado */
int vfs_remove(const char *path) {
    fs_node_t *parent;
    fs_node_t *target;
    char name[VFS_NAME_MAX];

    if (vfs_resolve_parent(path, &parent, name) != 0) {
        return -1;
    }

    target = finddir_fs(parent, name);
    if (target == 0) {
        return -2;
    }

    /* Impede a remoção de um nó que atue como ponto de montagem ou raiz de um FS montado */
    for (uint32_t i = 0U; i < VFS_MAX_MOUNTS; ++i) {
        if (vfs_mounts[i].mountpoint == target ||
            vfs_mounts[i].root == target) {
            return -3;
        }
    }

    return remove_fs(parent, name);
}

/**
 * Monta a raiz de um sistema de arquivos ('root') sobre o caminho do diretório 'path'.
 * Registra o vínculo na tabela 'vfs_mounts'.
 */
int vfs_mount(const char *path, fs_node_t *root) {
    fs_node_t *mountpoint;
    uint32_t free_slot = VFS_MAX_MOUNTS;

    if (path == 0 || root == 0 ||
        (root->flags & FS_DIRECTORY) == 0U) {
        return -1;
    }

    mountpoint = vfs_resolve_internal(path, 0U);
    if (mountpoint == 0 ||
        (mountpoint->flags & FS_DIRECTORY) == 0U ||
        mountpoint == root) {
        return -2;
    }

    /* Localiza um slot livre e verifica se o nó já não está atuando como ponto de montagem */
    for (uint32_t i = 0U; i < VFS_MAX_MOUNTS; ++i) {
        if (vfs_mounts[i].mountpoint == mountpoint) {
            return -3;
        }

        if (free_slot == VFS_MAX_MOUNTS &&
            vfs_mounts[i].mountpoint == 0) {
            free_slot = i;
        }
    }

    if (free_slot == VFS_MAX_MOUNTS) {
        return -4; // Tabela de montagem cheia
    }

    vfs_mounts[free_slot].mountpoint = mountpoint;
    vfs_mounts[free_slot].root = root;
    vfs_mounts_used++;

    /* Valida se o encadeamento não causou um ciclo de montagem circular */
    if (vfs_follow_mount(mountpoint) == 0) {
        vfs_mounts[free_slot].mountpoint = 0;
        vfs_mounts[free_slot].root = 0;
        vfs_mounts_used--;
        return -5;
    }

    return 0;
}

/* Desmonta o sistema de arquivos associado ao ponto de montagem em 'path' */
int vfs_unmount(const char *path) {
    fs_node_t *mountpoint;

    if (path == 0) {
        return -1;
    }

    mountpoint = vfs_resolve_internal(path, 0U);
    if (mountpoint == 0) {
        return -2;
    }

    for (uint32_t i = 0U; i < VFS_MAX_MOUNTS; ++i) {
        if (vfs_mounts[i].mountpoint == mountpoint) {
            vfs_mounts[i].mountpoint = 0;
            vfs_mounts[i].root = 0;

            if (vfs_mounts_used > 0U) {
                vfs_mounts_used--;
            }

            return 0;
        }
    }

    return -3;
}

/* Retorna a quantidade de sistemas de arquivos ativos montados no VFS */
uint32_t vfs_mount_count(void) {
    return vfs_mounts_used;
}