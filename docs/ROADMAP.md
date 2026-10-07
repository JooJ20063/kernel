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

## 4. Address spaces privados — v1 concluído

- page directory/CR3 privado por processo Ring 3;
- mappings supervisor do kernel compartilhando frames físicos;
- `.usertext` e `.userdata` clonados para frames privados;
- user stack privada por processo;
- troca de CR3 no context switch;
- destruição do address space junto do processo;
- sincronização dos mappings de heap do kernel.

A temporary mapping window supervisor-only em `0xFF800000` já permite copiar, ler e zerar frames físicos fora do identity mapping bootstrap. As páginas de userspace podem portanto usar qualquer frame administrado pelo PMM.

Limitação remanescente: page directories/page tables continuam abaixo de 12 MiB até termos recursive paging ou outro mecanismo para manipular estruturas de paginação em frames altos.

## 5. VFS hierárquico e namespace raiz

Antes de cristalizar paths no ELF/userspace:

- resolução componente a componente (`/bin/hello`);
- diretórios reais no RAMFS;
- mount points;
- raiz inicial planejada com `/bin`, `/dev`, `/proc`, `/etc`, `/lib` e `/tmp`;
- preparar `devfs` e `procfs` como filesystems montáveis, sem special-cases em `open()`.

O shell atual permanece monitor Ring 0. O futuro `/bin/sh` será Ring 3 e usará uma TTY através de `/dev/tty1`.

## 6. ELF loader

- parser ELF;
- `PT_LOAD`;
- mapeamento de segmentos;
- stack inicial;
- entry point;
- execução CPL3 a partir do VFS.

## 6.1. Infraestrutura de devices e sistema

Planejamento já definido para a primeira geração de userspace:

- `devfs` com pelo menos `/dev/null`, `/dev/zero`, `/dev/tty1`, `/dev/random` e `/dev/urandom`;
- TTY layer entre teclado/console e os FDs 0/1/2 do shell Ring 3;
- `/dev/random` e `/dev/urandom` apoiados por um subsistema real de entropia + CSPRNG, não por PRNG de teste;
- `procfs` para informações virtuais de processos/kernel;
- timekeeping separado em relógio monotônico e realtime, inicializado a partir do RTC e futuramente disciplinado por rede.

Esses blocos não são pré-requisitos para o primeiro ELF mínimo, mas são parte do caminho crítico para `/bin/init` e `/bin/sh`.

## 7. CLibC v0.1

Projeto userspace separado, consumindo os headers de `uapi/`:

- crt0;
- wrappers de syscall;
- memória/string;
- errno;
- read/write/open/close;
- processo básico.

Depois: `brk`, allocator e stdio.

## 8. Userspace real

- init;
- primeiros executáveis ELF;
- shell Ring 3;
- shell atual preservado como monitor de diagnóstico Ring 0.

## 9. Storage e filesystem persistente

- block layer;
- driver de armazenamento;
- filesystem persistente;
- RAMFS mantido para testes/initramfs.

## 10. Retomada do x86-64

Portar a arquitetura estabilizada de processos, ABI, ELF e userspace para x86-64 em vez de desenvolver dois modelos em paralelo.
