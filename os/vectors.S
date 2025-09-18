.section .vectors, "a"
.align 7
.global _vectors

_vectors:
    b _Reset                 /* Reset vector */
    b undef_handler          /* Undefined instruction */
    b swi_handler            /* SWI */
    b prefetch_abort_handler /* Prefetch abort */
    b data_abort_handler     /* Data abort */
    b reserved_handler       /* Reserved */
    b irq_handler            /* IRQ */
    b fiq_handler            /* FIQ */
