.global _Reset
.extern main
.extern stack_top

_Reset:
    /* Initialize stack */
    LDR sp, =stack_top

    /* Call main() */
    BL main

hang:
    B hang
