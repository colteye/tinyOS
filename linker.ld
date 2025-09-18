ENTRY(_Reset)

SECTIONS
{
    .vectors 0x00000000 : { KEEP(*(.vectors)) }

    . = 0x10000;
    .text : { *(.text*) }
    .data : { *(.data*) }
    .bss  : { *(.bss*) *(COMMON) }

    . = ALIGN(8);
    . = . + 0x2000; /* 8 KB stack */
    stack_top = .;
}
