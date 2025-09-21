#include "scheduler.h"
#include <stdint.h>
#include "uart.h"

#define NULL (0)
#define MAX_PRIORITIES (32U)
#define MAX_TASKS      (16U)

/* --- Task states --- */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_STOPPED
} task_state_t;

/* --- Task Control Block --- */
typedef struct task {
    uint32_t *stack;
    uint32_t stack_size;
    uint32_t *sp;
    uint32_t regs[8];
    uint32_t lr;
    uint32_t pc;

    struct task *next; // for ready/sleep lists
    struct task *prev;

    uint8_t priority;
    task_state_t state;
    uint32_t wake_tick;
} task_t;

/* --- Scheduler state --- */
typedef struct {
    task_t *ready_head[MAX_PRIORITIES];
    task_t *ready_tail[MAX_PRIORITIES];
    uint32_t ready_bitmap;

    task_t *sleep_head;

    task_t task_pool[MAX_TASKS];
    uint32_t task_count;

    task_t *current;
} scheduler_t;

static scheduler_t sched;


static void uart_puthex(unsigned int value) {
    static const char hexchars[] = "0123456789ABCDEF";
    char buf[11]; // "0x" + 8 digits + null
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[9 - i] = hexchars[value & 0xF];
        value >>= 4;
    }
    buf[10] = '\0';
    uart_puts(buf);
}

static void uart_putdec(unsigned int value) {
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (value == 0) {
        buf[--i] = '0';
    } else {
        while (value > 0 && i > 0) {
            buf[--i] = '0' + (value % 10);
            value /= 10;
        }
    }
    uart_puts(&buf[i]);
}


/* --- Forward declarations --- */
static void ready_enqueue(task_t *t);
static void ready_dequeue(task_t *t);
static task_t *pick_next_task(void);
static void sleep_enqueue(task_t *t);

/* --- Minimal memset --- */
static void memset(void *dst, int val, uint32_t n) {
    uint8_t *p = dst;
    while (n--) *p++ = (uint8_t)val;
}

/* --- Context switch (ARM naked) --- */
__attribute__((naked)) void task_switch(task_t *current, task_t *next);

/* --- API --- */
void scheduler_init(void) {
    memset(&sched, 0, sizeof(sched));
}

/* --- Task creation --- */
void task_create(void (*func)(void), uint32_t *stack, uint32_t size, uint8_t priority) {
    if (sched.task_count >= MAX_TASKS) return;

    task_t *t = &sched.task_pool[sched.task_count++];
    t->stack = stack;
    t->stack_size = size;
    t->sp = stack + size - 16; // reserve space for context save
    memset(t->regs, 0, sizeof(t->regs));
    t->lr = 0;
    t->pc = (uint32_t)func;
    t->priority = priority & 31;
    t->state = TASK_READY;
    t->wake_tick = 0;
    t->next = t->prev = NULL;

    ready_enqueue(t);
    
    uart_puts("TASK ADDED: ");
    uart_puthex((uint32_t)t);
    uart_puts("\r\n");
}

/* --- Sleep --- */
void sleep(uint32_t ms) {
    if (!sched.current) return;

    task_t *t = sched.current;
    t->wake_tick = ms;
    t->state = TASK_SLEEPING;

    // Remove from ready queue immediately
    ready_dequeue(t);

    // Add to sleep list
    sleep_enqueue(t);

    // Do NOT trigger SVC; preemption only occurs on tick
}

/* --- Start scheduler --- */
void scheduler_start(void) {
    task_t *first = pick_next_task();
    if (!first) return;

    sched.current = first;
    first->state = TASK_RUNNING;

    __asm__ volatile(
        "mov sp, %0\n"
        "bx %1\n"
        :
        : "r"(first->sp), "r"(first->pc)
    );
}

