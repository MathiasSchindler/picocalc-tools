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
#define PICOW_SSH_COMMAND "printf pico-sshd-ok"
#endif

static int g_console_row = 8;
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
    screen_line(7, "Auto exec follows:", LITEGRAY, BLACK);

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
    exec_config.output_callback = picow_ssh_output_callback;

    if (ssh_client_exec(&exec_config, &exit_status) != 0) {
        screen_line(14, "SSH exec failed", RED, BLACK);
    } else {
        snprintf(status_line, sizeof(status_line), "SSH exec exit %d", exit_status);
        screen_line(14, status_line, GREEN, BLACK);
    }

    screen_line(24, "q=halt", LITEGRAY, BLACK);
    while (1) {
        int key = read_i2c_kbd();
        if (key == 'q' || key == 0xb1) break;
        sleep_ms(20);
    }
    while (1) sleep_ms(1000);
}