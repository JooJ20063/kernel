
# 📑 Especificação de Arquitetura de Sistema (x86-32)

Este documento define as diretrizes de interação entre o Kernel e a CPU, estabelecendo os mecanismos de isolamento, proteção de memória e tratamento de eventos críticos. O objetivo primário é a resiliência do sistema e a proteção contra falhas de execução.

---

## 1. Modelo de Privilégios e Segmentação (GDT)

O sistema utiliza o **Flat Memory Model** para simplificar o endereçamento, mas mantém o rigor na separação de privilégios através dos anéis de proteção (*Protection Rings*).

| Seletor | Segmento        | Base         | Limite | Ring | Atributos             |
| :---    | :---            | :---         | :---   | :--- | :---                  |
| `0x08`  | **Kernel Code** | `0x00000000` | 4 GB   | 0    | Exec/Read, Conforming |
| `0x10`  | **Kernel Data** | `0x00000000` | 4 GB   | 0    | Read/Write, Expand-up |
| `0x18`  | **User Code** | `0x00000000` | 4 GB   | 3    | Exec/Read (Roadmap)   |
| `0x20`  | **User Data** | `0x00000000` | 4 GB   | 3    | Read/Write (Roadmap)   |

### Mecanismos de Isolamento:
* **Hierarquia de Rings:** O hardware impede que código executando em Ring 3 utilize instruções privilegiadas (`HLT`, `CLI`, `LIDT`, `IN/OUT`).
* **Transição de Contexto:** (Roadmap) Implementação do **Task State Segment (TSS)** para permitir o *Stack Switching* seguro durante a transição de privilégio (Ring 3 -> Ring 0).



---

## 2. Subsistema de Eventos e Vetores (IDT)

A **Interrupt Descriptor Table (IDT)** atua como o portão de controle do sistema. Todas as entradas são configuradas como **Interrupt Gates (0x8E)**, o que garante que as interrupções sejam desabilitadas automaticamente ao entrar no manipulador (*handler*), prevenindo condições de corrida.

### Classificação de Vetores:
1. **Exceções da CPU (0-31):** Tratamento de falhas de hardware e erros lógicos.
    * **#GP (Vetor 13):** Proteção geral contra acessos a segmentos inválidos ou instruções restritas.
    * **#PF (Vetor 14):** Pilar central para o VMM (Virtual Memory Manager), permitindo a carga de memória sob demanda.
2. **Interrupções de Hardware (32-47):** Remapeadas via **PIC 8259** para evitar conflitos com as exceções nativas da CPU.
3. **Interface de Sistema (0x80):** Vetor reservado para **System Calls**, permitindo que o Ring 3 solicite serviços ao Kernel de forma segura.



---

## 3. Gestão de Memória Física (PMM)

O Kernel utiliza um gerenciador baseado em **Bitmap** para o controle de frames físicos de 4 KiB.

* **Grão de Alocação:** Cada bit no bitmap representa 4 KiB de memória física.
* **Reserva de Segurança:** Os primeiros 1024 KiB (`0x00000000` - `0x000FFFFF`) são marcados como ocupados permanentemente para proteger a IVT, BDA da BIOS e o **Buffer VGA (`0xB8000`)**.
* **Integridade:** O PMM garante que apenas frames alinhados sejam entregues, prevenindo falhas de desalinhamento na ativação da paginação.

---

## 4. Estado da CPU e Registradores de Controle

O Kernel mantém controle estrito sobre o estado da CPU através dos registros de controle:

* **EFLAGS:** O bit IF (Interrupt Flag) é manipulado para proteger seções críticas do Kernel.
* **CR0:** Configurado para habilitar a Proteção de Escrita (**WP Bit**) e, futuramente, a Paginação (**PG Bit**).
* **CR3:** Gerencia o endereço base do *Page Directory* ativo, isolando os espaços de endereçamento virtual.



---

## 5. Roadmap de Hardening (Segurança Ativa)

Para garantir um sistema resiliente contra ataques externos e falhas de usuário, as seguintes tecnologias serão integradas:

1. **WP (Write Protect):** Bloqueia a escrita em páginas Read-Only mesmo para o Kernel (Ring 0), mitigando corrupção acidental de código.
2. **SMEP/SMAP:** Impede que o Kernel execute ou acesse dados em endereços de memória de usuário, mitigando ataques de escalada de privilégio (*Privilege Escalation*).
3. **NX Bit (No-Execute):** (Em Long Mode) Marca páginas de dados (pilha e heap) como não-executáveis para impedir a execução de *payloads* maliciosos.

---
