MEMORY
{
    vectors (rx) : ORIGIN = 0x00000000, LENGTH = 32
    code    (rx) : ORIGIN = 0x00010000, LENGTH = 32K
    ram     (rwx): ORIGIN = 0x00020000, LENGTH = 32M
}

SECTIONS
{
    /* Vector table at 0x0 */
    .vectors ALIGN(4) : {
        KEEP(*(.vectors))
    } > vectors

    /* Code */
    .text : {
        _text_start = .;
        *(.text*)
        _text_end = .;
    } > code

    /* Read-only data */
    .rodata ALIGN(4) : {
        *(.rodata*)
    } > code

    /* Initialized data */
    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > code

    /* Uninitialized data (BSS) */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > code

    /*------------------------
      Stacks in RAM
    ------------------------*/
    .stacks (NOLOAD) : ALIGN(8) {
        __stack_fiq_top = . + 0x1000; . += 0x1000;
        __stack_irq_top = . + 0x1000; . += 0x1000;
        __stack_svc_top = . + 0x1000; . += 0x1000;
        __stack_abt_top = . + 0x1000; . += 0x1000;
        __stack_und_top = . + 0x1000; . += 0x1000;
        __stack_sys_top = . + 0x1000; . += 0x1000;
    } > ram
}
