// drivers/uart.c
#include "uart.h"

#define UART0_DR (*(volatile unsigned int*)0x101f1000)

void uart_putc(char c) {
    UART0_DR = c;
}

void uart_puts(const char* s) {
    while (*s) uart_putc(*s++);
}