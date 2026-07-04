#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "cyw43.h"
#include "i2ckbd.h"
#include "lcdspi.h"
#include "picow_net_secrets.h"

struct pbuf;

uint16_t pbuf_copy_partial(const struct pbuf *buf, void *dataptr, uint16_t len, uint16_t offset) {
    (void)buf;
    (void)dataptr;
    (void)len;
    (void)offset;
    return 0;
}

#ifndef PICOW_NET_PING_HOST
#define PICOW_NET_PING_HOST ""
#endif

#ifndef PICOW_NET_DNS_NAME
#define PICOW_NET_DNS_NAME "example.com"
#endif

#define ROWS 26
#define COLS 40
#define ETH_TYPE_IP 0x0800u
#define ETH_TYPE_ARP 0x0806u
#define ETH_TYPE_IPV6 0x86ddu
#define ETH_TYPE_EAPOL 0x888eu
#define IP_PROTO_ICMP 1u
#define IP_PROTO_UDP 17u
#define DHCP_CLIENT_PORT 68u
#define DHCP_SERVER_PORT 67u
#define DNS_PORT 53u
#define JOIN_KIND_MASK 0x000fu
#define JOIN_ACTIVE 0x0001u
#define JOIN_AUTH 0x0200u
#define JOIN_LINK 0x0400u
#define JOIN_KEYED 0x0800u

static uint8_t g_mac[6];
static uint8_t g_ip[4];
static uint8_t g_mask[4];
static uint8_t g_gateway[4];
static uint8_t g_dns[4];
static uint8_t g_dhcp_server[4];
static uint8_t g_offer_ip[4];
static uint8_t g_arp_target[4];
static uint8_t g_arp_mac[6];
static uint8_t g_ping_from[4];
static uint8_t g_dns_answer[4];
static uint32_t g_xid = 0x50434431u;
static uint16_t g_ip_id = 1u;
static uint16_t g_ping_id = 0x5043u;
static uint16_t g_dns_id = 0x444eu;
static volatile int g_dhcp_msg;
static volatile bool g_sta_link_ready;
static volatile bool g_arp_ready;
static volatile bool g_ping_reply;
static volatile bool g_dns_reply;
static volatile uint32_t g_tx_frames;
static volatile uint32_t g_tx_errors;
static volatile int g_last_tx_ret;
static volatile uint32_t g_rx_frames;
static volatile uint32_t g_rx_bad_itf;
static volatile uint32_t g_rx_arp;
static volatile uint32_t g_rx_ip;
static volatile uint32_t g_rx_ipv6;
static volatile uint32_t g_rx_eapol;
static volatile uint32_t g_rx_other;
static volatile uint32_t g_rx_udp;
static volatile uint32_t g_rx_dhcp;
static volatile uint32_t g_rx_dhcp_xid;
static volatile uint16_t g_last_eth_type;
static volatile uint16_t g_last_udp_src;
static volatile uint16_t g_last_udp_dst;
static volatile size_t g_last_rx_len;
static volatile int g_last_rx_itf;
static volatile size_t g_last_tx_len;
static uint8_t g_first_rx_head[16];
static uint8_t g_last_rx_head[16];
static volatile int g_pm_ret;

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

static void status_line(int row, const char *text) {
    screen_line(row, text, YELLOW, BLACK);
}

