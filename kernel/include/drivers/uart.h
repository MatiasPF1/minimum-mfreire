#ifndef DRIVERS_UART_H
#define DRIVERS_UART_H

/* Returned by uart_getc when no byte is buffered. */
#define UART_NO_DATA (-1)
/* Returned by uart_getc, in stream order, where received bytes were dropped
 * because the RX buffer was full. */
#define UART_OVERRUN (-2)

/* Register the RX handler and enable UART0 RX interrupts at the device and
 * the interrupt controller. Call before CPU IRQs are enabled. */
void uart_init(void);

void uart_putc(char c);
void uart_puts(const char *s);

/* Pop one received byte (0-255), UART_OVERRUN, or UART_NO_DATA. Never
 * blocks. Enables CPU IRQs on return, so call only after IRQs are enabled. */
int uart_getc(void);

#endif
