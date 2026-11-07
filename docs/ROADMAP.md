# Cruzeiro OS — Roadmap

> Planejamento aprovado na reunião técnica do COCP de **7 de novembro de 2026**.
>
> **Status:** roadmap orientador, sujeito a revisão conforme resultados técnicos. Os números de PRs são **marcos de referência**, não datas de entrega nem condições automáticas de lançamento. A conclusão antecipada é desejável.

## 1. Diretrizes e prioridades

1. **Prioridade P0 — Userspace x86 (32 bits):** consolidar a ABI, a CLibC mínima, o processo de inicialização e um shell executado em Ring 3.
2. **Prioridade P1 — Estabilidade e homologação:** testes de isolamento, processos, memória, syscalls, VFS/ELF e builds reprodutíveis. Não trocar segurança e robustez por quantidade de PRs.
3. **Prioridade P2 — Portabilidade:** organizar dois forks/frentes para **x86-64** e **AArch64**, sem desviar recursos críticos do userspace principal. A árvore x86-64 do repositório principal continua congelada enquanto o modelo de processos e a ABI não estiverem estabilizados.
4. **Prioridade P3 — Ferramentas nativas e ambiente de sistema:** compiladores, ferramentas de desenvolvimento, interface gráfica e compatibilidade Linux.

A entrega do **Cruzeiro OS 1.0.0** dependerá de critérios objetivos de qualidade, **não** de atingir o PR #150. Poderá ser antecipada se os requisitos forem cumpridos.

## 2. Estado inicial (baseline em 2026-11-07)

O target suportado permanece **CZK_x86 (32 bits)**. O repositório já possui:

- Inicialização GRUB/Multiboot2; GDT, IDT, ISR, IRQ, PIC, PIT e teclado PS/2.
- Console VGA e serial; logging, panic e monitor de diagnóstico Ring 0.
- PMM, VMM, paginação, heap, espaços de endereçamento privados e proteção de memória.
- Scheduler preemptivo, processos/tarefas, TSS, gerenciamento lazy de FPU e transições Ring 0/Ring 3.
- ABI de syscalls via `int 0x80`, validação de ponteiros de usuário e gerenciamento básico de FDs.
- VFS hierárquico, RAMFS, montagens, devfs básico (`/dev/null`, `/dev/zero`, `/dev/tty1`) e TTY.
- Carregador ELF32 e programas de teste `/bin/hello` e `/bin/execprobe`.

**Atenção:** implementação existente não significa homologação completa. O `crt0` usado nos testes ELF não equivale ainda a uma CLibC independente. O shell atual é um **monitor Ring 0**, não `/bin/sh`.

## 3. Marcos de entrega

| Referência | Meta | Critério mínimo de aceite |
| --- | --- | --- |
| **Próximos PRs — P0** | CLibC v0.1, userspace independente e shell Ring 3 | `/bin/init` inicia `/bin/sh`; entrada/saída via TTY e syscalls; regressões de memória/processo passam |
| **Até ~PR #75** | **Tiny C Compiler (TCC) nativo** | TCC executa no Cruzeiro OS e compila, liga e executa um programa C simples, sem recorrer ao compilador do host durante esse teste |
| **Até ~PR #100** | **GCC nativo e ambiente gráfico mínimo** | Definir versão/escopo do GCC; compilação C mínima validada no OS; compositor ou servidor gráfico mínimo demonstrável (avaliar Nano-X/Microwindows ou alternativa) |
| **Em torno do PR #100** | **Compatibilidade com executáveis Linux: fase inicial** | Protótipo documentado de conversão/adaptação de ELF e/ou camada de compatibilidade de ABI; demonstrar ao menos um binário-alvo e explicitar limites |
| **Até ~PR #150, antecipável** | **Candidato a Cruzeiro OS 1.0.0** | Gates de release cumpridos; decisão explícita de publicação; versão não vinculada ao número de PR nem ao número de build |

As referências `#75`, `#100` e `#150` pressupõem numeração de pull requests no repositório principal e podem ser reajustadas conforme a cadência real de contribuições.

### 3.1 Dependências críticas antes do TCC

- **CLibC v0.1:** `crt0`, convenções de chamada, wrappers de syscalls, `errno`, memória/string, I/O e interfaces de processos.
- **Processos:** criação/execução de novos processos, `wait`, término e propagação de erros; decidir a interface de criação compatível com o modelo do kernel.
- **Memória userspace:** crescimento/alocação de heap (`brk` ou mecanismo equivalente) e allocator.
- **Filesystem:** leitura/escrita de arquivos, diretórios, caminhos e FDs suficientes para fonte, objetos e saída do linker; persistência pode ser incremental, mas os fluxos de compilação precisam ser reproduzíveis.
- **Toolchain:** definir assembler, linker, formato ELF, headers, bibliotecas e estratégia inicial de bootstrap/cross-compilation.
- **Testes:** compilação e execução de programas de exemplo de forma automatizada em QEMU.

