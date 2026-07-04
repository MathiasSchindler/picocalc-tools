#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "i2ckbd.h"
#include "lcdspi.h"
#include "picow_net.h"
#include "picow_net_secrets.h"
#include "ssh/ssh_client.h"

#define ROWS 26
#define COLS 40
#define TERM_TOP 0
#define TERM_BOTTOM 25
#define TERM_ROWS (TERM_BOTTOM - TERM_TOP + 1)

#ifndef PICOW_SSH_HOST
#define PICOW_SSH_HOST "192.168.178.176"
#endif

#ifndef PICOW_SSH_PORT
#define PICOW_SSH_PORT 2222U
#endif

#ifndef PICOW_SSH_USER
#define PICOW_SSH_USER "picocalc"
#endif

#ifndef PICOW_SSH_PASSWORD
#define PICOW_SSH_PASSWORD "picocalc"
#endif

#ifndef PICOW_SSH_COMMAND
#define PICOW_SSH_COMMAND "TERM=xterm-256color FORCE_COLOR=1 COLUMNS=40 LINES=26 script -qfec 'stty rows 26 cols 40; /bin/sh -i' /dev/null"
#endif

static char g_term_chars[TERM_ROWS][COLS];
static int g_term_fg[TERM_ROWS][COLS];
static int g_term_bg[TERM_ROWS][COLS];
static unsigned char g_term_dirty[TERM_ROWS];
static int g_term_row = 0;
static int g_term_col = 0;
static int g_term_saved_row = 0;
static int g_term_saved_col = 0;
static int g_term_current_fg = LITEGRAY;
static int g_term_current_bg = BLACK;
static int g_ansi_state = 0;
static char g_ansi_csi[48];
static int g_ansi_csi_len = 0;
static int g_net_log_row = 8;

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

static int term_clamp(int value, int max) {
    if (value < 0) return 0;
    if (value >= max) return max - 1;
    return value;
}

static int ansi_color_16(int code) {
    switch (code) {
        case 0: return BLACK;
        case 1: return RED;
        case 2: return GREEN;
        case 3: return YELLOW;
        case 4: return BLUE;
        case 5: return MAGENTA;
        case 6: return CYAN;
        case 7: return LITEGRAY;
        case 8: return GRAY;
        case 9: return RED;
        case 10: return GREEN;
        case 11: return YELLOW;
        case 12: return CERULEAN;
        case 13: return LILAC;
        case 14: return CYAN;
        case 15: return WHITE;
        default: return LITEGRAY;
    }
}

static void term_reset_attrs(void) {
    g_term_current_fg = LITEGRAY;
    g_term_current_bg = BLACK;
}

static void term_mark_row_dirty(int row) {
    if (row >= 0 && row < TERM_ROWS) g_term_dirty[row] = 1;
}

static void term_mark_all_dirty(void) {
    int row;
    for (row = 0; row < TERM_ROWS; ++row) g_term_dirty[row] = 1;
}

static void term_clear_cells(int row, int start_col, int end_col) {
    int col;
    if (row < 0 || row >= TERM_ROWS) return;
    if (start_col >= COLS || end_col < 0) return;
    if (start_col < 0) start_col = 0;
    if (end_col >= COLS) end_col = COLS - 1;
    if (end_col < start_col) return;
    for (col = start_col; col <= end_col; ++col) {
        g_term_chars[row][col] = ' ';
        g_term_fg[row][col] = g_term_current_fg;
        g_term_bg[row][col] = g_term_current_bg;
    }
    term_mark_row_dirty(row);
}

static void term_draw_row(int row) {
    char span[COLS + 1];
    int col = 0;
    if (row < 0 || row >= TERM_ROWS) return;
    while (col < COLS) {
        int start = col;
        int fg = g_term_fg[row][col];
        int bg = g_term_bg[row][col];
        int used = 0;
        while (col < COLS && g_term_fg[row][col] == fg && g_term_bg[row][col] == bg) {
            span[used++] = g_term_chars[row][col];
            col += 1;
        }
        span[used] = 0;
        lcd_set_cursor(start * 8, (TERM_TOP + row) * 12);
        lcd_print_string_color(span, fg, bg);
    }
    g_term_dirty[row] = 0;
}

