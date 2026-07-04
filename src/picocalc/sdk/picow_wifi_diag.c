#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "cyw43.h"
#include "i2ckbd.h"
#include "lcdspi.h"

#define DIAG_ROWS 26
#define DIAG_COLS 40
#define MAX_APS 12
#define SCAN_TIMEOUT_MS 12000u
#define CONNECT_TIMEOUT_MS 15000u

#define KEY_ENTER 0x0a
#define KEY_ESC 0xb1
#define KEY_UP 0x80
#define KEY_DOWN 0x81

typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    int rssi;
    uint16_t channel;
    uint8_t auth;
} WifiAp;

static WifiAp g_aps[MAX_APS];
static int g_ap_count;
static int g_selected;
static int g_open_count;
static char g_status[DIAG_COLS + 1];

static void screen_line(int row, const char *text, int fg, int bg) {
    char line[DIAG_COLS + 1];
    int col = 0;
    if (row < 0 || row >= DIAG_ROWS) return;
    while (col < DIAG_COLS && text[col] != 0) {
        line[col] = text[col];
        col += 1;
    }
    while (col < DIAG_COLS) line[col++] = ' ';
    line[DIAG_COLS] = 0;
    lcd_set_cursor(0, row * 12);
    lcd_print_string_color(line, fg, bg);
}

static void set_status(const char *text) {
    snprintf(g_status, sizeof(g_status), "%s", text);
    screen_line(24, g_status, YELLOW, BLACK);
}

static const char *auth_name(uint8_t auth) {
    if (auth == CYW43_AUTH_OPEN) return "open";
    return "sec";
}

static const char *link_name(int status) {
    switch (status) {
    case CYW43_LINK_DOWN: return "down";
    case CYW43_LINK_JOIN: return "joined";
    case CYW43_LINK_NOIP: return "no-ip";
    case CYW43_LINK_UP: return "up";
    case CYW43_LINK_FAIL: return "fail";
    case CYW43_LINK_NONET: return "no-net";
    case CYW43_LINK_BADAUTH: return "bad-auth";
    default: return "unknown";
    }
}

static int read_key(void) {
    int key = read_i2c_kbd();
    if (key < 0) key = getchar_timeout_us(0);
    if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';
    return key;
}

static void poll_for_ms(uint32_t ms) {
    absolute_time_t deadline = make_timeout_time_ms(ms);
    do {
        cyw43_arch_poll();
        sleep_ms(5);
    } while (!time_reached(deadline));
}

static void draw_header(void) {
    char line[DIAG_COLS + 1];
    snprintf(line, sizeof(line), "PicoCalc Pico W WiFi diag");
    screen_line(0, line, WHITE, BLACK);
    snprintf(line, sizeof(line), "r=rescan enter=open connect q=quit");
    screen_line(1, line, LITEGRAY, BLACK);
}

static void draw_scan_list(void) {
    char line[64];
    int row;
    snprintf(line, sizeof(line), "found %d APs, %d open", g_ap_count, g_open_count);
    screen_line(3, line, CYAN, BLACK);
    for (row = 0; row < MAX_APS; ++row) {
        int index = row;
        if (index < g_ap_count) {
            const WifiAp *ap = &g_aps[index];
            snprintf(line, sizeof(line), "%c%2d %3ddBm ch%2u %-4s %.18s",
                     index == g_selected ? '>' : ' ', index + 1, ap->rssi,
                     (unsigned)ap->channel, auth_name(ap->auth), ap->ssid);
            screen_line(5 + row, line, ap->auth == CYW43_AUTH_OPEN ? GREEN : LITEGRAY, BLACK);
        } else {
            screen_line(5 + row, "", LITEGRAY, BLACK);
        }
    }
}

static void draw_bssid(const WifiAp *ap) {
    char line[DIAG_COLS + 1];
    snprintf(line, sizeof(line), "bssid %02x:%02x:%02x:%02x:%02x:%02x",
             ap->bssid[0], ap->bssid[1], ap->bssid[2], ap->bssid[3], ap->bssid[4], ap->bssid[5]);
    screen_line(18, line, LITEGRAY, BLACK);
}

static void draw_all(void) {
    lcd_clear();
    draw_header();
    draw_scan_list();
    if (g_ap_count > 0) draw_bssid(&g_aps[g_selected]);
    screen_line(23, "Only open APs are attempted", LITEGRAY, BLACK);
    screen_line(24, g_status, YELLOW, BLACK);
}

