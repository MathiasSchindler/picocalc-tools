#include "pico/stdlib.h"
#include "hardware/clocks.h"

#include "i2ckbd.h"
#include "lcdspi.h"

#define PICOCALC_CONSOLE_COLS 40
#define PICOCALC_CONSOLE_ROWS 26
#define PICOCALC_FONT_WIDTH   8
#define PICOCALC_FONT_HEIGHT  12

#define PICOCALC_KEY_BACKSPACE 0x08
#define PICOCALC_KEY_ENTER     0x0a
#define PICOCALC_KEY_ESC       0xb1
#define PICOCALC_KEY_DEL       0xd4

static int g_picocalc_io_ready = 0;
static char g_console[PICOCALC_CONSOLE_ROWS][PICOCALC_CONSOLE_COLS + 1];
static int g_console_row = 0;
static int g_console_col = 0;

static void console_clear_buffer(void) {
    int row;
    int col;
    for (row = 0; row < PICOCALC_CONSOLE_ROWS; ++row) {
        for (col = 0; col < PICOCALC_CONSOLE_COLS; ++col) {
            g_console[row][col] = ' ';
        }
        g_console[row][PICOCALC_CONSOLE_COLS] = '\0';
    }
    g_console_row = 0;
    g_console_col = 0;
}

static void console_draw_row(int row) {
    lcd_set_cursor(0, row * PICOCALC_FONT_HEIGHT);
    lcd_print_string_color(g_console[row], GREEN, BLACK);
}

static void console_redraw(void) {
    int row;
    lcd_clear();
    for (row = 0; row < PICOCALC_CONSOLE_ROWS; ++row) {
        console_draw_row(row);
    }
}

static void console_newline(void) {
    g_console_col = 0;
    if (g_console_row + 1 < PICOCALC_CONSOLE_ROWS) {
        g_console_row += 1;
    } else {
        int row;
        for (row = 1; row < PICOCALC_CONSOLE_ROWS; ++row) {
            int col;
            for (col = 0; col <= PICOCALC_CONSOLE_COLS; ++col) {
                g_console[row - 1][col] = g_console[row][col];
            }
        }
        for (row = 0; row < PICOCALC_CONSOLE_COLS; ++row) {
            g_console[PICOCALC_CONSOLE_ROWS - 1][row] = ' ';
        }
        g_console[PICOCALC_CONSOLE_ROWS - 1][PICOCALC_CONSOLE_COLS] = '\0';
        console_redraw();
    }
}

static void console_backspace(void) {
    if (g_console_col > 0) {
        g_console_col -= 1;
    } else if (g_console_row > 0) {
        g_console_row -= 1;
        g_console_col = PICOCALC_CONSOLE_COLS - 1;
    } else {
        return;
    }
    g_console[g_console_row][g_console_col] = ' ';
    console_draw_row(g_console_row);
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
    if (ch == '\b') {
        console_backspace();
        return;
    }
    if (ch < 32 || ch >= 127) return;
    if (g_console_col >= PICOCALC_CONSOLE_COLS) console_newline();
    g_console[g_console_row][g_console_col] = (char)ch;
    text[0] = (char)ch;
    text[1] = '\0';
    lcd_set_cursor(g_console_col * PICOCALC_FONT_WIDTH, g_console_row * PICOCALC_FONT_HEIGHT);
    lcd_print_string_color(text, GREEN, BLACK);
    g_console_col += 1;
}

void picocalc_solve_io_init(void) {
    set_sys_clock_khz(133000, true);
    init_i2c_kbd();
    lcd_init();
    lcd_clear();
    (void)set_kbd_backlight(80);
    console_clear_buffer();
    g_picocalc_io_ready = 1;
}

int picocalc_solve_getchar_timeout_us(unsigned int timeout_us) {
    absolute_time_t deadline = make_timeout_time_us(timeout_us);
    do {
        int ch;
        if (!g_picocalc_io_ready) return getchar_timeout_us(timeout_us);
        ch = read_i2c_kbd();
        if (ch < 0) {
            ch = getchar_timeout_us(0);
        }
        if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 'a';
        if (ch == PICOCALC_KEY_ENTER) return '\n';
        if (ch == PICOCALC_KEY_BACKSPACE || ch == PICOCALC_KEY_DEL) return 8;
        if (ch == PICOCALC_KEY_ESC) return 27;
        if (ch >= 0) return ch;
        sleep_ms(1);
    } while (!time_reached(deadline));
    return PICO_ERROR_TIMEOUT;
}

void picocalc_solve_putchar(int ch) {
    putchar_raw(ch);
    if (g_picocalc_io_ready) {
        console_putchar(ch);
    }
}