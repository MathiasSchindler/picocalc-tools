#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "i2ckbd.h"
#include "lcdspi.h"
#include "ssh/ssh_client.h"

#define ROWS 26
#define COLS 40

static int g_console_row = 8;

static void screen_line(int row, const char *text, int fg, int bg) {
    char line[COLS + 1];
    int col = 0;
    if (row < 0 || row >= ROWS) return;
    while (col < COLS && text[col] != 0) {
        line[col] = text[col];
        col += 1;
    }
    while (col < COLS) line[col++] = ' ';
    line[COLS] = 0;
    lcd_set_cursor(0, row * 12);
    lcd_print_string_color(line, fg, bg);
}

void picow_ssh_write_console(const char *text, size_t length) {
    char line[COLS + 1];
    size_t i = 0;
    while (i < length) {
        int col = 0;
        while (i < length && text[i] != '\n' && col < COLS) {
            line[col++] = text[i++];
        }
        if (i < length && text[i] == '\n') i += 1;
        line[col] = 0;
        screen_line(g_console_row, line, LITEGRAY, BLACK);
        g_console_row += 1;
        if (g_console_row >= 23) g_console_row = 8;
    }
}

int main(void) {
    SshClientConfig config;

    init_i2c_kbd();
    lcd_init();
    lcd_clear();
    (void)set_kbd_backlight(80);

    screen_line(0, "PicoCalc SSH port", WHITE, BLACK);
    screen_line(2, "NewOS ssh/crypto vendored", CYAN, BLACK);
    screen_line(3, "TCP transport not implemented yet", YELLOW, BLACK);
    screen_line(5, "Compile/link smoke follows:", LITEGRAY, BLACK);

    memset(&config, 0, sizeof(config));
    config.host = "192.168.178.1";
    config.user = "picocalc";
    config.port = 22;
    config.password = "";
    config.identity_path = 0;
    config.verbose = 1;

    if (ssh_client_connect_and_run(&config) != 0) {
        screen_line(6, "SSH stopped at TCP boundary", GREEN, BLACK);
    } else {
        screen_line(6, "SSH session ended", GREEN, BLACK);
    }

    screen_line(24, "q=halt", LITEGRAY, BLACK);
    while (1) {
        int key = read_i2c_kbd();
        if (key == 'q' || key == 0xb1) break;
        sleep_ms(20);
    }
    while (1) sleep_ms(1000);
}