# Roadmap

## 0. Tree v2 e build

- reorganizar a árvore por responsabilidade;
- tornar o x86 o target suportado explícito;
- isolar o x86-64 experimental;
- separar UAPI de headers internos;
- modularizar o build.

## 1. Ring 3 sólido

- estabilizar mappings USER;
- user stack;
- transições CPL3/CPL0;
- preempção em Ring 3;
- retorno correto de exceptions e syscalls;
- adicionar `copy_from_user`, `copy_to_user` e validação de ponteiros.

## 2. Syscall ABI v0

Expandir o conjunto inicial para aproximadamente 12–16 chamadas estáveis, incluindo I/O, filesystem, processo e memória.

## 3. Processo e task

Separar o conceito de processo do contexto escalonável:

- processo: PID, address space, FD table e recursos;
- task/thread: contexto de CPU, kernel stack, estado de scheduler e FPU.

Inicialmente pode existir uma task por processo.

## 4. Address spaces e file descriptors

- CR3 por processo;
- userspace privado;
- user stack por processo;
- tabela de descritores.

## 5. ELF loader

- parser ELF;
- `PT_LOAD`;
- mapeamento de segmentos;
- stack inicial;
- entry point;
- execução CPL3 a partir do VFS.

## 6. CLibC v0.1

Projeto userspace separado, consumindo os headers de `uapi/`:

- crt0;
- wrappers de syscall;
- memória/string;
- errno;
- read/write/open/close;
- processo básico.

Depois: `brk`, allocator e stdio.

## 7. Userspace real

- init;
- primeiros executáveis ELF;
- shell Ring 3;
- shell atual preservado como monitor de diagnóstico Ring 0.

## 8. Storage e filesystem persistente

- block layer;
- driver de armazenamento;
- filesystem persistente;
- RAMFS mantido para testes/initramfs.

## 9. Retomada do x86-64

Portar a arquitetura estabilizada de processos, ABI, ELF e userspace para x86-64 em vez de desenvolver dois modelos em paralelo.
