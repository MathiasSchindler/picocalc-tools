#include "runtime.h"

#include "pico/stdlib.h"

#include <stddef.h>

#define SOLVE_REPL_LINE_CAPACITY 2048U
#define SOLVE_REPL_MAX_ARGS 96

int solve_main(int argc, char **argv);

static char g_repl_line[SOLVE_REPL_LINE_CAPACITY];
static char *g_repl_argv[SOLVE_REPL_MAX_ARGS];

__attribute__((weak)) void picocalc_solve_io_init(void) {
}

__attribute__((weak)) int picocalc_solve_getchar_timeout_us(unsigned int timeout_us) {
    return getchar_timeout_us(timeout_us);
}

__attribute__((weak)) void picocalc_solve_putchar(int ch) {
    putchar_raw(ch);
}

long solve_platform_write(int fd, const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    (void)fd;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] == '\n') {
            picocalc_solve_putchar('\r');
        }
        picocalc_solve_putchar((int)bytes[index]);
    }
    return (long)count;
}

static void repl_write(const char *text) {
    (void)rt_write_cstr(1, text);
}

static void repl_write_line(const char *text) {
    (void)rt_write_line(1, text);
}

static int repl_split_args(char *line, char **argv, int max_args) {
    int argc = 1;
    char *read = line;
    char *write = line;

    argv[0] = (char *)"solve";
    while (*read != '\0') {
        while (*read == ' ' || *read == '\t') read += 1;
        if (*read == '\0') break;
        if (argc >= max_args) return -1;
        argv[argc++] = write;
        if (*read == '"' || *read == '\'') {
            char quote = *read++;
            while (*read != '\0' && *read != quote) *write++ = *read++;
            if (*read == quote) read += 1;
        } else {
            while (*read != '\0' && *read != ' ' && *read != '\t') *write++ = *read++;
        }
        *write++ = '\0';
    }
    return argc;
}

static void repl_run_line(char *line) {
    int argc = repl_split_args(line, g_repl_argv, SOLVE_REPL_MAX_ARGS);
    if (argc < 0) {
        repl_write_line("error: too many arguments");
        return;
    }
    if (argc == 1) return;
    (void)solve_main(argc, g_repl_argv);
}

int main(void) {
    size_t length = 0U;

    stdio_init_all();
    picocalc_solve_io_init();
    sleep_ms(1200);
    repl_write_line("PicoCalc solve");
    repl_write_line("LCD/keyboard REPL ready; enter an expression or solve options.");
    repl_write("> ");

    while (1) {
        int ch = picocalc_solve_getchar_timeout_us(1000);
        if (ch < 0) continue;
        if (ch == '\r' || ch == '\n') {
            (void)rt_write_char(1, '\n');
            g_repl_line[length] = '\0';
            repl_run_line(g_repl_line);
            length = 0U;
            repl_write("> ");
        } else if (ch == 8 || ch == 127) {
            if (length > 0U) {
                length -= 1U;
                repl_write("\b \b");
            }
        } else if (ch >= 32 && ch < 127) {
            if (length + 1U < sizeof(g_repl_line)) {
                g_repl_line[length++] = (char)ch;
                (void)rt_write_char(1, (char)ch);
            }
        }
    }
}