static void term_flush_dirty(void) {
    int row;
    for (row = 0; row < TERM_ROWS; ++row) {
        if (g_term_dirty[row]) term_draw_row(row);
    }
}

static void term_clear_screen(void) {
    int row;
    for (row = 0; row < TERM_ROWS; ++row) {
        term_clear_cells(row, 0, COLS - 1);
    }
    g_term_row = 0;
    g_term_col = 0;
}

static void term_scroll_up(void) {
    int row;
    for (row = 1; row < TERM_ROWS; ++row) {
        memcpy(g_term_chars[row - 1], g_term_chars[row], sizeof(g_term_chars[row]));
        memcpy(g_term_fg[row - 1], g_term_fg[row], sizeof(g_term_fg[row]));
        memcpy(g_term_bg[row - 1], g_term_bg[row], sizeof(g_term_bg[row]));
    }
    term_clear_cells(TERM_ROWS - 1, 0, COLS - 1);
    term_mark_all_dirty();
}

static void term_newline(void) {
    g_term_col = 0;
    if (g_term_row + 1 >= TERM_ROWS) {
        term_scroll_up();
    } else {
        g_term_row += 1;
    }
}

static void term_backspace(void) {
    if (g_term_col <= 0) return;
    g_term_col -= 1;
    g_term_chars[g_term_row][g_term_col] = ' ';
    g_term_fg[g_term_row][g_term_col] = g_term_current_fg;
    g_term_bg[g_term_row][g_term_col] = g_term_current_bg;
    term_mark_row_dirty(g_term_row);
}

static void term_putchar_plain(int ch) {
    if (ch == '\r') {
        g_term_col = 0;
        return;
    }
    if (ch == '\n') {
        term_newline();
        return;
    }
    if (ch == '\b' || ch == 0x7f) {
        term_backspace();
        return;
    }
    if (ch == '\t') {
        do {
            term_putchar_plain(' ');
        } while ((g_term_col & 3) != 0);
        return;
    }
    if (ch < 32 || ch >= 127) return;
    if (g_term_col >= COLS) term_newline();
    g_term_chars[g_term_row][g_term_col] = (char)ch;
    g_term_fg[g_term_row][g_term_col] = g_term_current_fg;
    g_term_bg[g_term_row][g_term_col] = g_term_current_bg;
    term_mark_row_dirty(g_term_row);
    g_term_col += 1;
}

static int ansi_parse_params(int *params, int max_params) {
    int count = 0;
    int value = 0;
    int saw_digit = 0;
    int i = 0;
    if (g_ansi_csi[0] == '?') i = 1;
    while (i <= g_ansi_csi_len && count < max_params) {
        char ch = i < g_ansi_csi_len ? g_ansi_csi[i] : ';';
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + ch - '0';
            saw_digit = 1;
        } else if (ch == ';' || ch == ':') {
            params[count++] = saw_digit ? value : -1;
            value = 0;
            saw_digit = 0;
        }
        i += 1;
    }
    return count;
}

static void ansi_apply_sgr(const int *params, int count) {
    int i;
    if (count == 0) {
        term_reset_attrs();
        return;
    }
    for (i = 0; i < count; ++i) {
        int p = params[i] < 0 ? 0 : params[i];
        if (p == 0) {
            term_reset_attrs();
        } else if (p == 1) {
            if (g_term_current_fg == LITEGRAY) g_term_current_fg = WHITE;
        } else if (p == 22) {
            if (g_term_current_fg == WHITE) g_term_current_fg = LITEGRAY;
        } else if (p >= 30 && p <= 37) {
            g_term_current_fg = ansi_color_16(p - 30);
        } else if (p == 39) {
            g_term_current_fg = LITEGRAY;
        } else if (p >= 40 && p <= 47) {
            g_term_current_bg = ansi_color_16(p - 40);
        } else if (p == 49) {
            g_term_current_bg = BLACK;
        } else if (p >= 90 && p <= 97) {
            g_term_current_fg = ansi_color_16(p - 90 + 8);
        } else if (p >= 100 && p <= 107) {
            g_term_current_bg = ansi_color_16(p - 100 + 8);
        } else if ((p == 38 || p == 48) && i + 2 < count && params[i + 1] == 5) {
            int color = ansi_color_16(params[i + 2] & 15);
            if (p == 38) g_term_current_fg = color; else g_term_current_bg = color;
            i += 2;
        } else if ((p == 38 || p == 48) && i + 4 < count && params[i + 1] == 2) {
            int color = RGB(params[i + 2] < 0 ? 0 : params[i + 2], params[i + 3] < 0 ? 0 : params[i + 3], params[i + 4] < 0 ? 0 : params[i + 4]);
            if (p == 38) g_term_current_fg = color; else g_term_current_bg = color;
            i += 4;
        }
    }
}

