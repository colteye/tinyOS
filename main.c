#include "task.h"
#include "uart.h"   // assume uart_puts is implemented

#define STACK_SIZE 1024

// Preallocated stacks
static uint32_t stack1[STACK_SIZE];
static uint32_t stack2[STACK_SIZE];

// Task control blocks
static task_t task1, task2;

// Example tasks
void task_func1(void) {
    while (1) {
        uart_puts("Task 1\r\n");
        task_yield();
    }
}

void task_func2(void) {
    while (1) {
        uart_puts("Task 2 !!!\r\n");
        task_yield();
    }
}

int main(void) {
    // Initialize tasks
    task_create(&task1, task_func1, stack1, STACK_SIZE);
    task_create(&task2, task_func2, stack2, STACK_SIZE);

    // Link tasks in circular list
    task1.next = &task2;
    task2.next = &task1;

    uart_puts("Starting scheduler...\r\n");

    // Start scheduler
    scheduler_start(&task1);

    // Never reached
    return 0;
}