static uint16_t get16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static bool ip_eq(const uint8_t a[4], const uint8_t b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static bool ip_zero(const uint8_t ip[4]) {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static void ip_copy(uint8_t dst[4], const uint8_t src[4]) {
    memcpy(dst, src, 4);
}

static void fmt_ip(char *out, size_t out_size, const uint8_t ip[4]) {
    snprintf(out, out_size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void fmt_mac(char *out, size_t out_size, const uint8_t mac[6]) {
    snprintf(out, out_size, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char *link_status_name(int status) {
    if (status == CYW43_LINK_DOWN) return "DOWN";
    if (status == CYW43_LINK_JOIN) return "JOIN";
    if (status == CYW43_LINK_NOIP) return "NOIP";
    if (status == CYW43_LINK_UP) return "UP";
    if (status == CYW43_LINK_FAIL) return "FAIL";
    if (status == CYW43_LINK_NONET) return "NONET";
    if (status == CYW43_LINK_BADAUTH) return "BADAUTH";
    return "?";
}

static void show_radio_status(int row, const char *phase) {
    char line[COLS + 1];
    char bssid_text[18];
    uint8_t bssid[6];
    uint32_t join_state = cyw43_state.wifi_join_state;
    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    int32_t rssi = 0;
    int rssi_ret = -1;
    int bssid_ret = -1;

    snprintf(line, sizeof(line), "%s st %-7s ready%d %c%c%c%c %03lx", phase,
             link_status_name(status), g_sta_link_ready ? 1 : 0,
             (join_state & JOIN_ACTIVE) == JOIN_ACTIVE ? 'A' : '-',
             (join_state & JOIN_AUTH) == JOIN_AUTH ? 'u' : '-',
             (join_state & JOIN_LINK) == JOIN_LINK ? 'L' : '-',
             (join_state & JOIN_KEYED) == JOIN_KEYED ? 'K' : '-',
             (unsigned long)(join_state & 0xfffu));
    screen_line(row, line, LITEGRAY, BLACK);

    if (g_sta_link_ready) {
        bssid_ret = cyw43_wifi_get_bssid(&cyw43_state, bssid);
        rssi_ret = cyw43_wifi_get_rssi(&cyw43_state, &rssi);
    }
    if (bssid_ret == 0) fmt_mac(bssid_text, sizeof(bssid_text), bssid);
    else snprintf(bssid_text, sizeof(bssid_text), "--:--:--:--:--:--");
    snprintf(line, sizeof(line), "BSSID %s", bssid_text);
    screen_line(row + 1, line, bssid_ret == 0 ? CYAN : LITEGRAY, BLACK);

    if (rssi_ret == 0) snprintf(line, sizeof(line), "RSSI %ld dBm pm%d", (long)rssi, g_pm_ret);
    else snprintf(line, sizeof(line), "RSSI n/a err%d pm%d", rssi_ret, g_pm_ret);
    screen_line(row + 2, line, rssi_ret == 0 ? GREEN : LITEGRAY, BLACK);
}

static void show_sta_mac(int row) {
    char line[COLS + 1];
    char mac_text[18];
    fmt_mac(mac_text, sizeof(mac_text), g_mac);
    snprintf(line, sizeof(line), "STA MAC %s", mac_text);
    screen_line(row, line, LITEGRAY, BLACK);
}

static bool parse_ipv4(const char *text, uint8_t ip[4]) {
    unsigned int a, b, c, d;
    char tail;
    if (text == NULL || text[0] == 0) return false;
    if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    ip[0] = (uint8_t)a;
    ip[1] = (uint8_t)b;
    ip[2] = (uint8_t)c;
    ip[3] = (uint8_t)d;
    return true;
}

static uint16_t checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    while (len >= 2) {
        sum += get16(data);
        data += 2;
        len -= 2;
    }
    if (len != 0) sum += (uint16_t)data[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return (uint16_t)~sum;
}

static int send_frame(uint8_t *frame, size_t len) {
    int ret;
    while (len < 60u) frame[len++] = 0;
    g_last_tx_len = len;
    ret = cyw43_send_ethernet(&cyw43_state, CYW43_ITF_STA, len, frame, false);
    g_last_tx_ret = ret;
    if (ret == 0) g_tx_frames += 1;
    else g_tx_errors += 1;
    return ret;
}

static void fill_eth(uint8_t *frame, const uint8_t dst[6], uint16_t type) {
    memcpy(frame, dst, 6);
    memcpy(frame + 6, g_mac, 6);
    put16(frame + 12, type);
}

static void fill_ip(uint8_t *ip, uint16_t total_len, uint8_t proto, const uint8_t dst[4]) {
    memset(ip, 0, 20);
    ip[0] = 0x45;
    put16(ip + 2, total_len);
    put16(ip + 4, g_ip_id++);
    ip[8] = 64;
    ip[9] = proto;
    memcpy(ip + 12, g_ip, 4);
    memcpy(ip + 16, dst, 4);
    put16(ip + 10, checksum(ip, 20));
}

static void send_arp_request(const uint8_t target_ip[4]) {
    static const uint8_t broadcast[6] = {255, 255, 255, 255, 255, 255};
    uint8_t frame[60];
    fill_eth(frame, broadcast, ETH_TYPE_ARP);
    put16(frame + 14, 1);
    put16(frame + 16, ETH_TYPE_IP);
    frame[18] = 6;
    frame[19] = 4;
    put16(frame + 20, 1);
    memcpy(frame + 22, g_mac, 6);
    memcpy(frame + 28, g_ip, 4);
    memset(frame + 32, 0, 6);
    memcpy(frame + 38, target_ip, 4);
    (void)send_frame(frame, sizeof(frame));
}

static bool wait_until(absolute_time_t deadline, volatile bool *flag) {
    while (!*flag && !time_reached(deadline)) {
        cyw43_arch_poll();
        sleep_ms(10);
    }
    return *flag;
}

static bool arp_resolve(const uint8_t target_ip[4], uint8_t mac[6]) {
    int attempt;
    ip_copy(g_arp_target, target_ip);
    g_arp_ready = false;
    for (attempt = 0; attempt < 6; ++attempt) {
        send_arp_request(target_ip);
        if (wait_until(make_timeout_time_ms(500), &g_arp_ready)) {
            memcpy(mac, g_arp_mac, 6);
            return true;
        }
    }
    return false;
}

static bool same_subnet(const uint8_t a[4], const uint8_t b[4]) {
    int i;
    for (i = 0; i < 4; ++i) {
        if ((a[i] & g_mask[i]) != (b[i] & g_mask[i])) return false;
    }
    return true;
}

static const uint8_t *next_hop_for(const uint8_t dst[4]) {
    if (!ip_zero(g_gateway) && !same_subnet(g_ip, dst)) return g_gateway;
    return dst;
}

static void dhcp_send(uint8_t msg_type) {
    static const uint8_t broadcast[6] = {255, 255, 255, 255, 255, 255};
    static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    uint8_t frame[342];
    uint8_t *ip = frame + 14;
    uint8_t *udp = ip + 20;
    uint8_t *dhcp = udp + 8;
    size_t opt;
    size_t dhcp_len;
    memset(frame, 0, sizeof(frame));
    fill_eth(frame, broadcast, ETH_TYPE_IP);
    dhcp[0] = 1;
    dhcp[1] = 1;
    dhcp[2] = 6;
    put32(dhcp + 4, g_xid);
    put16(dhcp + 10, 0x8000u);
    memcpy(dhcp + 28, g_mac, 6);
    dhcp[236] = 99;
    dhcp[237] = 130;
    dhcp[238] = 83;
    dhcp[239] = 99;
    opt = 240;
    dhcp[opt++] = 53;
    dhcp[opt++] = 1;
    dhcp[opt++] = msg_type;
    dhcp[opt++] = 61;
    dhcp[opt++] = 7;
    dhcp[opt++] = 1;
    memcpy(dhcp + opt, g_mac, 6);
    opt += 6;
    dhcp[opt++] = 55;
    dhcp[opt++] = 4;
    dhcp[opt++] = 1;
    dhcp[opt++] = 3;
    dhcp[opt++] = 6;
    dhcp[opt++] = 15;
    if (msg_type == 3) {
        dhcp[opt++] = 50;
        dhcp[opt++] = 4;
        memcpy(dhcp + opt, g_offer_ip, 4);
        opt += 4;
        if (!ip_zero(g_dhcp_server)) {
            dhcp[opt++] = 54;
            dhcp[opt++] = 4;
            memcpy(dhcp + opt, g_dhcp_server, 4);
            opt += 4;
        }
    }
    dhcp[opt++] = 255;
    dhcp_len = opt < 300u ? 300u : opt;
    fill_ip(ip, (uint16_t)(20u + 8u + dhcp_len), IP_PROTO_UDP, broadcast_ip);
    put16(udp, DHCP_CLIENT_PORT);
    put16(udp + 2, DHCP_SERVER_PORT);
    put16(udp + 4, (uint16_t)(8u + dhcp_len));
    (void)send_frame(frame, 14u + 20u + 8u + dhcp_len);
}

static void show_dhcp_counters(int row, int attempt) {
    char line[COLS + 1];
    snprintf(line, sizeof(line), "try%d tx%lu e%lu r%lu bad%lu", attempt,
             (unsigned long)g_tx_frames, (unsigned long)g_tx_errors,
             (unsigned long)g_rx_frames, (unsigned long)g_rx_bad_itf);
    screen_line(row, line, LITEGRAY, BLACK);
    snprintf(line, sizeof(line), "ip%lu udp%lu dhcp%lu xid%lu ret%d",
             (unsigned long)g_rx_ip, (unsigned long)g_rx_udp,
             (unsigned long)g_rx_dhcp, (unsigned long)g_rx_dhcp_xid,
             g_last_tx_ret);
    screen_line(row + 1, line, LITEGRAY, BLACK);
    snprintf(line, sizeof(line), "v6%lu eap%lu oth%lu i%d t%04x l%lu/%lu",
             (unsigned long)g_rx_ipv6, (unsigned long)g_rx_eapol,
             (unsigned long)g_rx_other, g_last_rx_itf, g_last_eth_type,
             (unsigned long)g_last_rx_len, (unsigned long)g_last_tx_len);
    screen_line(row + 2, line, LITEGRAY, BLACK);
    snprintf(line, sizeof(line), "rx %02x%02x%02x%02x %02x%02x %02x%02x %02x%02x",
             g_last_rx_head[0], g_last_rx_head[1], g_last_rx_head[2], g_last_rx_head[3],
             g_last_rx_head[12], g_last_rx_head[13], g_last_rx_head[14], g_last_rx_head[15],
             g_first_rx_head[12], g_first_rx_head[13]);
    screen_line(row + 3, line, LITEGRAY, BLACK);
}

static void parse_dhcp(const uint8_t *payload, size_t len) {
    size_t opt = 240;
    uint8_t msg = 0;
    g_rx_dhcp += 1;
    if (len < 241u || payload[0] != 2 || get32(payload + 4) != g_xid) return;
    g_rx_dhcp_xid += 1;
    while (opt < len) {
        uint8_t code = payload[opt++];
        uint8_t oplen;
        if (code == 0) continue;
        if (code == 255) break;
        if (opt >= len) break;
        oplen = payload[opt++];
        if (opt + oplen > len) break;
        if (code == 53 && oplen >= 1) msg = payload[opt];
        else if (code == 1 && oplen >= 4) memcpy(g_mask, payload + opt, 4);
        else if (code == 3 && oplen >= 4) memcpy(g_gateway, payload + opt, 4);
        else if (code == 6 && oplen >= 4) memcpy(g_dns, payload + opt, 4);
        else if (code == 54 && oplen >= 4) memcpy(g_dhcp_server, payload + opt, 4);
        opt += oplen;
    }
    if (msg == 2 || msg == 5) {
        memcpy(g_offer_ip, payload + 16, 4);
        if (msg == 5) memcpy(g_ip, payload + 16, 4);
        g_dhcp_msg = msg;
    }
}

static bool dhcp_obtain(void) {
    int attempt;
    char line[COLS + 1];
    memset(g_ip, 0, sizeof(g_ip));
    memset(g_mask, 0, sizeof(g_mask));
    memset(g_gateway, 0, sizeof(g_gateway));
    memset(g_dns, 0, sizeof(g_dns));
    memset(g_dhcp_server, 0, sizeof(g_dhcp_server));
    for (attempt = 0; attempt < 4; ++attempt) {
        absolute_time_t deadline;
        absolute_time_t update_at;
        g_dhcp_msg = 0;
        status_line(7, "DHCP discover...");
        dhcp_send(1);
        deadline = make_timeout_time_ms(2500);
        update_at = make_timeout_time_ms(250);
        while (g_dhcp_msg != 2 && !time_reached(deadline)) {
            cyw43_arch_poll();
            if (time_reached(update_at)) {
                show_radio_status(3, "dhcp");
                show_dhcp_counters(20, attempt + 1);
                update_at = make_timeout_time_ms(250);
            }
            sleep_ms(10);
        }
        if (g_dhcp_msg != 2) continue;
        fmt_ip(line, sizeof(line), g_offer_ip);
        screen_line(8, line, CYAN, BLACK);
        g_dhcp_msg = 0;
        status_line(7, "DHCP request...");
        dhcp_send(3);
        deadline = make_timeout_time_ms(2500);
        update_at = make_timeout_time_ms(250);
        while (g_dhcp_msg != 5 && !time_reached(deadline)) {
            cyw43_arch_poll();
            if (time_reached(update_at)) {
                show_radio_status(3, "dhcp");
                show_dhcp_counters(20, attempt + 1);
                update_at = make_timeout_time_ms(250);
            }
            sleep_ms(10);
        }
        if (g_dhcp_msg == 5) return true;
    }
    return false;
}

static void send_icmp_echo(const uint8_t dst_ip[4], const uint8_t dst_mac[6]) {
    uint8_t frame[14 + 20 + 8 + 16];
    uint8_t *ip = frame + 14;
    uint8_t *icmp = ip + 20;
    fill_eth(frame, dst_mac, ETH_TYPE_IP);
    fill_ip(ip, sizeof(frame) - 14u, IP_PROTO_ICMP, dst_ip);
    memset(icmp, 0, 8 + 16);
    icmp[0] = 8;
    put16(icmp + 4, g_ping_id);
    put16(icmp + 6, 1);
    memcpy(icmp + 8, "PicoCalcNetDiag!", 16);
    put16(icmp + 2, checksum(icmp, 8 + 16));
    (void)send_frame(frame, sizeof(frame));
}

static bool ping_host(const uint8_t target_ip[4]) {
    uint8_t mac[6];
    const uint8_t *hop = next_hop_for(target_ip);
    char line[COLS + 1];
    status_line(13, "ARP for ping...");
    if (!arp_resolve(hop, mac)) return false;
    g_ping_reply = false;
    status_line(13, "ICMP echo request...");
    send_icmp_echo(target_ip, mac);
    if (!wait_until(make_timeout_time_ms(2500), &g_ping_reply)) return false;
    fmt_ip(line, sizeof(line), g_ping_from);
    screen_line(14, line, GREEN, BLACK);
    return true;
}

static size_t encode_dns_name(uint8_t *out, const char *name) {
    size_t used = 0;
    const char *label = name;
    while (*label != 0) {
        const char *dot = strchr(label, '.');
        size_t len = dot == NULL ? strlen(label) : (size_t)(dot - label);
        if (len == 0 || len > 63 || used + len + 1 >= 240) return 0;
        out[used++] = (uint8_t)len;
        memcpy(out + used, label, len);
        used += len;
        if (dot == NULL) break;
        label = dot + 1;
    }
    out[used++] = 0;
    return used;
}

static void send_dns_query(const uint8_t dns_ip[4], const uint8_t dst_mac[6], const char *name) {
    uint8_t frame[14 + 20 + 8 + 256];
    uint8_t *ip = frame + 14;
    uint8_t *udp = ip + 20;
    uint8_t *dns = udp + 8;
    size_t qlen;
    size_t dns_len;
    fill_eth(frame, dst_mac, ETH_TYPE_IP);
    memset(dns, 0, 256);
    put16(dns, g_dns_id);
    put16(dns + 2, 0x0100);
    put16(dns + 4, 1);
    qlen = encode_dns_name(dns + 12, name);
    if (qlen == 0) return;
    put16(dns + 12 + qlen, 1);
    put16(dns + 12 + qlen + 2, 1);
    dns_len = 12 + qlen + 4;
    fill_ip(ip, (uint16_t)(20u + 8u + dns_len), IP_PROTO_UDP, dns_ip);
    put16(udp, 49152u);
    put16(udp + 2, DNS_PORT);
    put16(udp + 4, (uint16_t)(8u + dns_len));
    put16(udp + 6, 0);
    (void)send_frame(frame, 14u + 20u + 8u + dns_len);
}

static size_t dns_skip_name(const uint8_t *dns, size_t len, size_t pos) {
    while (pos < len) {
        uint8_t c = dns[pos++];
        if (c == 0) return pos;
        if ((c & 0xc0u) == 0xc0u) return pos + 1;
        pos += c;
    }
    return len + 1;
}

static void parse_dns(const uint8_t *dns, size_t len) {
    uint16_t qd;
    uint16_t an;
    size_t pos;
    uint16_t i;
    if (len < 12 || get16(dns) != g_dns_id) return;
    qd = get16(dns + 4);
    an = get16(dns + 6);
    pos = 12;
    for (i = 0; i < qd; ++i) {
        pos = dns_skip_name(dns, len, pos);
        pos += 4;
        if (pos > len) return;
    }
    for (i = 0; i < an; ++i) {
        uint16_t type;
        uint16_t class_code;
        uint16_t rdlen;
        pos = dns_skip_name(dns, len, pos);
        if (pos + 10 > len) return;
        type = get16(dns + pos);
        class_code = get16(dns + pos + 2);
        rdlen = get16(dns + pos + 8);
        pos += 10;
        if (pos + rdlen > len) return;
        if (type == 1 && class_code == 1 && rdlen == 4) {
            memcpy(g_dns_answer, dns + pos, 4);
            g_dns_reply = true;
            return;
        }
        pos += rdlen;
    }
}

static bool dns_query(const char *name) {
    uint8_t mac[6];
    const uint8_t *dns_ip = ip_zero(g_dns) ? g_gateway : g_dns;
    const uint8_t *hop = next_hop_for(dns_ip);
    char line[COLS + 1];
    if (ip_zero(dns_ip)) return false;
    status_line(17, "ARP for DNS...");
    if (!arp_resolve(hop, mac)) return false;
    g_dns_reply = false;
    status_line(17, "DNS query...");
    send_dns_query(dns_ip, mac, name);
    if (!wait_until(make_timeout_time_ms(3000), &g_dns_reply)) return false;
    fmt_ip(line, sizeof(line), g_dns_answer);
    screen_line(18, line, GREEN, BLACK);
    return true;
}

static void parse_arp(const uint8_t *frame, size_t len) {
    const uint8_t *arp = frame + 14;
    g_rx_arp += 1;
    if (len < 42 || get16(arp) != 1 || get16(arp + 2) != ETH_TYPE_IP || arp[4] != 6 || arp[5] != 4) return;
    if (get16(arp + 6) != 2) return;
    if (ip_eq(arp + 14, g_arp_target)) {
        memcpy(g_arp_mac, arp + 8, 6);
        g_arp_ready = true;
    }
}

static void parse_ipv4_frame(const uint8_t *frame, size_t len) {
    const uint8_t *ip = frame + 14;
    size_t ihl;
    uint16_t total;
    uint8_t proto;
    g_rx_ip += 1;
    if (len < 34 || (ip[0] >> 4) != 4) return;
    ihl = (size_t)(ip[0] & 0x0fu) * 4u;
    if (ihl < 20 || len < 14 + ihl) return;
    total = get16(ip + 2);
    if (total < ihl || 14u + total > len) return;
    proto = ip[9];
    if (proto == IP_PROTO_UDP && total >= ihl + 8u) {
        const uint8_t *udp = ip + ihl;
        const uint8_t *payload = udp + 8;
        uint16_t src = get16(udp);
        uint16_t dst = get16(udp + 2);
        uint16_t udp_len = get16(udp + 4);
        g_rx_udp += 1;
        g_last_udp_src = src;
        g_last_udp_dst = dst;
        if (udp_len < 8 || ihl + udp_len > total) return;
        if (src == DHCP_SERVER_PORT && dst == DHCP_CLIENT_PORT) parse_dhcp(payload, udp_len - 8u);
        else if (src == DNS_PORT && dst == 49152u) parse_dns(payload, udp_len - 8u);
    } else if (proto == IP_PROTO_ICMP && total >= ihl + 8u) {
        const uint8_t *icmp = ip + ihl;
        if (icmp[0] == 0 && get16(icmp + 4) == g_ping_id && get16(icmp + 6) == 1) {
            memcpy(g_ping_from, ip + 12, 4);
            g_ping_reply = true;
        }
    }
}

void cyw43_cb_tcpip_init(cyw43_t *self, int itf) {
    (void)self;
    (void)itf;
}

void cyw43_cb_tcpip_deinit(cyw43_t *self, int itf) {
    (void)self;
    (void)itf;
}

void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf) {
    (void)self;
    if (itf == CYW43_ITF_STA) g_sta_link_ready = true;
}

void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf) {
    (void)self;
    if (itf == CYW43_ITF_STA) g_sta_link_ready = false;
}

void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf) {
    uint16_t type;
    (void)cb_data;
    g_rx_frames += 1;
    g_last_rx_itf = itf;
    g_last_rx_len = len;
    if (len >= sizeof(g_last_rx_head)) {
        memcpy(g_last_rx_head, buf, sizeof(g_last_rx_head));
        if (g_rx_frames == 1) memcpy(g_first_rx_head, buf, sizeof(g_first_rx_head));
    } else {
        memset(g_last_rx_head, 0, sizeof(g_last_rx_head));
        memcpy(g_last_rx_head, buf, len);
        if (g_rx_frames == 1) {
            memset(g_first_rx_head, 0, sizeof(g_first_rx_head));
            memcpy(g_first_rx_head, buf, len);
        }
    }
    if (itf != CYW43_ITF_STA || len < 14) {
        g_rx_bad_itf += 1;
        return;
    }
    type = get16(buf + 12);
    g_last_eth_type = type;
    if (type == ETH_TYPE_ARP) parse_arp(buf, len);
    else if (type == ETH_TYPE_IP) parse_ipv4_frame(buf, len);
    else if (type == ETH_TYPE_IPV6) g_rx_ipv6 += 1;
    else if (type == ETH_TYPE_EAPOL) g_rx_eapol += 1;
    else g_rx_other += 1;
}

static bool wait_for_join(void) {
    absolute_time_t deadline = make_timeout_time_ms(20000);
    absolute_time_t update_at = make_timeout_time_ms(250);
    while (!time_reached(deadline)) {
        int status;
        cyw43_arch_poll();
        status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        if (time_reached(update_at)) {
            show_radio_status(3, "join");
            update_at = make_timeout_time_ms(250);
        }
        if (g_sta_link_ready) return true;
        if (status == CYW43_LINK_FAIL || status == CYW43_LINK_NONET || status == CYW43_LINK_BADAUTH) return false;
        sleep_ms(50);
    }
    return false;
}

int main(void) {
    uint8_t ping_ip[4];
    char line[COLS + 1];
    int err;
    init_i2c_kbd();
    lcd_init();
    lcd_clear();
    (void)set_kbd_backlight(80);
    screen_line(0, "PicoCalc Pico W net diag", WHITE, BLACK);
    snprintf(line, sizeof(line), "SSID %.26s", PICOW_NET_SSID);
    screen_line(2, line, CYAN, BLACK);
    status_line(4, "cyw43 init...");
    err = cyw43_arch_init();
    if (err != 0) {
        snprintf(line, sizeof(line), "cyw43 init failed %d", err);
        status_line(4, line);
        while (1) sleep_ms(1000);
    }
    cyw43_arch_enable_sta_mode();
    (void)cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, g_mac);
    show_sta_mac(5);
    g_pm_ret = cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
    g_sta_link_ready = false;
    status_line(6, "joining WiFi...");
    err = cyw43_wifi_join(&cyw43_state, strlen(PICOW_NET_SSID), (const uint8_t *)PICOW_NET_SSID,
                          strlen(PICOW_NET_PASSWORD), (const uint8_t *)PICOW_NET_PASSWORD,
                          PICOW_NET_PASSWORD[0] == 0 ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK,
                          NULL, CYW43_CHANNEL_NONE);
    if (err != 0 || !wait_for_join()) {
        snprintf(line, sizeof(line), "join failed err=%d", err);
        status_line(6, line);
        show_radio_status(3, "fail");
        while (1) sleep_ms(1000);
    }
    status_line(6, "WiFi joined");
    show_radio_status(3, "up");

    if (!dhcp_obtain()) {
        status_line(7, "DHCP failed");
        show_radio_status(3, "dhcp");
        show_dhcp_counters(20, 4);
        while (1) sleep_ms(1000);
    }
    fmt_ip(line, sizeof(line), g_ip);
    screen_line(7, "DHCP ok", GREEN, BLACK);
    screen_line(8, line, GREEN, BLACK);
    fmt_ip(line, sizeof(line), g_gateway);
    screen_line(9, line, LITEGRAY, BLACK);
    fmt_ip(line, sizeof(line), g_dns);
    screen_line(10, line, LITEGRAY, BLACK);

    if (!parse_ipv4(PICOW_NET_PING_HOST, ping_ip)) ip_copy(ping_ip, g_gateway);
    fmt_ip(line, sizeof(line), ping_ip);
    screen_line(12, line, CYAN, BLACK);
    if (ping_host(ping_ip)) screen_line(13, "ping reply", GREEN, BLACK);
    else screen_line(13, "ping failed", RED, BLACK);

    screen_line(16, PICOW_NET_DNS_NAME, CYAN, BLACK);
    if (dns_query(PICOW_NET_DNS_NAME)) screen_line(17, "DNS A reply", GREEN, BLACK);
    else screen_line(17, "DNS failed", RED, BLACK);

    screen_line(23, "r=retest q=deinit", LITEGRAY, BLACK);
    while (1) {
        static absolute_time_t update_at;
        static bool update_at_set;
        int key;
        cyw43_arch_poll();
        if (!update_at_set || time_reached(update_at)) {
            show_radio_status(3, "idle");
            update_at = make_timeout_time_ms(1000);
            update_at_set = true;
        }
        key = read_i2c_kbd();
        if (key == 'q' || key == 0xb1) break;
        sleep_ms(20);
    }
    cyw43_arch_deinit();
    while (1) sleep_ms(1000);
}