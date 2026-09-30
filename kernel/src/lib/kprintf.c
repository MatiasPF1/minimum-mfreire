#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/uart.h"
#include "lib/kprintf.h"

/*
 * Division is done without '/' or '%': those compile to calls into libgcc's
 * Thumb division helpers, which hang under minemu. n / 10 is computed as
 * (n * ceil(2^35 / 10)) >> 35, exact for every 32-bit n, using one umull.
 */
static uint32_t div10(uint32_t n) {
    return (uint32_t)(((uint64_t)n * UINT32_C(0xcccccccd)) >> 35);
}

static void print_decimal(uint32_t value) {
    char buffer[10];
    int length = 0;

    do {
        uint32_t quotient = div10(value);
        buffer[length++] = (char)('0' + (value - quotient * 10));
        value = quotient;
    } while (value != 0);

    while (length > 0) {
        uart_putc(buffer[--length]);
    }
}

static void print_hex(uint32_t value) {
    static const char digits[] = "0123456789abcdef";
    int shift = 28;

    /* Skip leading zero nibbles, but always print at least one digit. */
    while (shift > 0 && ((value >> shift) & 0xf) == 0) {
        shift -= 4;
    }
    for (; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> shift) & 0xf]);
    }
}

static void print_signed(int32_t value) {
    if (value < 0) {
        uart_putc('-');
        /* Negate as unsigned so INT32_MIN does not overflow. */
        print_decimal(-(uint32_t)value);
    } else {
        print_decimal((uint32_t)value);
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt != '\0'; ++fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt);
            continue;
        }

        ++fmt;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(args, const char *);
            uart_puts(s != NULL ? s : "(null)");
            break;
        }
        case 'c':
            uart_putc((char)va_arg(args, int));
            break;
        case 'd':
            print_signed((int32_t)va_arg(args, int));
            break;
        case 'u':
            print_decimal((uint32_t)va_arg(args, unsigned int));
            break;
        case 'x':
            print_hex((uint32_t)va_arg(args, unsigned int));
            break;
        case '%':
            uart_putc('%');
            break;
        case '\0':
            /* Trailing lone '%': stop at the end of the string. */
            va_end(args);
            return;
        default:
            /* Unknown specifier: print it verbatim. */
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }
    }

    va_end(args);
}
