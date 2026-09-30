#include <stdint.h>

#include "drivers/uart.h"
#include "lib/kprintf.h"
#include "shell/msh.h"

/*
 * Typed characters are not echoed: the TUI already mirrors them, including
 * backspaces, so the shell only prints prompts and command output.
 */

static char line[MSH_LINE_MAX + 1];

/*
 * Logical length of the line as typed. It may exceed MSH_LINE_MAX; bytes past
 * the limit are ignored rather than stored. Counting them anyway keeps
 * backspace in step with the screen: backspacing an overlong line back under
 * the limit leaves exactly the first MSH_LINE_MAX bytes the user sees.
 */
static uint32_t length;

static void prompt(void) {
    uart_puts("msh> ");
}

static uint32_t skip_spaces(uint32_t i) {
    while (i < length && line[i] == ' ') {
        ++i;
    }
    return i;
}

static int word_is(uint32_t start, uint32_t end, const char *name) {
    uint32_t i = start;
    while (i < end && *name != '\0' && line[i] == *name) {
        ++i;
        ++name;
    }
    return i == end && *name == '\0';
}

static void execute(void) {
    uint32_t start = skip_spaces(0);
    uint32_t end = start;

    if (start == length) {
        return; /* Empty or all-space line. */
    }

    while (end < length && line[end] != ' ') {
        ++end;
    }

    if (word_is(start, end, "echo")) {
        /* Any run of spaces after "echo" is a single separator. */
        line[length] = '\0';
        kprintf("%s\n", &line[skip_spaces(end)]);
        return;
    }

    line[end] = '\0';
    kprintf("command not found: %s\n", &line[start]);
}

static void handle_input(int c) {
    if (c == UART_OVERRUN) {
        /* Bytes were lost, possibly the '\n', so this line can't be trusted.
         * Report it now rather than waiting for a newline that may be gone. */
        uart_puts("\nmsh: input lost, line discarded\n");
        length = 0;
        prompt();
        return;
    }

    if (c == '\n') {
        if (length > MSH_LINE_MAX) {
            kprintf("msh: line too long (max %d bytes)\n", MSH_LINE_MAX);
        } else {
            execute();
        }
        length = 0;
        prompt();
        return;
    }

    if (c == 0x08 || c == 0x7f) {
        if (length > 0) {
            --length;
        }
        return;
    }

    if (length < MSH_LINE_MAX) {
        line[length] = (char)c;
    }
    ++length;
}

void msh_run(void) {
    length = 0;
    prompt();

    for (;;) {
        int c = uart_getc();
        if (c != UART_NO_DATA) {
            handle_input(c);
        }
    }
}