static bool same_bssid(const uint8_t a[6], const uint8_t b[6]) {
    int i;
    for (i = 0; i < 6; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    int i;
    WifiAp *ap;
    (void)env;
    if (result == NULL || result->ssid_len == 0) return 0;
    for (i = 0; i < g_ap_count; ++i) {
        if (same_bssid(g_aps[i].bssid, result->bssid)) return 0;
    }
    if (g_ap_count >= MAX_APS) return 0;
    ap = &g_aps[g_ap_count++];
    memset(ap, 0, sizeof(*ap));
    memcpy(ap->bssid, result->bssid, sizeof(ap->bssid));
    i = result->ssid_len;
    if (i > 32) i = 32;
    memcpy(ap->ssid, result->ssid, (size_t)i);
    ap->ssid[i] = 0;
    ap->rssi = result->rssi;
    ap->channel = result->channel;
    ap->auth = result->auth_mode;
    if (ap->auth == CYW43_AUTH_OPEN) g_open_count += 1;
    draw_scan_list();
    draw_bssid(ap);
    return 0;
}

static void start_scan(void) {
    cyw43_wifi_scan_options_t options;
    absolute_time_t deadline;
    int err;
    memset(&options, 0, sizeof(options));
    memset(g_aps, 0, sizeof(g_aps));
    g_ap_count = 0;
    g_open_count = 0;
    g_selected = 0;
    set_status("starting scan...");
    draw_scan_list();
    err = cyw43_wifi_scan(&cyw43_state, &options, NULL, scan_result);
    if (err != 0) {
        char line[DIAG_COLS + 1];
        snprintf(line, sizeof(line), "scan start failed: %d", err);
        set_status(line);
        return;
    }
    deadline = make_timeout_time_ms(SCAN_TIMEOUT_MS);
    while (cyw43_wifi_scan_active(&cyw43_state) && !time_reached(deadline)) {
        cyw43_arch_poll();
        sleep_ms(20);
    }
    if (cyw43_wifi_scan_active(&cyw43_state)) set_status("scan timeout; partial results");
    else set_status("scan complete");
    draw_scan_list();
    if (g_ap_count > 0) draw_bssid(&g_aps[g_selected]);
}

static void connect_selected_open(void) {
    WifiAp *ap;
    char line[DIAG_COLS + 1];
    int err;
    int status;
    if (g_ap_count <= 0) {
        set_status("no AP selected");
        return;
    }
    ap = &g_aps[g_selected];
    if (ap->auth != CYW43_AUTH_OPEN) {
        set_status("selected AP is not open");
        return;
    }
    snprintf(line, sizeof(line), "connecting to %.22s", ap->ssid);
    set_status(line);
    err = cyw43_arch_wifi_connect_timeout_ms(ap->ssid, NULL, CYW43_AUTH_OPEN, CONNECT_TIMEOUT_MS);
    status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    snprintf(line, sizeof(line), "connect err=%d link=%s", err, link_name(status));
    set_status(line);
    draw_bssid(ap);
}

static void move_selection(int delta) {
    if (g_ap_count <= 0) return;
    g_selected += delta;
    if (g_selected < 0) g_selected = g_ap_count - 1;
    if (g_selected >= g_ap_count) g_selected = 0;
    draw_scan_list();
    draw_bssid(&g_aps[g_selected]);
}

int main(void) {
    int init_result;
    stdio_init_all();
    init_i2c_kbd();
    lcd_init();
    lcd_clear();
    (void)set_kbd_backlight(80);
    snprintf(g_status, sizeof(g_status), "booting cyw43...");
    draw_all();

    init_result = cyw43_arch_init();
    if (init_result != 0) {
        char line[DIAG_COLS + 1];
        snprintf(line, sizeof(line), "cyw43 init failed: %d", init_result);
        set_status(line);
        while (1) sleep_ms(1000);
    }

    cyw43_arch_enable_sta_mode();
    set_status("cyw43 ready");
    poll_for_ms(250);
    start_scan();

    while (1) {
        int key;
        cyw43_arch_poll();
        key = read_key();
        if (key == 'q' || key == KEY_ESC) break;
        if (key == 'r') start_scan();
        else if (key == KEY_ENTER || key == '\r' || key == '\n') connect_selected_open();
        else if (key == 'j' || key == KEY_DOWN) move_selection(1);
        else if (key == 'k' || key == KEY_UP) move_selection(-1);
        sleep_ms(20);
    }

    set_status("deinit cyw43");
    cyw43_arch_deinit();
    while (1) sleep_ms(1000);
}
