#include "core/irq.h"
#include "minemu/irq.h"
#include "minemu/platform.h"

#define IRQ_SOURCE_COUNT 4

static irq_handler_t handlers[IRQ_SOURCE_COUNT];

void irq_register(uint32_t source, irq_handler_t handler) {
    if (source < IRQ_SOURCE_COUNT) {
        handlers[source] = handler;
    }
}

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;

    /* Spurious: nothing was claimed, so there is nothing to end. */
    if (source == MINEMU_IRQ_NONE) {
        return frame;
    }

    if (source < IRQ_SOURCE_COUNT && handlers[source] != 0) {
        handlers[source]();
    }

    /* Always end the interrupt, even without a handler, so the controller
     * is never left with a stuck active source. */
    MINEMU_INTERRUPT->eoi = source;
    return frame;
}
