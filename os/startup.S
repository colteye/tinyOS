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

    .global save_svc_regs
    .type save_svc_regs, %function
/* 
 * void save_svc_regs(uint32_t *task_regs, uint32_t *svc_sp);
 *
 * task_regs -> pointer to buffer for r0–r12, lr (14 words)
 * svc_sp    -> pointer to SPSR on SVC stack
 */
/* 
 * Full SVC context switch: save current task, restore next task.
 * Assumes:
 *   - svc_switch_from  -> current task TCB pointer
 *   - svc_switch_to    -> next task TCB pointer
 *   - TCB layout:
 *       regs[0..12], sp, lr, cpsr
 */

svc:
    push {r0} 
    mrs r0,cpsr
    orr r0,r0,#0x80
    msr cpsr_c,r0
    pop {r0}

    /* --- Save SVC context to stack --- */
    push {r0-r12, lr}       @ push all general-purpose registers + LR
    mrs r0, spsr            @ get SPSR
    push {r0}               @ push SPSR

    bl svc_handler          @ decide if we switch tasks

    /* --- Get current task pointer --- */
    ldr r0, =svc_switch_from  @ r0 = address of pointer to current task
    ldr r0, [r0]              @ r0 = current task TCB pointer

    /* --- Prepare to copy registers --- */
    add r1, r0, #12           @ r1 = &current->regs[0] in TCB
    add r2, sp, #0    @ copy SP into r2
    str r2, [sp]! @ store SP on stack

    /* --- Copy r0-r12 from stack to TCB --- */
    ldr r3, [r2, #56] @ r0
    str r3, [r1], #4
    ldr r3, [r2, #52] @ r1
    str r3, [r1], #4
    ldr r3, [r2, #48] @ r2
    str r3, [r1], #4
    ldr r3, [r2, #44] @ r3
    str r3, [r1], #4
    ldr r3, [r2, #40] @ r4
    str r3, [r1], #4
    ldr r3, [r2, #36] @ r5
    str r3, [r1], #4
    ldr r3, [r2, #32] @ r6
    str r3, [r1], #4
    ldr r3, [r2, #28] @ r7
    str r3, [r1], #4
    ldr r3, [r2, #24] @ r8
    str r3, [r1], #4
    ldr r3, [r2, #20] @ r9
    str r3, [r1], #4
    ldr r3, [r2, #16] @ r10
    str r3, [r1], #4
    ldr r3, [r2, #12] @ r11
    str r3, [r1], #4
    ldr r3, [r2, #8]  @ r12
    str r3, [r1], #4

    push {r0}
    pop {r0-r12, lr}       @ push all general-purpose registers + LR

    ldr     r0, =svc_switch_from
    ldr     r0, [r0]            @ current task TCB
    
    ldr     r1, =svc_switch_to
    ldr     r1, [r1]            @ next task TCB
   // cmp     r1, #0
   // beq     svc_exit_no_switch   @ no switch requested

    /* --- Switch to SYS mode to access banked regs --- */
    mrs     r2, cpsr
    bic     r2, r2, #0x1F
    orr     r2, r2, #0x1F       @ SYS mode
    orr     r2, r2, #0x80       @ disable IRQ
    msr     cpsr_c, r2

    str     sp, [r0, #8]        @ save SP
    str     lr, [r0, #64]       @ save LR
    mrs     r2, cpsr
    str     r2, [r0, #68]       @ save CPSR

    /* --- Restore next task context --- */
    push    {r1}                @ preserve r1
    ldr     r2, [r1, #68]       @ restore CPSR
    msr     cpsr_cxsf, r2       @ apply CPSR

    ldr     r0, [r1, #12]   @ 
    add     r1, r1, #20          @
    ldmia   r1, {r2-r12}          @
    pop     {r1}                 @ 

    ldr     sp, [r1, #8]        @ restore SP
    ldr     lr, [r1, #64]       @ restore LR
    ldr     r1, [r1, #16]   @

    mrs r0,cpsr
    bic r0,r0,#0x80
    msr cpsr_c,r0

    bx      lr                   @ return to next task

//svc_exit_no_switch:
//    ldmfd   sp!, {r0-r12, lr}    @ optional: restore original context if no switch
//    bx      lr


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