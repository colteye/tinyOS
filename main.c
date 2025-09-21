#include "uart.h"
#include "scheduler.h"
#include <stdint.h>

/*-----------------------------------------------------------------
  Hardware definitions
-----------------------------------------------------------------*/
#define TIMER0_BASE     (0x101E2000)
#define TIMER0_LOAD     (*(volatile unsigned int*)(TIMER0_BASE + 0x00))
#define TIMER0_VALUE    (*(volatile unsigned int*)(TIMER0_BASE + 0x04))
#define TIMER0_CONTROL  (*(volatile unsigned int*)(TIMER0_BASE + 0x08))
#define TIMER0_INTCLR   (*(volatile unsigned int*)(TIMER0_BASE + 0x0C))
#define TIMER0_RIS      (*(volatile unsigned int*)(TIMER0_BASE + 0x10))
#define TIMER0_MIS      (*(volatile unsigned int*)(TIMER0_BASE + 0x14))
#define TIMER0_BGLOAD   (*(volatile unsigned int*)(TIMER0_BASE + 0x18))
#define TIMER0_MS       (1000)  // 1 ms = 1000 us

#define NVIC_BASE       (0x10140000)
#define VICIRQSTATUS    (*(volatile unsigned int*)(NVIC_BASE + 0x000))
#define VICFIQSTATUS    (*(volatile unsigned int*)(NVIC_BASE + 0x004))
#define VICRAWINTR      (*(volatile unsigned int*)(NVIC_BASE + 0x008))
#define VICINTSELECT    (*(volatile unsigned int*)(NVIC_BASE + 0x00C))
#define VICINTENABLE    (*(volatile unsigned int*)(NVIC_BASE + 0x010))
#define VICINTENCLEAR   (*(volatile unsigned int*)(NVIC_BASE + 0x014))
#define VICSOFTINT      (*(volatile unsigned int*)(NVIC_BASE + 0x018))

// Enable IRQ globally (implemented in startup.S)
extern void interrupt_enable(void);

/*-----------------------------------------------------------------
  IRQ handler
-----------------------------------------------------------------*/
void irq_handler(void) {
    TIMER0_INTCLR = 0;   // Clear timer interrupt
    __asm__ volatile("svc 0");  // request reschedule
}

/* SVC handler: calls scheduler_tick */
void svc_handler(void) {
    scheduler_tick();
}

/*-----------------------------------------------------------------
  Tasks
-----------------------------------------------------------------*/
#define STACK_SIZE 1024

static uint32_t stack1[STACK_SIZE];
static uint32_t stack2[STACK_SIZE];

void task1(void) {
    while (1) {
        uart_puts("Task 1 running\r\n");
        //sleep(1000);
    }
}

void task2(void) {
    while (1) {
        uart_puts("Task 2 running\r\n");
        //sleep(500);
    }
}

/*-----------------------------------------------------------------
  Main
-----------------------------------------------------------------*/
int main(void) {
    uart_puts("Booting...\r\n");

    // Configure timer for 1ms periodic interrupts
    TIMER0_CONTROL = 0x00;     // Stop timer
    TIMER0_LOAD    = TIMER0_MS; 
    TIMER0_INTCLR  = 0;        // Clear any pending interrupt
    TIMER0_CONTROL = 0xE2;     // Enable: Timer, Periodic, IRQ

    // Enable interrupt in VIC
    VICINTENABLE = 0x10;

    // Enable global IRQs
    interrupt_enable();

    // Initialize scheduler
    scheduler_init();

    // Create tasks
    task_create(task1, stack1, STACK_SIZE, 0);
    task_create(task2, stack2, STACK_SIZE, 0);

    uart_puts("Starting scheduler...\r\n");

    // Start the scheduler (never returns)
    scheduler_start();

    // Safety infinite loop (should never reach here)
    while (1);
    return 0;
}
