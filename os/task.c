#include "task.h"
#include <stdint.h>

static task_t *current_task = 0;

// Offsets in task_t
// stack: 0 (uint32_t*)
// stack_size: 4 (uint32_t)
// sp: 8 (uint32_t*)
// regs[8]: 12..43 (r4-r11)
// lr: 44
// pc: 48
// next: 52

void task_create(task_t *task, void (*func)(void), uint32_t *stack, uint32_t size) {
    task->stack = stack;
    task->stack_size = size;
    task->sp = stack + size;      // SP points to top of stack
    task->lr = 0;
    task->pc = (uint32_t)func;
    for (int i = 0; i < 8; i++)
        task->regs[i] = 0;        // r4-r11
    task->next = 0;
}

// Save current stack to TCB
static void save_stack(task_t *task, uint32_t *current_sp) {
    uint32_t used = task->stack + task->stack_size - current_sp;
    for (uint32_t i = 0; i < used; i++)
        task->stack[task->stack_size - used + i] = current_sp[i];
    task->sp = task->stack + task->stack_size - used;
}

// Restore stack from TCB
static void restore_stack(task_t *task) {
    uint32_t used = task->stack + task->stack_size - task->sp;
    uint32_t *dest = task->stack + task->stack_size - used;
    for (uint32_t i = 0; i < used; i++)
        dest[i] = task->stack[task->stack_size - used + i];
    task->sp = dest;
}

// Context switch: save current task, restore next
__attribute__((naked)) void task_switch(task_t *current, task_t *next) {
    __asm__ volatile(
        // Save r4-r11 into current->regs
        "add r2, r0, #12\n"        // r2 = &current->regs[0]
        "stmia r2, {r4-r11}\n"

        // Save SP, LR, PC
        "str sp, [r0, #8]\n"       // current->sp
        "str lr, [r0, #44]\n"      // current->lr
        "adr r3, 1f\n"
        "str r3, [r0, #48]\n"      // current->pc

        // Load r4-r11 from next->regs
        "add r2, r1, #12\n"
        "ldmia r2, {r4-r11}\n"

        // Restore SP, LR, PC
        "ldr sp, [r1, #8]\n"
        "ldr lr, [r1, #44]\n"
        "ldr r3, [r1, #48]\n"

        // Update current_task
        "ldr r2, =current_task\n"
        "str r1, [r2]\n"

        // Jump to next task
        "bx r3\n"

        "1:\n"
        "bx lr\n"
    );
}

// Yield to next task
void task_yield(void) {
    if (!current_task || !current_task->next)
        return;

    task_t *next_task = current_task->next;

    // Save current stack
    save_stack(current_task, (uint32_t *)__builtin_frame_address(0));

    // Restore next stack
    restore_stack(next_task);

    // Switch
    task_switch(current_task, next_task);
}

// Start scheduler
void scheduler_start(task_t *first) {
    current_task = first;

    restore_stack(first);

    __asm__ volatile(
        "ldr r0, =current_task\n"
        "ldr r0, [r0]\n"
        "ldr sp, [r0, #8]\n"
        "ldr r1, [r0, #48]\n"
        "bx r1\n"
    );
}
