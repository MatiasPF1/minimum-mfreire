#ifndef CORE_IRQ_H
#define CORE_IRQ_H

#include <stdint.h>

typedef void (*irq_handler_t)(void);

/* Install the handler for a MINEMU_IRQ_* source. Call with IRQs disabled. */
void irq_register(uint32_t source, irq_handler_t handler);

#endif
