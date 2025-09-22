/*-----------------------------------------------------
   SVC vector (minimal save of SPSR+LR only)
------------------------------------------------------*/
    .extern svc_handler         /* existing C handler */
    .extern svc_switch_from     /* defined in C */
    .extern svc_switch_to

.global svc
.type svc, %function
.global irq
.type irq, %function


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
.type reset, %function

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

    msr cpsr_c,#0xDF        /* SYS mode */
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

/*svc:
    stmfd sp!, {r0-r12, lr}      @ save regs and LR
    mrs r0, spsr                  @ save SPSR
    stmfd sp!, {r0}

    bl svc_handler                 @ call C function

    ldmfd sp!, {r0}               @ restore SPSR
    msr spsr_cxsf, r0
    ldmfd sp!, {r0-r12, lr}
    movs pc, lr                   @ return, restores CPSR from SPSR*/


svc:
    /* --- Save SPSR + SVC LR on SVC stack --- */
    mrs r0, spsr
    stmfd sp!, {r0, lr}        /* save SPSR and SVC LR */

    bl svc_handler             /* sets svc_switch_from / svc_switch_to */

    ldr r1, =svc_switch_to
    ldr r1, [r1]
    //cmp r1, #0
    //beq 1f                     /* no switch requested */

    ldr r0, =svc_switch_from
    ldr r0, [r0]

    /* --- Switch to System mode to access banked regs --- */
    mrs r2, cpsr
    bic r2, r2, #0x1F
    orr r2, r2, #0x1F         /* SYS mode */
    orr r2, r2,#0x80          /* disable interrupts */
    msr cpsr_c, r2

    add r3, r0, #12
    stmia r3, {r4-r11}         /* current->regs */
    str sp, [r0, #8]           /* current->sp */
    str lr, [r0, #44]          /* current->lr */
    mrs r2, cpsr
    str r2, [r0, #48]          /* current->cpsr */

    /* --- Restore next task context --- */
    add r3, r1, #12
    ldmia r3, {r4-r11}         /* next->regs */
    ldr sp, [r1, #8]           /* next->sp */
    ldr lr, [r1, #44]          /* next->lr */
    ldr r2, [r1, #48]          /* load next CPSR */
    msr cpsr_cxsf, r2          /* restore CPSR */

    /* --- Return to next task in SYS mode --- */
    bx lr

//1:  /* No switch requested: restore old SPSR + LR */
//    ldmfd sp!, {r0, lr}
//    msr spsr_cxsf, r0
//    movs pc, lr

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