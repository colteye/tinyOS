#include "scheduler.h"
#include "uart.h"
#include <stdint.h>

#define STACK_SIZE 1024
static uint32_t stack1[STACK_SIZE];
static uint32_t stack2[STACK_SIZE];

void task1(void) {
    while (1) {
        uart_puts("Task 1 running\r\n");
        //sleep(1000); // 1s
    }
}

void task2(void) {
    while (1) {
        uart_puts("Task 2 running\r\n");
        //sleep(500); // 0.5s
    }
}

int main(void) {
    uart_puts("Booting...\r\n");

    scheduler_init();

    task_create(task1, stack1, STACK_SIZE, 0);
    task_create(task2, stack2, STACK_SIZE, 0);

    uart_puts("Starting scheduler...\r\n");
    scheduler_start();

    while (1); // never reached
}