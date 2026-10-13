#include <stdint.h>

#include "core/loader.h"
#include "drivers/uart.h"
#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/trap.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

/*
 * Every handler validates the request, the length and the whole user range
 * before it sends, removes input, advances the RNG or traces anything.
 */
static int32_t uart_ioctl(volatile struct minemu_uart_regs *uart, uint32_t request,
                          uint32_t pointer, uint32_t length) {
    if (length > MINEMU_IOCTL_MAX_TRANSFER) {
        return MINEMU_IOCTL_ERROR;
    }
    if (request == MINEMU_IOCTL_UART_WRITE &&
        user_range_ok(pointer, length, MINEMU_PTE_READABLE)) {
        uart_write(uart, (const char *)(uintptr_t)pointer, (int)length);
        return (int32_t)length;
    }
    if (request == MINEMU_IOCTL_UART_READ &&
        user_range_ok(pointer, length, MINEMU_PTE_WRITABLE)) {
        int count = uart_read(uart, (char *)(uintptr_t)pointer, (int)length);
        return count < 0 ? MINEMU_IOCTL_ERROR : count;
    }
    return MINEMU_IOCTL_ERROR;
}

static int32_t rng_ioctl(uint32_t request, uint32_t argument) {
    if (request == MINEMU_IOCTL_RNG_SEED) {
        MINEMU_RNG->seed = argument;
        return 0;
    }
    if ((request != MINEMU_IOCTL_RNG_NEXT && request != MINEMU_IOCTL_RNG_STATE) ||
        !user_range_ok(argument, sizeof(uint32_t), MINEMU_PTE_WRITABLE)) {
        return MINEMU_IOCTL_ERROR;
    }

    /* Reading the data register is what advances the generator. */
    uint32_t value = request == MINEMU_IOCTL_RNG_NEXT ? MINEMU_RNG->data : MINEMU_RNG->state;
    /* Stored a byte at a time so the user pointer need not be aligned. */
    uint8_t *out = (uint8_t *)(uintptr_t)argument;
    for (uint32_t index = 0; index < sizeof(uint32_t); ++index) {
        out[index] = (uint8_t)value;
        value >>= 8;
    }
    return 0;
}

static int32_t sys_ioctl(uint32_t fd, uint32_t request, uint32_t argument0,
                         uint32_t argument1) {
    switch (fd) {
    case MINEMU_FD_UART_IN:
        if (request != MINEMU_IOCTL_UART_READ) {
            break;
        }
        return uart_ioctl(MINEMU_UART0, request, argument0, argument1);
    case MINEMU_FD_UART_OUT:
        if (request != MINEMU_IOCTL_UART_WRITE) {
            break;
        }
        return uart_ioctl(MINEMU_UART0, request, argument0, argument1);
    case MINEMU_FD_UART_DIAG:
        return uart_ioctl(MINEMU_UART1, request, argument0, argument1);
    case MINEMU_FD_RNG:
        return rng_ioctl(request, argument0);
    case MINEMU_FD_TRACE:
        if (request != MINEMU_IOCTL_TRACE_EVENT) {
            break;
        }
        minemu_trace_event(argument0);
        return 0;
    default:
        break;
    }
    return MINEMU_IOCTL_ERROR;
}

/* The entry checks are adapted from minemu_svc_dispatch in
 * examples/user-mode-hello/kernel/main.c (minimum-template). */
struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    uint32_t mode = frame->spsr & CPSR_MODE_MASK;
    if (frame->exception_id != MINEMU_EXCEPTION_SVC || mode != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    /* Only read the instruction once the caller is known to be A32 user code. */
    uint32_t instruction = *(const uint32_t *)(uintptr_t)frame->fault_pc;
    if (instruction != A32_SVC_ZERO || frame->r[7] != MINEMU_SYSCALL_IOCTL) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    frame->r[0] = (uint32_t)sys_ioctl(frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
    return frame;
}
