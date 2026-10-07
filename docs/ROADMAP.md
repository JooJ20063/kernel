# Roadmap

## 0. Tree v2 e build — concluído

- árvore organizada por responsabilidade;
- x86 definido como target suportado;
- x86-64 isolado como experimental;
- UAPI separada de headers internos;
- build modularizado.

## 1. Ring 3 sólido — concluído para o scaffolding atual

- mappings USER e user stack;
- transições CPL3/CPL0;
- preempção em Ring 3;
- retorno de exceptions e syscalls;
- `copy_from_user`, `copy_to_user` e strings seguras;
- faults recuperáveis de Ring 3 encerram apenas o processo culpado.

O próximo salto de Ring 3 depende do ELF loader e de address spaces privados.

## 2. Syscall ABI v0 — 13 chamadas implementadas

```text
write exit getpid yield getppid sleep wait
open read close lseek fstat readdir
```

Próximas candidatas antes da CLibC completa: `brk` e a chamada de criação/execução de processo que acompanhará o ELF loader.

## 3. Processo e task — process model v1 concluído

Separação atual:

- `process_t`: PID, PPID, nome, exit status, CR3 e FD table;
- `task_t`: TID, contexto de CPU, kernel stack, estado de scheduler e FPU;
- relação atual: 1 processo : 1 task.

## 4. Address spaces privados — próximo bloco

- page directory/CR3 efetivamente privado por processo;
- mapeamento compartilhado do kernel + mappings privados de userspace;
- user stack por processo;
- ownership e destruição das páginas do processo;
- preparação para `brk` e ELF.

A FD table já pertence ao `process_t`.

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
