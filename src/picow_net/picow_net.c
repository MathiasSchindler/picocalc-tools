#include "picow_net.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cyw43.h"

struct pbuf;

uint16_t pbuf_copy_partial(const struct pbuf *buf, void *dataptr, uint16_t len, uint16_t offset) {
    (void)buf;
    (void)dataptr;
    (void)len;
    (void)offset;
    return 0;
}

#define ETH_TYPE_IP 0x0800u
#define ETH_TYPE_ARP 0x0806u
#define IP_PROTO_UDP 17u
#define DHCP_CLIENT_PORT 68u
#define DHCP_SERVER_PORT 67u
#define PICOW_NET_MAX_UDP_PAYLOAD 1400u

static PicowNetInfo g_info;
static PicowNetStats g_stats;
static uint8_t g_offer_ip[4];
static uint8_t g_arp_target[4];
static uint8_t g_arp_mac[6];
static uint8_t g_udp_cache_ip[4];
static uint8_t g_udp_cache_mac[6];
static uint32_t g_xid = 0x50434431u;
static uint16_t g_ip_id = 1u;
static volatile int g_dhcp_msg;
static volatile bool g_sta_link_ready;
static volatile bool g_arp_ready;
static int g_udp_cache_valid;
static PicowNetIpv4Handler g_ipv4_handler;
static void *g_ipv4_handler_user_data;
static PicowNetRxTraceHandler g_rx_trace_handler;
static void *g_rx_trace_handler_user_data;

static void log_line(const PicowNetConfig *config, const char *message) {
    if (config != 0 && config->log != 0) config->log(config->log_user_data, message);
}

static uint16_t get16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t fnv1a32(const uint8_t *data, size_t len) {
    uint32_t hash = 2166136261u;
    size_t index;
    for (index = 0; index < len; ++index) hash = (hash ^ data[index]) * 16777619u;
    return hash;
}

static uint32_t parse_rxtraffic_counter(const uint8_t *data, size_t len) {
    static const char marker[] = "OCWYRX";
    size_t index;
    for (index = 0; index + sizeof(marker) - 1u <= len; ++index) {
        size_t pos;
        uint32_t value = 0;
        if (memcmp(data + index, marker, sizeof(marker) - 1u) != 0) continue;
        pos = index + sizeof(marker) - 1u;
        while (pos < len && data[pos] == ' ') pos += 1;
        while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
            value = value * 10u + (uint32_t)(data[pos] - '0');
            pos += 1;
        }
        return value;
    }
    return 0;
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

