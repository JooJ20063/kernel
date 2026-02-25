# 📑 Guia de Desenvolvimento e Ciclo de Vida do Sistema

Este documento define os procedimentos para compilação, teste e integração contínua (CI), garantindo que o Kernel mantenha a sua integridade e padrões de segurança durante o desenvolvimento.

---

## 1. Ambiente de Compilação (Toolchain)

O Kernel é desenvolvido num ambiente *freestanding*, o que significa que não depende de nenhuma biblioteca padrão do sistema operativo hospedeiro.

### Pré-requisitos
* **Compilador:** `gcc` com suporte a `-m32` (cross-compiler `i686-elf-gcc` é recomendado).
* **Assembler:** `binutils` (GAS - GNU Assembler).
* **Boot:** `grub-mkrescue` e `xorriso` para geração de imagens ISO.
* **Emulação:** `qemu-system-i386` para testes de runtime.

---

## 2. Automação de Tarefas (Makefile)

O `Makefile` é o centro da nossa automação. Ele gere a árvore de objetos separada do código fonte para manter o repositório limpo.

| Comando | Descrição | Requisito de Segurança |
| :--- | :--- | :--- |
| `make` | Compila os módulos C e ASM em objetos `.o`. | Garante que o código respeita o padrão `-m32`. |
| `make check` | Executa testes estáticos e verificações de integridade. | Deteta erros de sintaxe e lógica pré-boot. |
| `make iso` | Gera a imagem bootável `kernel.iso`. | Cria a estrutura Multiboot 2 válida. |
| `make run` | Inicia a emulação no QEMU. | Permite auditoria de logs em tempo real. |
| `make clean` | Remove todos os ficheiros da pasta `build/`. | Garante builds limpos e sem artefatos obsoletos. |

---

## 3. Convenções de Engenharia

Para garantir que o kernel seja "imbatível", seguimos padrões rígidos de organização:

### Estrutura de Diretórios
* **`/arch/x86`**: Camada de Abstração de Hardware (HAL).
* **`/kernel`**: Núcleo agnóstico (Gestão de memória, escalonamento).
* **`/include`**: Contratos e definições de tipos (ex: `stdint.h`).
* **`/boot`**: Código de inicialização em baixo nível (Assembly).
* **`/build`**: (Ignorado pelo Git) Contém os binários intermédios.

### Flags de Compilação Críticas
* `-ffreestanding`: Garante que não há inclusão de bibliotecas padrão do host.
* `-fno-stack-protector`: Atualmente desativado para permitir a implementação manual de *Stack Canaries* no futuro.
* `-Wall -Wextra`: Tratamento rigoroso de avisos como erros potenciais.



---

## 4. Integração Contínua (CI)

Utilizamos **GitHub Actions** para validar cada alteração de código. O ficheiro de configuração encontra-se em `.github/workflows/build.yml`.

### Etapas do Pipeline:
1.  **Setup:** Instalação do `gcc-multilib` e ferramentas de build x86.
2.  **Compilation:** Execução do `make` para validar que o kernel compila sem erros.
3.  **Validation:** Execução do `make check` para garantir que as funções base (como o PMM) funcionam conforme esperado.
4.  **Artifacts:** Armazenamento do `kernel.bin` para análise posterior de símbolos e debug.

---

## 5. Fluxo de Trabalho Recomendado

1.  **Modificação:** Alterar o código num módulo específico (ex: `pmm.c`).
2.  **Documentação:** Se a lógica de hardware mudar, atualizar o `ARCHITECTURE.md`.
3.  **Verificação:** Executar `make clean && make run` para garantir que o sistema faz boot e o Shell responde.
4.  **Push:** Enviar para o repositório apenas ficheiros fonte; a CI encarrega-se da validação final.