static void ansi_process_csi(char final_ch) {
    int params[16];
    int count = ansi_parse_params(params, 16);
    int first = count > 0 && params[0] >= 0 ? params[0] : 0;
    int amount = first == 0 ? 1 : first;

    if (g_ansi_csi[0] == '?' && (final_ch == 'h' || final_ch == 'l')) return;
    if (final_ch == 'm') {
        ansi_apply_sgr(params, count);
    } else if (final_ch == 'K') {
        if (first == 1) term_clear_cells(g_term_row, 0, g_term_col);
        else if (first == 2) term_clear_cells(g_term_row, 0, COLS - 1);
        else term_clear_cells(g_term_row, g_term_col, COLS - 1);
    } else if (final_ch == 'J') {
        int row;
        if (first == 2 || first == 3) {
            term_clear_screen();
        } else if (first == 1) {
            for (row = 0; row < g_term_row; ++row) term_clear_cells(row, 0, COLS - 1);
            term_clear_cells(g_term_row, 0, g_term_col);
        } else {
            term_clear_cells(g_term_row, g_term_col, COLS - 1);
            for (row = g_term_row + 1; row < TERM_ROWS; ++row) term_clear_cells(row, 0, COLS - 1);
        }
    } else if (final_ch == 'H' || final_ch == 'f') {
        int row = count > 0 && params[0] > 0 ? params[0] - 1 : 0;
        int col = count > 1 && params[1] > 0 ? params[1] - 1 : 0;
        g_term_row = term_clamp(row, TERM_ROWS);
        g_term_col = term_clamp(col, COLS);
    } else if (final_ch == 'A') {
        g_term_row = term_clamp(g_term_row - amount, TERM_ROWS);
    } else if (final_ch == 'B') {
        g_term_row = term_clamp(g_term_row + amount, TERM_ROWS);
    } else if (final_ch == 'C') {
        g_term_col = term_clamp(g_term_col + amount, COLS);
    } else if (final_ch == 'D') {
        g_term_col = term_clamp(g_term_col - amount, COLS);
    } else if (final_ch == 'G') {
        g_term_col = term_clamp(amount - 1, COLS);
    } else if (final_ch == 's') {
        g_term_saved_row = g_term_row;
        g_term_saved_col = g_term_col;
    } else if (final_ch == 'u') {
        g_term_row = g_term_saved_row;
        g_term_col = g_term_saved_col;
    }
}

static void console_putchar(int ch) {
    if (g_ansi_state == 0) {
        if (ch == 0x1b) {
            g_ansi_state = 1;
            return;
        }
        term_putchar_plain(ch);
        return;
    }
    if (g_ansi_state == 1) {
        if (ch == '[') {
            g_ansi_state = 2;
            g_ansi_csi_len = 0;
        } else if (ch == ']') {
            g_ansi_state = 3;
        } else if (ch == '(' || ch == ')' || ch == '*' || ch == '+') {
            g_ansi_state = 5;
        } else if (ch == 'c') {
            term_reset_attrs();
            term_clear_screen();
            g_ansi_state = 0;
        } else if (ch == '7') {
            g_term_saved_row = g_term_row;
            g_term_saved_col = g_term_col;
            g_ansi_state = 0;
        } else if (ch == '8') {
            g_term_row = g_term_saved_row;
            g_term_col = g_term_saved_col;
            g_ansi_state = 0;
        } else {
            g_ansi_state = 0;
        }
        return;
    }
    if (g_ansi_state == 2) {
        if (ch >= 0x40 && ch <= 0x7e) {
            g_ansi_csi[g_ansi_csi_len] = 0;
            ansi_process_csi((char)ch);
            g_ansi_state = 0;
        } else if (g_ansi_csi_len + 1 < (int)sizeof(g_ansi_csi)) {
            g_ansi_csi[g_ansi_csi_len++] = (char)ch;
        }
        return;
    }
    if (g_ansi_state == 3) {
        if (ch == 7) g_ansi_state = 0;
        else if (ch == 0x1b) g_ansi_state = 4;
        return;
    }
    if (g_ansi_state == 4) {
        g_ansi_state = 0;
        return;
    }
    if (g_ansi_state == 5) {
        g_ansi_state = 0;
    }
}

