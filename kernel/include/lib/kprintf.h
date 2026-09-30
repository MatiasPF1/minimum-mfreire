#ifndef LIB_KPRINTF_H
#define LIB_KPRINTF_H

/* Supports %s %c %d %u %x and %%. */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
