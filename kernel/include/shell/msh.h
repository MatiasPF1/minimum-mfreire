#ifndef SHELL_MSH_H
#define SHELL_MSH_H

/* Longest accepted command line in bytes, not counting the '\n'. */
#define MSH_LINE_MAX 20

/* Run the kernel shell forever. Needs uart_init() and CPU IRQs enabled. */
void msh_run(void) __attribute__((noreturn));

#endif