/* --- Scheduler tick: called from SVC --- */
/* --- Scheduler tick: called from SVC --- */
void scheduler_tick(void) {
    uart_puts("\r\n[Scheduler Tick]\r\n");

    /* --- Wake sleeping tasks --- */
    task_t *t = sched.sleep_head;
    while (t) {
        task_t *next = t->next;
        if (t->wake_tick > 0) {
            t->wake_tick--;
            uart_puts(" Decrementing wake_tick for task ");
            uart_puthex((uint32_t)t);
            uart_puts(" -> ");
            uart_putdec(t->wake_tick);
            uart_puts("\r\n");
        }
        if (t->wake_tick == 0) {
            uart_puts(" Waking task ");
            uart_puthex((uint32_t)t);
            uart_puts("\r\n");

            t->state = TASK_READY;

            // Remove from sleep list
            if (t->prev) t->prev->next = t->next;
            if (t->next) t->next->prev = t->prev;
            if (sched.sleep_head == t) sched.sleep_head = t->next;
            t->next = t->prev = NULL;

            // Add back to ready queue
            ready_enqueue(t);
        }
        t = next;
    }

    /* --- Round-robin selection --- */
    task_t *curr = sched.current;

    if (curr && curr->state == TASK_RUNNING) {
        uart_puts(" Current task still running: ");
        uart_puthex((uint32_t)curr);
        uart_puts(" (moving to back of queue)\r\n");

        curr->state = TASK_READY;
        ready_enqueue(curr); // safe, it's not in the queue anymore
    }

    task_t *next_task = pick_next_task();
    if (!next_task) {
        uart_puts(" No next task found, staying idle.\r\n");
        return;
    }

    uart_puts(" Switching to next task: ");
    uart_puthex((uint32_t)next_task);
    uart_puts("\r\n");

    sched.current = next_task;
    next_task->state = TASK_RUNNING;
    task_switch(curr, next_task);
}

/* --- Ready queue helpers --- */
static void ready_enqueue(task_t *t) {
    uint8_t p = t->priority & 31;
    t->next = t->prev = NULL;
    if (!sched.ready_head[p]) {
        sched.ready_head[p] = sched.ready_tail[p] = t;
        sched.ready_bitmap |= (1u << p);
    } else {
        t->prev = sched.ready_tail[p];
        sched.ready_tail[p]->next = t;
        sched.ready_tail[p] = t;
    }
}

static void ready_dequeue(task_t *t) {
    uint8_t p = t->priority & 31;
    if (!sched.ready_head[p]) return;

    if (t->prev) t->prev->next = t->next;
    else sched.ready_head[p] = t->next;

    if (t->next) t->next->prev = t->prev;
    else sched.ready_tail[p] = t->prev;

    t->next = t->prev = NULL;

    if (!sched.ready_head[p]) sched.ready_bitmap &= ~(1u << p);
}

static task_t *pick_next_task(void) {
    if (!sched.ready_bitmap) return NULL;
    uint32_t bits = sched.ready_bitmap;
    while (bits) {
        uint8_t p = __builtin_ctz(bits);
        bits &= ~(1u << p);
        task_t *cur = sched.ready_head[p];
        while (cur) {
            if (cur->state == TASK_READY) {
                ready_dequeue(cur);   // remove before returning
                return cur;
            }
            cur = cur->next;
        }
    }
    return NULL;
}

/* --- Sleep list enqueue --- */
static void sleep_enqueue(task_t *t) {
    t->next = sched.sleep_head;
    t->prev = NULL;
    if (sched.sleep_head) sched.sleep_head->prev = t;
    sched.sleep_head = t;
}

/* --- Low-level context switch --- */
__attribute__((naked)) void task_switch(task_t *current, task_t *next) {
    __asm__ volatile (
        /* --- Save SVC CPSR --- */
        "mrs r12, cpsr\n"

        /* --- Switch to System mode --- */
        "mrs r2, cpsr\n"
        "bic r2, r2, #0x1F\n"
        "orr r2, r2, #0x1F\n"
        "msr cpsr_c, r2\n"

        /* --- Save current task context --- */
        "stmfd sp!, {r4-r11}\n"          // save callee-saved
        "str sp, [r0, #8]\n"             // current->sp
        "str lr, [r0, #44]\n"            // current->lr
        "add r3, r0, #12\n"
        "stmia r3, {r4-r11}\n"           // current->regs

        /* --- Restore next task context --- */
        "add r3, r1, #12\n"
        "ldmia r3, {r4-r11}\n"           // next->regs
        "ldr sp, [r1, #8]\n"             // next->sp
        "ldr lr, [r1, #44]\n"            // next->lr

        /* --- Switch back to SVC mode --- */
        "msr cpsr_c, r12\n"

        /* --- Return normally from SVC --- */
        "bx lr\n"
    );
}