static int ip_eq(const uint8_t a[4], const uint8_t b[4]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

static int mac_eq(const uint8_t a[6], const uint8_t b[6]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && a[4] == b[4] && a[5] == b[5];
}

static int mac_broadcast(const uint8_t mac[6]) {
    return mac[0] == 255u && mac[1] == 255u && mac[2] == 255u && mac[3] == 255u && mac[4] == 255u && mac[5] == 255u;
}

static uint32_t mac_dst_class(const uint8_t mac[6]) {
    if (mac_eq(mac, g_info.mac)) return PICOW_NET_RX_DST_ME;
    if (mac_broadcast(mac)) return PICOW_NET_RX_DST_BROADCAST;
    return PICOW_NET_RX_DST_OTHER;
}

static int ip_zero(const uint8_t ip[4]) {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static void ip_copy(uint8_t dst[4], const uint8_t src[4]) {
    memcpy(dst, src, 4);
}

static int same_subnet(const uint8_t a[4], const uint8_t b[4]) {
    int i;
    for (i = 0; i < 4; ++i) {
        if ((a[i] & g_info.mask[i]) != (b[i] & g_info.mask[i])) return 0;
    }
    return 1;
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

static void fill_eth(uint8_t *frame, const uint8_t dst[6], uint16_t type) {
    memcpy(frame, dst, 6);
    memcpy(frame + 6, g_info.mac, 6);
    put16(frame + 12, type);
}

static void fill_ip(uint8_t *ip, uint16_t total_len, uint8_t proto, const uint8_t dst[4]) {
    memset(ip, 0, 20);
    ip[0] = 0x45;
    put16(ip + 2, total_len);
    put16(ip + 4, g_ip_id++);
    ip[8] = 64;
    ip[9] = proto;
    memcpy(ip + 12, g_info.ip, 4);
    memcpy(ip + 16, dst, 4);
    put16(ip + 10, checksum(ip, 20));
}

int picow_net_send_ethernet(const uint8_t *frame, size_t length) {
    uint8_t padded[1536];
    if (frame == 0 || length > sizeof(padded)) return -1;
    memcpy(padded, frame, length);
    while (length < 60u) padded[length++] = 0;
    g_stats.tx_ethernet += 1;
    return cyw43_send_ethernet(&cyw43_state, CYW43_ITF_STA, length, padded, false);
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
    memcpy(frame + 22, g_info.mac, 6);
    memcpy(frame + 28, g_info.ip, 4);
    memset(frame + 32, 0, 6);
    memcpy(frame + 38, target_ip, 4);
    (void)picow_net_send_ethernet(frame, sizeof(frame));
}

static bool wait_bool(absolute_time_t deadline, volatile bool *flag) {
    while (!*flag && !time_reached(deadline)) {
        cyw43_arch_poll();
        sleep_ms(10);
    }
    return *flag;
}

int picow_net_arp_resolve(const uint8_t target_ip[4], uint8_t mac_out[6]) {
    int attempt;
    if (target_ip == 0 || mac_out == 0) return -1;
    ip_copy(g_arp_target, target_ip);
    g_arp_ready = false;
    for (attempt = 0; attempt < 6; ++attempt) {
        send_arp_request(target_ip);
        if (wait_bool(make_timeout_time_ms(500), &g_arp_ready)) {
            memcpy(mac_out, g_arp_mac, 6);
            return 0;
        }
    }
    return -1;
}

const uint8_t *picow_net_next_hop_for(const uint8_t dst_ip[4]) {
    if (!ip_zero(g_info.gateway) && !same_subnet(g_info.ip, dst_ip)) return g_info.gateway;
    return dst_ip;
}

int picow_net_send_udp(const uint8_t dst_ip[4], uint16_t dst_port, uint16_t src_port, const void *payload, size_t payload_len) {
    uint8_t frame[14 + 20 + 8 + PICOW_NET_MAX_UDP_PAYLOAD];
    uint8_t dst_mac[6];
    const uint8_t *hop;
    uint8_t *ip = frame + 14;
    uint8_t *udp = ip + 20;
    uint16_t udp_len;
    if (dst_ip == 0 || payload == 0 || payload_len > PICOW_NET_MAX_UDP_PAYLOAD) {
        g_stats.tx_udp_errors += 1;
        return -1;
    }
    hop = picow_net_next_hop_for(dst_ip);
    if (g_udp_cache_valid && ip_eq(g_udp_cache_ip, hop)) {
        memcpy(dst_mac, g_udp_cache_mac, 6);
    } else {
        if (picow_net_arp_resolve(hop, dst_mac) != 0) {
            g_stats.tx_udp_errors += 1;
            return -2;
        }
        ip_copy(g_udp_cache_ip, hop);
        memcpy(g_udp_cache_mac, dst_mac, 6);
        g_udp_cache_valid = 1;
    }
    fill_eth(frame, dst_mac, ETH_TYPE_IP);
    fill_ip(ip, (uint16_t)(20u + 8u + payload_len), IP_PROTO_UDP, dst_ip);
    udp_len = (uint16_t)(8u + payload_len);
    put16(udp, src_port);
    put16(udp + 2, dst_port);
    put16(udp + 4, udp_len);
    put16(udp + 6, 0);
    memcpy(udp + 8, payload, payload_len);
    g_stats.tx_udp += 1;
    return picow_net_send_ethernet(frame, 14u + 20u + udp_len);
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
    memcpy(dhcp + 28, g_info.mac, 6);
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
    memcpy(dhcp + opt, g_info.mac, 6);
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
        if (!ip_zero(g_info.dhcp_server)) {
            dhcp[opt++] = 54;
            dhcp[opt++] = 4;
            memcpy(dhcp + opt, g_info.dhcp_server, 4);
            opt += 4;
        }
    }
    dhcp[opt++] = 255;
    dhcp_len = opt < 300u ? 300u : opt;
    fill_ip(ip, (uint16_t)(20u + 8u + dhcp_len), IP_PROTO_UDP, broadcast_ip);
    put16(udp, DHCP_CLIENT_PORT);
    put16(udp + 2, DHCP_SERVER_PORT);
    put16(udp + 4, (uint16_t)(8u + dhcp_len));
    (void)picow_net_send_ethernet(frame, 14u + 20u + 8u + dhcp_len);
}

static void parse_dhcp(const uint8_t *payload, size_t len) {
    size_t opt = 240;
    uint8_t msg = 0;
    if (len < 241u || payload[0] != 2 || get32(payload + 4) != g_xid) return;
    while (opt < len) {
        uint8_t code = payload[opt++];
        uint8_t oplen;
        if (code == 0) continue;
        if (code == 255) break;
        if (opt >= len) break;
        oplen = payload[opt++];
        if (opt + oplen > len) break;
        if (code == 53 && oplen >= 1) msg = payload[opt];
        else if (code == 1 && oplen >= 4) memcpy(g_info.mask, payload + opt, 4);
        else if (code == 3 && oplen >= 4) memcpy(g_info.gateway, payload + opt, 4);
        else if (code == 6 && oplen >= 4) memcpy(g_info.dns, payload + opt, 4);
        else if (code == 54 && oplen >= 4) memcpy(g_info.dhcp_server, payload + opt, 4);
        opt += oplen;
    }
    if (msg == 2 || msg == 5) {
        memcpy(g_offer_ip, payload + 16, 4);
        if (msg == 5) memcpy(g_info.ip, payload + 16, 4);
        g_dhcp_msg = msg;
    }
}

static int dhcp_obtain(const PicowNetConfig *config) {
    int attempt;
    memset(g_info.ip, 0, sizeof(g_info.ip));
    memset(g_info.mask, 0, sizeof(g_info.mask));
    memset(g_info.gateway, 0, sizeof(g_info.gateway));
    memset(g_info.dns, 0, sizeof(g_info.dns));
    memset(g_info.dhcp_server, 0, sizeof(g_info.dhcp_server));
    for (attempt = 0; attempt < 4; ++attempt) {
        absolute_time_t deadline;
        log_line(config, "DHCP discover...");
        g_dhcp_msg = 0;
        dhcp_send(1);
        deadline = make_timeout_time_ms(2500);
        while (g_dhcp_msg != 2 && !time_reached(deadline)) {
            cyw43_arch_poll();
            sleep_ms(10);
        }
        if (g_dhcp_msg != 2) continue;
        log_line(config, "DHCP request...");
        g_dhcp_msg = 0;
        dhcp_send(3);
        deadline = make_timeout_time_ms(2500);
        while (g_dhcp_msg != 5 && !time_reached(deadline)) {
            cyw43_arch_poll();
            sleep_ms(10);
        }
        if (g_dhcp_msg == 5) return 0;
    }
    return -1;
}

static void parse_arp(const uint8_t *frame, size_t len) {
    const uint8_t *arp = frame + 14;
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
    if (len < 34 || (ip[0] >> 4) != 4) return;
    ihl = (size_t)(ip[0] & 0x0fu) * 4u;
    if (ihl < 20 || len < 14 + ihl) return;
    total = get16(ip + 2);
    if (total < ihl || 14u + total > len) return;
    g_stats.ipv4_frames += 1;
    g_stats.last_ip_src = get32(ip + 12);
    g_stats.last_ip_dst = get32(ip + 16);
    proto = ip[9];
    g_stats.last_ip_proto = proto;
    if (proto == IP_PROTO_UDP && total >= ihl + 8u) {
        const uint8_t *udp = ip + ihl;
        const uint8_t *payload = udp + 8;
        uint16_t src = get16(udp);
        uint16_t dst = get16(udp + 2);
        uint16_t udp_len = get16(udp + 4);
        g_stats.udp_frames += 1;
        g_stats.last_udp_src = src;
        g_stats.last_udp_dst = dst;
        g_stats.last_udp_len = udp_len;
        if (udp_len >= 8 && ihl + udp_len <= total && src == DHCP_SERVER_PORT && dst == DHCP_CLIENT_PORT) {
            g_stats.dhcp_frames += 1;
            parse_dhcp(payload, udp_len - 8u);
        }
    }
    if (g_ipv4_handler != 0) {
        g_stats.ipv4_handler_calls += 1;
        g_ipv4_handler(ip, total, g_ipv4_handler_user_data);
    }
}

static void trace_rx_frame(const uint8_t *frame, size_t len, uint16_t type, uint32_t dst_class) {
    PicowNetRxTrace trace;
    const uint8_t *ip;
    size_t ihl;
    uint16_t total;
    if (g_rx_trace_handler == 0) return;
    memset(&trace, 0, sizeof(trace));
    trace.length = (uint32_t)len;
    trace.ethernet_type = type;
    trace.dst_class = dst_class;
    trace.frame_hash = fnv1a32(frame, len);
    if (type == ETH_TYPE_IP && len >= 34u) {
        ip = frame + 14;
        if ((ip[0] >> 4) == 4) {
            ihl = (size_t)(ip[0] & 0x0fu) * 4u;
            if (ihl >= 20u && len >= 14u + ihl) {
                total = get16(ip + 2);
                if (total >= ihl && 14u + total <= len) {
                    trace.ip_src = get32(ip + 12);
                    trace.ip_dst = get32(ip + 16);
                    trace.ip_proto = ip[9];
                    if (trace.ip_proto == IP_PROTO_UDP && total >= ihl + 8u) {
                        const uint8_t *udp = ip + ihl;
                        uint16_t udp_len;
                        trace.udp_src = get16(udp);
                        trace.udp_dst = get16(udp + 2);
                        udp_len = get16(udp + 4);
                        trace.udp_len = udp_len;
                        if (udp_len >= 8u && ihl + udp_len <= total) {
                            const uint8_t *udp_payload = udp + 8;
                            size_t udp_payload_len = (size_t)udp_len - 8u;
                            trace.udp_payload_len = (uint32_t)udp_payload_len;
                            trace.udp_payload_hash = fnv1a32(udp_payload, udp_payload_len);
                            trace.rxtraffic_counter = parse_rxtraffic_counter(udp_payload, udp_payload_len);
                        }
                    }
                }
            }
        }
    }
    g_rx_trace_handler(&trace, g_rx_trace_handler_user_data);
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
    uint32_t dst_class;
    (void)cb_data;
    if (itf != CYW43_ITF_STA || len < 14) return;
    g_stats.ethernet_frames += 1;
    type = get16(buf + 12);
    dst_class = mac_dst_class(buf);
    g_stats.last_ethernet_len = (uint32_t)len;
    g_stats.last_ethernet_type = type;
    if (dst_class == PICOW_NET_RX_DST_ME) g_stats.ethernet_to_me += 1;
    else if (dst_class == PICOW_NET_RX_DST_BROADCAST) g_stats.ethernet_broadcast += 1;
    else g_stats.ethernet_other += 1;
    trace_rx_frame(buf, len, type, dst_class);
    if (type == ETH_TYPE_ARP) {
        g_stats.arp_frames += 1;
        parse_arp(buf, len);
    }
    else if (type == ETH_TYPE_IP) parse_ipv4_frame(buf, len);
}

static int wait_for_join(void) {
    absolute_time_t deadline = make_timeout_time_ms(20000);
    while (!time_reached(deadline)) {
        int status;
        cyw43_arch_poll();
        status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        if (g_sta_link_ready) return 0;
        if (status == CYW43_LINK_FAIL || status == CYW43_LINK_NONET || status == CYW43_LINK_BADAUTH) return -1;
        sleep_ms(50);
    }
    return -1;
}

int picow_net_join_dhcp(const PicowNetConfig *config, PicowNetInfo *info_out) {
    const char *ssid;
    const char *password;
    int err;
    if (config == 0 || config->ssid == 0) return -1;
    ssid = config->ssid;
    password = config->password == 0 ? "" : config->password;
    memset(&g_info, 0, sizeof(g_info));
    memset(&g_stats, 0, sizeof(g_stats));
    g_udp_cache_valid = 0;
    cyw43_arch_enable_sta_mode();
    (void)cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, g_info.mac);
    (void)cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
    g_sta_link_ready = false;
    log_line(config, "joining WiFi...");
    err = cyw43_wifi_join(&cyw43_state, strlen(ssid), (const uint8_t *)ssid,
                          strlen(password), (const uint8_t *)password,
                          password[0] == 0 ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK,
                          NULL, CYW43_CHANNEL_NONE);
    if (err != 0 || wait_for_join() != 0) return -1;
    log_line(config, "WiFi joined");
    if (dhcp_obtain(config) != 0) return -1;
    if (info_out != 0) *info_out = g_info;
    return 0;
}

int picow_net_init_join_dhcp(const PicowNetConfig *config, PicowNetInfo *info_out) {
    int err;
    log_line(config, "cyw43 init...");
    err = cyw43_arch_init();
    if (err != 0) return -1;
    return picow_net_join_dhcp(config, info_out);
}

void picow_net_poll(void) {
    cyw43_arch_poll();
}

const PicowNetInfo *picow_net_info(void) {
    return &g_info;
}

const PicowNetStats *picow_net_stats(void) {
    return &g_stats;
}

void picow_net_set_ipv4_handler(PicowNetIpv4Handler handler, void *user_data) {
    g_ipv4_handler = handler;
    g_ipv4_handler_user_data = user_data;
}

void picow_net_set_rx_trace_handler(PicowNetRxTraceHandler handler, void *user_data) {
    g_rx_trace_handler = handler;
    g_rx_trace_handler_user_data = user_data;
}

int picow_net_parse_ipv4(const char *text, uint8_t ip_out[4]) {
    unsigned int a, b, c, d;
    char tail;
    if (text == 0 || text[0] == 0 || ip_out == 0) return -1;
    if (sscanf(text, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return -1;
    if (a > 255 || b > 255 || c > 255 || d > 255) return -1;
    ip_out[0] = (uint8_t)a;
    ip_out[1] = (uint8_t)b;
    ip_out[2] = (uint8_t)c;
    ip_out[3] = (uint8_t)d;
    return 0;
}

void picow_net_format_ipv4(char *out, size_t out_size, const uint8_t ip[4]) {
    if (out == 0 || out_size == 0) return;
    if (ip == 0) snprintf(out, out_size, "0.0.0.0");
    else snprintf(out, out_size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}