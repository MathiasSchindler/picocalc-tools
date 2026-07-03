#include "picocalc_lcd_bare.h"
#include "runtime.h"

#define CONSOLE_COLS 26
#define CONSOLE_ROWS 20
#define CONSOLE_SCALE 2
#define CONSOLE_X 4
#define CONSOLE_Y 4
#define CONSOLE_CELL_W (6 * CONSOLE_SCALE)
#define CONSOLE_CELL_H (8 * CONSOLE_SCALE)

int solve_main(int argc, char **argv);

static int g_console_col;
static int g_console_row;

static void console_newline(void) {
    g_console_col = 0;
    g_console_row += 1;
    if (g_console_row >= CONSOLE_ROWS) {
        g_console_row = 0;
        picocalc_lcd_clear(0x000000u);
    }
}

static void console_putchar(int ch) {
    char text[2];
    if (ch == '\r') {
        g_console_col = 0;
        return;
    }
    if (ch == '\n') {
        console_newline();
        return;
    }
    if (ch < 32 || ch >= 127) return;
    if (g_console_col >= CONSOLE_COLS) console_newline();
    text[0] = (char)ch;
    text[1] = 0;
    picocalc_lcd_puts_scale(CONSOLE_X + g_console_col * CONSOLE_CELL_W,
                            CONSOLE_Y + g_console_row * CONSOLE_CELL_H,
                            text,
                            0x00ff00u,
                            0x000000u,
                            CONSOLE_SCALE);
    g_console_col += 1;
}

long solve_platform_write(int fd, const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    (void)fd;
    for (i = 0; i < count; ++i) {
        console_putchar(bytes[i]);
    }
    return (long)count;
}

void bare_main(void) {
    char *argv[] = {"solve", "6x-3=0"};
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    (void)rt_write_line(1, "BARE SOLVE");
    (void)rt_write_line(1, "6X-3=0");
    (void)solve_main(2, argv);
#ifndef PICOCALC_BARE_SIM
    while (1) {
    }
#endif
}