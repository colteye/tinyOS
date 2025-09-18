/* scheduler.c
 * Preemptive priority scheduler for VersatilePB (ARM926EJ-S).
 *
 * Notes:
 *  - task_switch() is the only place that performs the low-level
 *    register save/restore. It expects pointers to the current and
 *    next task_t in r0 and r1 respectively (ARM calling convention).
 *  - task_create() initializes a minimal stack + PC so the first
 *    context switch into a task will work.
 *  - All inline asm avoids "ldr rX, =symbol" patterns that can
 *    produce OFFSET_IMM relocation errors.
 */

#include "scheduler.h"
#include <stdint.h>
#include <string.h>

#include "uart.h"   // assume uart_puts is implemented

/* --- Hardware-specific definitions --- */
#define SYSTEM_CLOCK 50000000UL   // 50 MHz for VersatilePB
#define TIMER0_BASE  0x101E2000UL // Generic timer base

#define TIMER_LOAD    (*(volatile uint32_t *)(TIMER0_BASE + 0x00))
#define TIMER_VALUE   (*(volatile uint32_t *)(TIMER0_BASE + 0x04))
#define TIMER_CTRL    (*(volatile uint32_t *)(TIMER0_BASE + 0x08))
#define TIMER_CLR_IRQ (*(volatile uint32_t *)(TIMER0_BASE + 0x0C))

#define VIC_BASE       0x10140000
#define VIC_INTENABLE  (*(volatile uint32_t *)(VIC_BASE + 0x10))
#define VIC_VADDR      (*(volatile uint32_t *)(VIC_BASE + 0xF00))
#define TIMER0_IRQ_BIT 4


#define TIMER_ENABLE      (1 << 7)
#define TIMER_IRQ_ENABLE  (1 << 5)
#define TIMER_PERIODIC    (1 << 6)

#define MAX_PRIORITIES (32U)
#define MAX_TASKS      (16U)

/* --- Task state --- */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_STOPPED
} task_state_t;

/* --- Task Control Block --- */
typedef struct task {
    uint32_t *stack;        /* base of stack array (low address) */
    uint32_t stack_size;    /* size in uint32_t words */
    uint32_t *sp;           /* saved SP (stack grows down) */
    uint32_t regs[8];       /* saved r4-r11 */
    uint32_t lr;            /* saved LR */
    uint32_t pc;            /* saved PC (entry on first run) */

    struct task *next;
    uint8_t priority;
    task_state_t state;
    uint32_t wake_tick;
} task_t;

/* --- Scheduler state --- */
typedef struct {
    task_t *ready_head[MAX_PRIORITIES];
    task_t *ready_tail[MAX_PRIORITIES];
    uint32_t ready_bitmap;

    task_t task_pool[MAX_TASKS];
    uint32_t task_count;

    task_t *current;
    uint32_t tick;
} scheduler_t;

static scheduler_t sched;

/* --- Forward declarations --- */
static void ready_dequeue(task_t *t);
static void ready_enqueue(task_t *t);
static task_t *pick_next_task(void);

/* Low-level context switch implemented in assembly (naked). */
__attribute__((naked)) void task_switch(task_t *current, task_t *next);


static void vic_init(void) {
    VIC_INTENABLE |= (1 << TIMER0_IRQ_BIT);  // enable Timer0 IRQ
}

/* --- Helper: timer init --- */
static void timer_init(void) {
    /* 1 ms tick */
    TIMER_LOAD = SYSTEM_CLOCK / 1000 - 1;
    TIMER_CTRL = TIMER_ENABLE | TIMER_IRQ_ENABLE | TIMER_PERIODIC;
}

/* --- Enable IRQs (ARM) --- */
static void enable_interrupts(void) {
    __asm__ volatile(
        "mrs r0, cpsr\n"
        "bic r0, r0, #0x80\n"   /* clear I bit */
        "msr cpsr_c, r0\n"
        ::: "r0"
    );
}


