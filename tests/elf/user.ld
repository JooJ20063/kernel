/* Define _start como ponto de entrada do executável ELF. */
ENTRY(_start)

/*
 * Define os segmentos carregáveis descritos pelos program headers.
 * As flags combinam PF_R = 4, PF_W = 2 e PF_X = 1.
 */
PHDRS
{
    /* Código e constantes: leitura e execução. */
    text PT_LOAD FLAGS(5);

    /* Dados inicializados e BSS: leitura e escrita. */
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* Endereço virtual base do executável no espaço de usuário. */
    . = 0x02000000;

    /* Agrupa o código em uma seção alinhada a uma página de 4 KiB. */
    .text ALIGN(0x1000) : {
        *(.text*)
    } :text

    /* Constantes compartilham o segmento de leitura e execução. */
    .rodata : {
        *(.rodata*)
    } :text

    /*
     * Inicia os dados em uma nova página, separando-os do segmento
     * de código para permitir permissões de escrita diferentes.
     */
    . = ALIGN(0x1000);

    /* Dados com valores iniciais armazenados no arquivo ELF. */
    .data : {
        *(.data*)
    } :data

    /*
     * Dados sem inicialização explícita e símbolos COMMON.
     * O carregador deve zerar a parte do segmento não presente no arquivo
     * quando p_memsz for maior que p_filesz.
     */
    .bss : {
        *(.bss*)
        *(COMMON)
    } :data

    /*
     * Descarta comentários, notas e metadados de desenrolamento de pilha
     * que não são utilizados por estes executáveis de teste.
     */
    /DISCARD/ : {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
    }
}