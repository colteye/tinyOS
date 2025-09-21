/*-----------------------------------------------------
   Vector table at 0x0
------------------------------------------------------*/
.section .vectors, "a"
.global _start
_start:
    b reset         /* 0x00 Reset */
    b hang          /* 0x04 Undefined */
    b svc           /* 0x08 SVC */
    b hang          /* 0x0C Prefetch abort */
    b hang          /* 0x10 Data abort */
    b hang          /* 0x14 Reserved */
    b irq           /* 0x18 IRQ */
    b hang          /* 0x1C FIQ */

/*-----------------------------------------------------
   Reset handler
------------------------------------------------------*/
.section .text
.global reset

/* Linker-provided stack symbols */
.extern __stack_fiq_top
.extern __stack_irq_top
.extern __stack_svc_top
.extern __stack_abt_top
.extern __stack_und_top
.extern __stack_sys_top

reset:
    /*---------------------------------------
      CPU stack setup for different modes
    ---------------------------------------*/
    msr cpsr_c,#0xD1        /* FIQ mode */
    ldr sp,=__stack_fiq_top

    msr cpsr_c,#0xD2        /* IRQ mode */
    ldr sp,=__stack_irq_top

    msr cpsr_c,#0xD3        /* SVC mode */
    ldr sp,=__stack_svc_top

    msr cpsr_c,#0xD7        /* ABT mode */
    ldr sp,=__stack_abt_top

    msr cpsr_c,#0xDB        /* UND mode */
    ldr sp,=__stack_und_top

    msr cpsr_c,#0xDF        /* SYS mode (start here!) */
    ldr sp,=__stack_sys_top

    /* Call main */
    bl main

1:  b 1b

hang:
    b hang

/*-----------------------------------------------------
   IRQ handler
------------------------------------------------------*/
irq:
    push {r0-r12,r14}
    bl irq_handler
    pop {r0-r12,r14}
    subs pc,r14,#4

svc:
    push {r0-r12,r14}
    bl svc_handler
    pop {r0-r12,r14}
    subs pc,r14,#4

/*-----------------------------------------------------
   IRQ helpers
------------------------------------------------------*/
.global interrupt_enable
interrupt_enable:
    mrs r0,cpsr
    bic r0,r0,#0x80
    msr cpsr_c,r0
    bx lr

.global interrupt_disable
interrupt_disable:
    mrs r0,cpsr
    orr r0,r0,#0x80
    msr cpsr_c,r0
    bx lr