/**
 * @brief Zero-fill memory region.
 *
 * @param dst Pointer to memory to clear.
 * @param n   Number of bytes to clear.
 */
void memzero(void *dst, size_t n) {
    unsigned char *p = dst;
    while (n--) {
        *p++ = 0;
    }
}

/* --- API implementations --- */
void scheduler_init(void) 
{
    memzero(&sched, sizeof(sched));
    vic_init();
    timer_init();
    enable_interrupts();
}

void task_create(void (*func)(void), uint32_t *stack, uint32_t size, uint8_t priority) {
    if (sched.task_count >= MAX_TASKS) return;

    task_t *t = &sched.task_pool[sched.task_count++];

    t->stack = stack;
    t->stack_size = size;
    /* Prepare initial stack pointer near top of provided stack.
       Reserve a small area so that the first context switch can
       store/pop registers if needed. */
    if (size >= 32) {
        t->sp = stack + size - 16; /* leave room for a few words */
    } else {
        t->sp = stack + size - 1;
    }

    /* zero saved registers so restore won't introduce garbage */
    for (int i = 0; i < 8; ++i) t->regs[i] = 0;
    t->lr = 0;
    t->pc = (uint32_t)func;

    t->priority = priority & 31;
    t->next = NULL;
    t->state = TASK_READY;
    t->wake_tick = 0;

    ready_enqueue(t);
}

void sleep(uint32_t ms) {
    if (!sched.current) return;

    task_t *t = sched.current;
    t->wake_tick = ms;
    t->state = TASK_SLEEPING;

    __asm__ volatile("swi 0"); // request reschedule
}

void scheduler_start(void) {
    task_t *first = pick_next_task();
    if (!first) {
        while (1);
    }

    sched.current = first;
    first->state = TASK_RUNNING;

    /* For the very first task we simply set SP and branch to its PC.
       We avoid calling task_switch(NULL, first) because task_switch
       assumes a valid current pointer to store registers into. */
    uint32_t *first_sp = first->sp;
    uint32_t first_pc = first->pc;

    /* Jump to the task entry point with the prepared stack. */
    __asm__ volatile(
        "mov sp, %0\n"    /* set SP to task's stack */
        "bx  %1\n"        /* jump to PC (task function) */
        :
        : "r"(first_sp), "r"(first_pc)
        : /* no clobber */
    );

    /* never returns */
    while (1);
}

void timer0_irq_handler(void) {
    TIMER_CLR_IRQ = 1;

    uart_puts("TIMER!!!\n");

    // Decrement all sleepers
    for (uint32_t i = 0; i < sched.task_count; ++i) {
        task_t *t = &sched.task_pool[i];
        if (t->state == TASK_SLEEPING && t->wake_tick > 0) {
            t->wake_tick--;
            if (t->wake_tick == 0) {
                t->state = TASK_READY;
            }
        }
    }

    __asm__ volatile("swi 0");
}

/* --- Timer IRQ (1ms) ---
   This function should be called from the IRQ vector (irq_handler).
*/
void irq_handler(void) {
    uint32_t vic_addr = VIC_VADDR;  // acknowledge
    if (vic_addr == TIMER0_BASE) {
        timer0_irq_handler();
    }
    VIC_VADDR = 0;  // signal end-of-interrupt
}

/* --- SWI handler (C) ---
   Dequeues the current task, picks the next ready task, then
   re-enqueues the current task if it is still running.
*/
void swi_handler(void) {
    task_t *curr = sched.current;

    /* Dequeue current task if it is still in the ready queue */
    if (curr) {
        ready_dequeue(curr);
    }

    /* Pick the next ready task */
    task_t *next = pick_next_task();

    /* If no next task or same as current, nothing to do */
    if (!next || next == curr) return;

    /* Re-enqueue current task if it is still running */
    if (curr && curr->state == TASK_RUNNING) {
        curr->state = TASK_READY;
        ready_enqueue(curr);
    }

    /* Switch to next task */
    sched.current = next;
    sched.current->state = TASK_RUNNING;

    task_switch(curr, sched.current);
}