### 3.2 GCC e interface gráfica

O **GCC nativo** é meta de maior complexidade que o TCC: depende de runtime, bibliotecas, utilitários, toolchain e infraestrutura de arquivos madura. Registrar a versão-alvo e os recursos de linguagem suportados antes de declarar a meta concluída.

Para a GUI, avaliar **Nano-X/Microwindows** ou solução equivalente. A primeira demonstração pode usar framebuffer e drivers mínimos; interface gráfica não deve bloquear os fundamentos do userspace.

### 3.3 Executáveis Linux: duas frentes, não uma promessa de compatibilidade geral

1. **Conversor/adaptador ELF Linux → Cruzeiro:** investigar ajustes de ELF, linking e dependências para casos específicos.
2. **Camada de compatibilidade estilo Wine (conceitualmente):** mapear syscalls, ABI, runtime, loaders e bibliotecas de programas Linux para serviços do Cruzeiro OS.

**ELF é formato de executável, não garantia de compatibilidade de ABI.** Alterar o cabeçalho de um ELF não traduz automaticamente syscalls, dependências, TLS, sinais ou loader dinâmico. Iniciar por **Linux i386**, com um conjunto pequeno e explícito de programas. O marco próximo ao PR #100 significa **protótipo inicial**, não compatibilidade Linux completa.

## 4. Estratégia multi-arquitetura

### Principal: x86 (32 bits)

- Segue como plataforma de referência para CLibC, userspace, compiladores e aceitação de releases.
- Manter o monitor Ring 0 para diagnóstico, separado do shell de usuário.

### Fork x86-64

- **Decidido:** criar fork/frente de desenvolvimento específica.
- **Situação:** criação e implementação ainda pendentes; não confundir decisão de organização com port funcional.
- Reaproveitar abstrações genéricas após a estabilização de ABI, memória e processos. A árvore experimental x86-64 da main continua congelada por enquanto.

### Fork AArch64 (ARM64)

- **Decidido:** criar fork/frente experimental com prioridade secundária.
- Etapa A: interfaces independentes de arquitetura, sem atrasar o x86.
- Etapa B: boot, exceções, IRQ, timer e memória no **QEMU `virt`**.
- Etapa C: testes em **Proxmox sobre host ARM64** compatível com virtualização/KVM.
- Etapa D: validação em **hardware físico ARM64 escolhido**, com drivers específicos conforme a plataforma.

**Não presumir** que drivers do QEMU `virt` funcionem em uma placa ARM64 real. Os forks são **planejados, ainda não criados por este documento**.

## 5. Infraestrutura e sistemas de arquivos

Evoluir em paralelo, respeitando prioridades:

- `procfs`, ampliação de `devfs` e dispositivos de sistema.
- Entropia real/CSPRNG para `/dev/random` e `/dev/urandom` — evitar PRNG de teste como implementação final.
- Relógio monotônico e de tempo real.
- Block layer, drivers de armazenamento e filesystem persistente.
- Testes de integridade de arquivos, falhas de I/O, memória e recuperação.

## 6. Critérios candidatos para 1.0.0

A versão 1.0.0 exige uma **decisão explícita de release** após evidências de:

1. Boot reproduzível no x86 suportado e CI verde.
2. Isolamento e estabilidade de memória/processos sob testes de regressão.
3. `/bin/init`, shell Ring 3 e CLibC documentados e operacionais.
4. Execução e compilação nativa de C conforme escopo homologado.
5. I/O, arquivos e comportamento de erros documentados e testados.
6. Procedimento de instalação/execução, limitações conhecidas e notas de release.
7. Critérios de segurança, qualidade e compatibilidade revisados.

GCC, GUI, persistência e compatibilidade Linux terão critérios de aceitação próprios definidos antes de serem considerados bloqueadores obrigatórios de 1.0.0. **Nenhum PR, isoladamente, implica lançamento automático.**

## 7. Próximas ações

1. Especificar CLibC v0.1 e a ABI mínima necessária ao `/bin/init` e `/bin/sh`.
2. Abrir issues/PRs por capacidade, com testes e dependências explícitos.
3. Definir contrato de bootstrap e o conjunto de testes que atestará o TCC nativo.
4. Planejar os dois forks e responsáveis, sem interromper o desenvolvimento x86.
5. Atualizar este roadmap com evidências de implementação e homologação a cada marco.

---

**Registro de decisões (COCP, 2026-11-07)**

- **D01 — Aprovada:** userspace x86 como prioridade.
- **D02 — Aprovada:** frente experimental AArch64, com implantação gradual.
- **D03 — Diretrizes de roadmap definidas em reunião:** dois forks (x86-64 e AArch64); TCC até a referência PR #75; GCC e GUI mínima até ~PR #100; protótipo de compatibilidade Linux próximo a ~PR #100; 1.0.0 referenciada em ~PR #150, com antecipação permitida.
