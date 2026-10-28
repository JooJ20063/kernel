/* Ponto de entrada do kernel (símbolo onde a execução começa após o bootloader) */
ENTRY(_start)

SECTIONS
{
    /* Define o endereço de carga inicial no primeiro megabyte (1MB - padrão para boot via Multiboot/GRUB) */
    . = 1M;
    _kernel_start = .;

    /* Seção de código executável do Kernel (alinhada em página de 4KB) */
    .text ALIGN(4K) : {
        _text_start = .;
        *(.multiboot) /* Cabeçalho Multiboot posicionado no início para ser encontrado pelo GRUB */
        *(.text*)     /* Todas as seções de código de todos os arquivos objeto */
        _text_end = .;
    }

    /* Seção de dados somente leitura (constantes, strings literais) */
    .rodata ALIGN(4K) : {
        _rodata_start = .;
        *(.rodata*)
        _rodata_end = .;
    }

    /* Código executável isolado para tarefas/programas em User Mode (Ring 3) */
    .usertext ALIGN(4K) : {
        _user_text_start = .;
        *(.usertext)
        _user_text_end = .;
    }

    /* Dados inicializados de User Mode */
    .userdata ALIGN(4K) : {
        _user_data_start = .;
        *(.userdata)
        _user_data_end = .;
    }

    /* Seção de dados inicializados do Kernel (variáveis globais/estáticas pré-definidas) */
    .data ALIGN(4K) : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    }

    /* Seção de dados não inicializados (variáveis zeradas no boot) */
    .bss ALIGN(4K) : {
        _bss_start = .;
        *(COMMON)   /* Símbolos comuns não inicializados */
        *(.bss*)
        _bss_end = .;
    }

    /* Símbolo que marca o fim absoluto da imagem do kernel na memória RAM */
    _kernel_end = .;
}