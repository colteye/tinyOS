ENTRY(_Reset)

SECTIONS {
  . = 0x10000;
  .text : { *(.text*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  . = ALIGN(8);
  . = . + 0x2000; /* 8096 byte stack */
  stack_top = .;
}
