#include <stdint.h>

#include "minemu/user_abi.h"

/* Longest accepted command line in bytes, not counting the '\n'. The hw2
 * tests send lines longer than the 20 bytes of the assignment text. */
#define MSH_LINE_MAX 128

/* Passed to handle_input when the kernel reports dropped input. */
#define INPUT_LOST (-1)

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

static void print(const char *text) {
    uint32_t count = 0;
    while (text[count] != '\0') {
        ++count;
    }
    (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)text, count);
}

static void prompt(void) {
    print("msh> ");
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
        /* Print the words that follow, joined by single spaces. They are
         * packed toward the front of the line, which never overtakes i. */
        uint32_t out = 0;
        for (uint32_t i = skip_spaces(end); i < length; i = skip_spaces(i)) {
            if (out != 0) {
                line[out++] = ' ';
            }
            while (i < length && line[i] != ' ') {
                line[out++] = line[i++];
            }
        }
        line[out++] = '\n';
        line[out] = '\0';
        print(line);
        return;
    }

    line[end] = '\0';
    print("command not found: ");
    print(&line[start]);
    print("\n");
}

static void handle_input(int c) {
    if (c == INPUT_LOST) {
        /* Bytes were lost, possibly the '\n', so this line can't be trusted.
         * Report it now rather than waiting for a newline that may be gone. */
        print("\nmsh: input lost, line discarded\n");
        length = 0;
        prompt();
        return;
    }

    if (c == '\n') {
        /* An overlong line is discarded without a message. */
        if (length <= MSH_LINE_MAX) {
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

void minemu_user_main(void) {
    char input[16];

    length = 0;
    prompt();

    for (;;) {
        /* Never blocks: 0 means nothing has arrived yet, so poll again. */
        int32_t count = ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ, (void *)input,
                              (uint32_t)sizeof(input));
        if (count < 0) {
            handle_input(INPUT_LOST);
        }
        for (int32_t index = 0; index < count; ++index) {
            handle_input((unsigned char)input[index]);
        }
    }
}