/* --- pick_next_task() ---
   Only scans the ready queues and returns the first READY task.
   Does NOT modify queues or re-enqueue anything.
*/
static task_t *pick_next_task(void) {
    if (!sched.ready_bitmap) return NULL;

    uint32_t bits = sched.ready_bitmap;

    while (bits) {
        uint8_t p = __builtin_ctz(bits);
        bits &= ~(1u << p);

        task_t *cur = sched.ready_head[p];

        while (cur) {
            if (cur->state == TASK_READY) {
                return cur;
            }
            cur = cur->next;
        }
    }

    return NULL;
}

/* --- Ready queue helpers --- */
static void ready_enqueue(task_t *t) {
    uint8_t p = t->priority & 31;
    t->next = NULL;

    if (!sched.ready_head[p]) {
        sched.ready_head[p] = sched.ready_tail[p] = t;
        sched.ready_bitmap |= (1u << p);
    } else {
        sched.ready_tail[p]->next = t;
        sched.ready_tail[p] = t;
    }
}

/* --- dequeue a task from its ready queue (no state change) --- */
static void ready_dequeue(task_t *t) {
    uint8_t p = t->priority & 31;
    task_t *prev = NULL;
    task_t *cur = sched.ready_head[p];

    while (cur) {
        if (cur == t) {
            if (prev) prev->next = cur->next;
            else sched.ready_head[p] = cur->next;

            if (sched.ready_tail[p] == cur) sched.ready_tail[p] = prev;

            cur->next = NULL;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

/* --- Low-level context switch (assembly) ---
   ARM calling convention: first arg in r0 (current), second arg in r1 (next).
   This function:
     - saves r4-r11 into current->regs
     - saves sp into current->sp
     - saves lr into current->lr
     - saves a PC-sentinel (adr) into current->pc so returning to this saved TCB will
       resume after the task_switch call (see "1:" label)
     - restores r4-r11 from next->regs
     - restores sp, lr, pc from next->TCB and branches to it
*/
__attribute__((naked)) void task_switch(task_t *current, task_t *next) {
    __asm__ volatile (
        /* r0 = current, r1 = next */

        /* Save r4-r11 into current->regs (stored at current + offsetof(regs)).
           regs[] sits after sp and stack_size in our struct: we use a fixed offset
           consistent with the C struct layout below. To make offsets robust we
           compute addresses purely in registers (add base + immediate). */
        "push {r4-r11}\n"               /* push to preserve while computing addresses */
        "mov r2, r0\n"                  /* r2 = current */
        "add r2, r2, #12\n"             /* r2 -> &current->regs  (struct layout: stack, stack_size = 2 words => +8; sp @ +8; but using same offsets as prior code) */
        "pop {r4-r11}\n"
        "stmia r2, {r4-r11}\n"          /* store r4-r11 into current->regs */

        /* Save SP into current->sp (offset #8 from start of struct in this layout) */
        "str sp, [r0, #8]\n"

        /* Save LR into current->lr (offset #44) */
        "str lr, [r0, #44]\n"

        /* Save a return PC (address after the context switch) into current->pc */
        "adr r3, 1f\n"
        "str r3, [r0, #48]\n"

        /* Load r4-r11 from next->regs (next + 12) */
        "add r2, r1, #12\n"
        "ldmia r2, {r4-r11}\n"

        /* Restore SP, LR, PC from next TCB */
        "ldr sp, [r1, #8]\n"
        "ldr lr, [r1, #44]\n"
        "ldr r3, [r1, #48]\n"

        /* Branch to next task's saved PC */
        "bx r3\n"

        /* If we return here (resumed task), branch to LR */
        "1:\n"
        "bx lr\n"
    );
}
