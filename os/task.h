#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef struct task {
    uint32_t *stack;        // Pointer to preallocated stack array
    uint32_t stack_size;    // Size of stack in uint32_t
    uint32_t *sp;           // Current stack pointer
    uint32_t regs[8];       // r4-r11 callee-saved
    uint32_t lr;            // Link register
    uint32_t pc;            // Program counter
    struct task *next;      // Next task in circular list
} task_t;

void task_create(task_t *task, void (*func)(void), uint32_t *stack, uint32_t size);
void task_yield(void);
void scheduler_start(task_t *first);

#endif
