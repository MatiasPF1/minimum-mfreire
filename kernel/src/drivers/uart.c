#include "core/irq.h"
#include "drivers/uart.h"
#include "minemu/irq.h"
#include "minemu/platform.h"

/*
 * RX ring buffer shared between the IRQ handler (producer) and uart_getc
 * (consumer). The handler runs with IRQs masked by the CPU; uart_getc masks
 * IRQs itself while it touches the buffer. Size is a power of two so the
 * indices wrap with a mask instead of '%'. One slot stays empty to tell a
 * full buffer from an empty one, so it holds RX_BUFFER_SIZE - 1 entries.
 *
 * Entries are 16-bit so a byte can never be mistaken for RX_OVERRUN_MARK.
 * The last free slot is reserved for that marker: when bytes must be
 * dropped, the marker records where the gap is, so the reader learns about
 * the loss (e.g. of a newline) instead of waiting for input that is gone.
 */
#define RX_BUFFER_SIZE 256u
#define RX_BUFFER_MASK (RX_BUFFER_SIZE - 1)
#define RX_OVERRUN_MARK 0x100u

static volatile uint16_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_head; /* next slot to write */
static volatile uint32_t rx_tail; /* next slot to read */

static void rx_push(uint16_t entry) {
    rx_buffer[rx_head] = entry;
    rx_head = (rx_head + 1) & RX_BUFFER_MASK;
}

static void uart_rx_irq(void) {
    /* Drain the hardware FIFO completely, or the IRQ stays asserted. */
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        uint16_t byte = (uint16_t)(MINEMU_UART0->rx_data & 0xff);
        uint32_t used = (rx_head - rx_tail) & RX_BUFFER_MASK;
        uint32_t free = (RX_BUFFER_SIZE - 1) - used;

        if (free >= 2) {
            rx_push(byte);
        } else if (free == 1 &&
                   rx_buffer[(rx_head - 1) & RX_BUFFER_MASK] != RX_OVERRUN_MARK) {
            rx_push(RX_OVERRUN_MARK);
        }
        /* Otherwise the byte is dropped inside a gap already marked. */
    }
}

void uart_init(void) {
    rx_head = 0;
    rx_tail = 0;
    irq_register(MINEMU_IRQ_UART0, uart_rx_irq);
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_INTERRUPT->enable |= UINT32_C(1) << MINEMU_IRQ_UART0;
}

void uart_putc(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
    }
    MINEMU_UART0->tx_data = (uint32_t)(uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

int uart_getc(void) {
    int byte = UART_NO_DATA;

    minemu_irq_disable();
    if (rx_tail != rx_head) {
        uint16_t entry = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) & RX_BUFFER_MASK;
        byte = entry == RX_OVERRUN_MARK ? UART_OVERRUN : (int)entry;
    }
    minemu_irq_enable();

    return byte;
}