static void terminal_init(void) {
    term_reset_attrs();
    term_clear_screen();
    term_flush_dirty();
}

void picow_ssh_write_console(const char *text, size_t length) {
    size_t i = 0;
    while (i < length) {
        console_putchar((unsigned char)text[i++]);
    }
    term_flush_dirty();
}

static int picow_ssh_output_callback(const unsigned char *data, size_t size, int extended, void *user_data) {
    (void)extended;
    (void)user_data;
    picow_ssh_write_console((const char *)data, size);
    return 0;
}

static void picow_ssh_net_log(void *user_data, const char *message) {
    (void)user_data;
    screen_line(g_net_log_row, message, YELLOW, BLACK);
    g_net_log_row += 1;
    if (g_net_log_row >= 13) g_net_log_row = 8;
}

int main(void) {
    SshClientConfig config;
    SshClientExecConfig exec_config;
    PicowNetConfig net_config;
    PicowNetInfo net_info;
    char status_line[COLS + 1];
    int exit_status = 255;

    init_i2c_kbd();
    lcd_init();
    lcd_clear();
    (void)set_kbd_backlight(80);

    screen_line(0, "PicoCalc SSH port", WHITE, BLACK);
    screen_line(2, "NewOS sshd target:", CYAN, BLACK);
    snprintf(status_line, sizeof(status_line), "%s:%u", PICOW_SSH_HOST, PICOW_SSH_PORT);
    screen_line(3, status_line, LITEGRAY, BLACK);
    snprintf(status_line, sizeof(status_line), "SSID %.26s", PICOW_NET_SSID);
    screen_line(5, status_line, LITEGRAY, BLACK);

    memset(&net_config, 0, sizeof(net_config));
    net_config.ssid = PICOW_NET_SSID;
    net_config.password = PICOW_NET_PASSWORD;
    net_config.log = picow_ssh_net_log;
    if (picow_net_init_join_dhcp(&net_config, &net_info) != 0) {
        screen_line(6, "WiFi/DHCP failed", RED, BLACK);
        while (1) sleep_ms(1000);
    }
    picow_net_format_ipv4(status_line, sizeof(status_line), net_info.ip);
    screen_line(6, status_line, GREEN, BLACK);
    screen_line(7, "Interactive SSH shell:", LITEGRAY, BLACK);
    terminal_init();

    memset(&config, 0, sizeof(config));
    config.host = PICOW_SSH_HOST;
    config.user = PICOW_SSH_USER;
    config.port = PICOW_SSH_PORT;
    config.password = PICOW_SSH_PASSWORD;
    config.identity_path = 0;
    config.verbose = 1;

    memset(&exec_config, 0, sizeof(exec_config));
    exec_config.client = config;
    exec_config.command = PICOW_SSH_COMMAND;
    exec_config.interactive = 1;
    exec_config.local_echo = 0;
    exec_config.output_callback = picow_ssh_output_callback;

    if (ssh_client_exec(&exec_config, &exit_status) != 0) {
        screen_line(24, "SSH shell failed", RED, BLACK);
    } else {
        snprintf(status_line, sizeof(status_line), "SSH shell exit %d", exit_status);
        screen_line(24, status_line, GREEN, BLACK);
    }

    screen_line(24, "q=halt", LITEGRAY, BLACK);
    while (1) {
        int key = read_i2c_kbd();
        if (key == 'q' || key == 0xb1) break;
        sleep_ms(20);
    }
    while (1) sleep_ms(1000);
}