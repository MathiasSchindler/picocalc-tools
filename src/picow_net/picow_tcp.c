#include "picow_tcp.h"

#include <stdint.h>
#include <string.h>

#include "pico/rand.h"
#include "pico/stdlib.h"

#include "picow_net.h"

#define ETH_TYPE_IP 0x0800u
#define IP_PROTO_TCP 6u
#define TCP_FLAG_FIN 0x01u
#define TCP_FLAG_SYN 0x02u
#define TCP_FLAG_RST 0x04u
#define TCP_FLAG_PSH 0x08u
#define TCP_FLAG_ACK 0x10u
#define TCP_SOCKET_FD 3
#define TCP_MSS 536u
#define TCP_RX_CAPACITY 32768u

typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2
} TcpState;

typedef struct {
    TcpState state;
    uint8_t remote_ip[4];
    uint8_t remote_mac[6];
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t snd_una;
    uint32_t snd_nxt;
    uint32_t rcv_nxt;
    uint8_t rx[TCP_RX_CAPACITY];
    size_t rx_start;
    size_t rx_len;
} TcpSocket;

static TcpSocket g_tcp;
static PicowTcpDebug g_tcp_debug;

static void send_ack(void);

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

static uint16_t tcp_checksum(const uint8_t src[4], const uint8_t dst[4], const uint8_t *tcp, size_t tcp_len) {
    uint32_t sum = 0;
    size_t i;
    for (i = 0; i < 4; i += 2) sum += (uint16_t)(((uint16_t)src[i] << 8) | src[i + 1]);
    for (i = 0; i < 4; i += 2) sum += (uint16_t)(((uint16_t)dst[i] << 8) | dst[i + 1]);
    sum += IP_PROTO_TCP;
    sum += (uint16_t)tcp_len;
    while (tcp_len >= 2) {
        sum += get16(tcp);
        tcp += 2;
        tcp_len -= 2;
    }
    if (tcp_len != 0) sum += (uint16_t)tcp[0] << 8;
    while (sum >> 16) sum = (sum & 0xffffu) + (sum >> 16);
    return (uint16_t)~sum;
}

static int seq_leq(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) <= 0;
}

static size_t rx_append(const uint8_t *data, size_t len) {
    size_t offset;
    if (len > TCP_RX_CAPACITY - g_tcp.rx_len) len = TCP_RX_CAPACITY - g_tcp.rx_len;
    for (offset = 0; offset < len; ++offset) {
        g_tcp.rx[(g_tcp.rx_start + g_tcp.rx_len + offset) % TCP_RX_CAPACITY] = data[offset];
    }
    g_tcp.rx_len += len;
    return len;
}

static long rx_take(void *buffer, size_t count) {
    uint8_t *out = (uint8_t *)buffer;
    size_t n = count < g_tcp.rx_len ? count : g_tcp.rx_len;
    size_t i;
    for (i = 0; i < n; ++i) out[i] = g_tcp.rx[(g_tcp.rx_start + i) % TCP_RX_CAPACITY];
    g_tcp.rx_start = (g_tcp.rx_start + n) % TCP_RX_CAPACITY;
    g_tcp.rx_len -= n;
    if (n != 0 && (g_tcp.state == TCP_STATE_ESTABLISHED || g_tcp.state == TCP_STATE_CLOSE_WAIT)) send_ack();
    return (long)n;
}

static int send_segment(uint8_t flags, const uint8_t *payload, size_t payload_len) {
    const PicowNetInfo *info = picow_net_info();
    uint8_t frame[14 + 20 + 20 + TCP_MSS + 4];
    uint8_t *ip = frame + 14;
    uint8_t *tcp = ip + 20;
    size_t tcp_header_len = (flags & TCP_FLAG_SYN) ? 24u : 20u;
    size_t tcp_len = tcp_header_len + payload_len;
    size_t frame_len = 14u + 20u + tcp_len;
    if (payload_len > TCP_MSS || info == 0) return -1;
    memcpy(frame, g_tcp.remote_mac, 6);
    memcpy(frame + 6, info->mac, 6);
    put16(frame + 12, ETH_TYPE_IP);
    memset(ip, 0, 20);
    ip[0] = 0x45;
    put16(ip + 2, (uint16_t)(20u + tcp_len));
    ip[8] = 64;
    ip[9] = IP_PROTO_TCP;
    memcpy(ip + 12, info->ip, 4);
    memcpy(ip + 16, g_tcp.remote_ip, 4);
    put16(ip + 10, checksum(ip, 20));
    memset(tcp, 0, tcp_header_len);
    put16(tcp, g_tcp.local_port);
    put16(tcp + 2, g_tcp.remote_port);
    put32(tcp + 4, g_tcp.snd_nxt);
    put32(tcp + 8, g_tcp.rcv_nxt);
    tcp[12] = (uint8_t)(tcp_header_len / 4u) << 4;
    tcp[13] = flags;
    put16(tcp + 14, (uint16_t)(TCP_RX_CAPACITY - g_tcp.rx_len));
    if (flags & TCP_FLAG_SYN) {
        tcp[20] = 2;
        tcp[21] = 4;
        put16(tcp + 22, TCP_MSS);
    }
    if (payload_len != 0) memcpy(tcp + tcp_header_len, payload, payload_len);
    put16(tcp + 16, tcp_checksum(info->ip, g_tcp.remote_ip, tcp, tcp_len));
    g_tcp_debug.tx_segments += 1u;
    return picow_net_send_ethernet(frame, frame_len);
}

static void send_ack(void) {
    (void)send_segment(TCP_FLAG_ACK, 0, 0);
}

static void tcp_handle_ipv4(const uint8_t *packet, size_t length, void *user_data) {
    const PicowNetInfo *info = picow_net_info();
    const uint8_t *tcp;
    const uint8_t *payload;
    size_t ihl;
    size_t tcp_header_len;
    size_t payload_len;
    uint16_t total;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t flags;
    (void)user_data;
    if (g_tcp.state == TCP_STATE_CLOSED || packet == 0 || info == 0 || length < 40) return;
    if ((packet[0] >> 4) != 4 || packet[9] != IP_PROTO_TCP) return;
    ihl = (size_t)(packet[0] & 0x0fu) * 4u;
    if (ihl < 20 || length < ihl + 20) return;
    total = get16(packet + 2);
    if (total > length || total < ihl + 20) return;
    if (memcmp(packet + 16, info->ip, 4) != 0 || memcmp(packet + 12, g_tcp.remote_ip, 4) != 0) return;
    tcp = packet + ihl;
    src_port = get16(tcp);
    dst_port = get16(tcp + 2);
    if (src_port != g_tcp.remote_port || dst_port != g_tcp.local_port) return;
    seq = get32(tcp + 4);
    ack = get32(tcp + 8);
    tcp_header_len = (size_t)(tcp[12] >> 4) * 4u;
    if (tcp_header_len < 20 || ihl + tcp_header_len > total) return;
    flags = tcp[13];
    payload = tcp + tcp_header_len;
    payload_len = total - ihl - tcp_header_len;
    if (flags & TCP_FLAG_RST) {
        g_tcp_debug.resets += 1u;
        g_tcp.state = TCP_STATE_CLOSED;
        return;
    }
    if (seq_leq(g_tcp.snd_una, ack) && seq_leq(ack, g_tcp.snd_nxt)) g_tcp.snd_una = ack;
    if (g_tcp.state == TCP_STATE_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK) && ack == g_tcp.snd_nxt) {
            g_tcp.rcv_nxt = seq + 1u;
            g_tcp.state = TCP_STATE_ESTABLISHED;
            send_ack();
        }
        return;
    }
    if (g_tcp.state == TCP_STATE_FIN_WAIT_1 && g_tcp.snd_una == g_tcp.snd_nxt) g_tcp.state = TCP_STATE_FIN_WAIT_2;
    if (payload_len != 0) {
        if (seq == g_tcp.rcv_nxt) {
            size_t accepted = rx_append(payload, payload_len);
            g_tcp_debug.rx_segments += 1u;
            g_tcp_debug.rx_bytes += (uint32_t)accepted;
            if (accepted < payload_len) {
                g_tcp_debug.rx_overflow_events += 1u;
                g_tcp_debug.rx_dropped_bytes += (uint32_t)(payload_len - accepted);
            }
            g_tcp.rcv_nxt += (uint32_t)accepted;
        }
        send_ack();
    }
    if (flags & TCP_FLAG_FIN) {
        if (seq + (uint32_t)payload_len == g_tcp.rcv_nxt) g_tcp.rcv_nxt += 1u;
        if (g_tcp.state == TCP_STATE_ESTABLISHED) g_tcp.state = TCP_STATE_CLOSE_WAIT;
        else if (g_tcp.state == TCP_STATE_FIN_WAIT_1 || g_tcp.state == TCP_STATE_FIN_WAIT_2) g_tcp.state = TCP_STATE_CLOSED;
        send_ack();
    }
}

int picow_tcp_is_socket_fd(int fd) {
    return fd == TCP_SOCKET_FD;
}

int picow_tcp_connect(const char *host, unsigned int port, int *socket_fd_out) {
    const uint8_t *hop;
    uint32_t isn;
    int attempt;
    if (socket_fd_out == 0 || port == 0 || port > 65535U) return -1;
    *socket_fd_out = -1;
    memset(&g_tcp, 0, sizeof(g_tcp));
    memset(&g_tcp_debug, 0, sizeof(g_tcp_debug));
    if (picow_net_parse_ipv4(host, g_tcp.remote_ip) != 0) return -1;
    hop = picow_net_next_hop_for(g_tcp.remote_ip);
    if (picow_net_arp_resolve(hop, g_tcp.remote_mac) != 0) return -1;
    g_tcp.local_port = (uint16_t)(49152u + (get_rand_32() & 0x3fffu));
    g_tcp.remote_port = (uint16_t)port;
    isn = get_rand_32();
    g_tcp.snd_una = isn;
    g_tcp.snd_nxt = isn;
    g_tcp.state = TCP_STATE_SYN_SENT;
    picow_net_set_ipv4_handler(tcp_handle_ipv4, 0);
    for (attempt = 0; attempt < 6; ++attempt) {
        g_tcp.snd_nxt = isn;
        if (send_segment(TCP_FLAG_SYN, 0, 0) != 0) return -1;
        g_tcp.snd_nxt = isn + 1u;
        absolute_time_t deadline = make_timeout_time_ms(1000);
        while (g_tcp.state == TCP_STATE_SYN_SENT && !time_reached(deadline)) {
            picow_net_poll();
            sleep_ms(2);
        }
        if (g_tcp.state == TCP_STATE_ESTABLISHED) {
            *socket_fd_out = TCP_SOCKET_FD;
            return 0;
        }
    }
    g_tcp.state = TCP_STATE_CLOSED;
    picow_net_set_ipv4_handler(0, 0);
    return -1;
}

long picow_tcp_read(int fd, void *buffer, size_t count) {
    absolute_time_t deadline;
    if (!picow_tcp_is_socket_fd(fd) || buffer == 0 || count == 0) return -1;
    if (g_tcp.rx_len != 0) return rx_take(buffer, count);
    deadline = make_timeout_time_ms(30000);
    while (g_tcp.rx_len == 0 && g_tcp.state != TCP_STATE_CLOSED && !time_reached(deadline)) {
        picow_net_poll();
        sleep_ms(2);
    }
    if (g_tcp.rx_len != 0) return rx_take(buffer, count);
    if (g_tcp.state != TCP_STATE_CLOSED) g_tcp_debug.read_timeouts += 1u;
    return g_tcp.state == TCP_STATE_CLOSED ? 0 : -1;
}

long picow_tcp_write(int fd, const void *buffer, size_t count) {
    const uint8_t *data = (const uint8_t *)buffer;
    size_t sent = 0;
    if (!picow_tcp_is_socket_fd(fd) || (buffer == 0 && count != 0) || g_tcp.state != TCP_STATE_ESTABLISHED) return -1;
    while (sent < count) {
        size_t chunk = count - sent;
        absolute_time_t deadline;
        absolute_time_t resend_deadline;
        uint32_t end_seq;
        if (chunk > TCP_MSS) chunk = TCP_MSS;
        if (send_segment(TCP_FLAG_ACK | TCP_FLAG_PSH, data + sent, chunk) != 0) return sent == 0 ? -1 : (long)sent;
        end_seq = g_tcp.snd_nxt + (uint32_t)chunk;
        g_tcp.snd_nxt = end_seq;
        deadline = make_timeout_time_ms(2000);
        resend_deadline = make_timeout_time_ms(250);
        while (seq_leq(g_tcp.snd_una, end_seq - 1u) && g_tcp.state == TCP_STATE_ESTABLISHED && !time_reached(deadline)) {
            picow_net_poll();
            if (time_reached(resend_deadline) && seq_leq(g_tcp.snd_una, end_seq - 1u)) {
                uint32_t saved_snd_nxt = g_tcp.snd_nxt;
                g_tcp.snd_nxt = end_seq - (uint32_t)chunk;
                (void)send_segment(TCP_FLAG_ACK | TCP_FLAG_PSH, data + sent, chunk);
                g_tcp.snd_nxt = saved_snd_nxt;
                g_tcp_debug.tx_retransmits += 1u;
                resend_deadline = make_timeout_time_ms(250);
            }
            sleep_ms(2);
        }
        if (seq_leq(g_tcp.snd_una, end_seq - 1u)) {
            g_tcp_debug.write_ack_timeouts += 1u;
            return sent == 0 ? -1 : (long)sent;
        }
        sent += chunk;
    }
    return (long)sent;
}

int picow_tcp_close(int fd) {
    if (!picow_tcp_is_socket_fd(fd)) return -1;
    if (g_tcp.state == TCP_STATE_ESTABLISHED || g_tcp.state == TCP_STATE_CLOSE_WAIT) {
        (void)send_segment(TCP_FLAG_ACK | TCP_FLAG_FIN, 0, 0);
        g_tcp.snd_nxt += 1u;
        g_tcp.state = TCP_STATE_FIN_WAIT_1;
    }
    picow_net_set_ipv4_handler(0, 0);
    g_tcp.state = TCP_STATE_CLOSED;
    return 0;
}

int picow_tcp_poll_fd(int fd, int timeout_milliseconds) {
    absolute_time_t deadline;
    if (!picow_tcp_is_socket_fd(fd)) return -1;
    if (g_tcp.rx_len != 0 || g_tcp.state == TCP_STATE_CLOSE_WAIT || g_tcp.state == TCP_STATE_CLOSED) return 1;
    if (timeout_milliseconds < 0) deadline = at_the_end_of_time;
    else deadline = make_timeout_time_ms((uint32_t)timeout_milliseconds);
    while (g_tcp.rx_len == 0 && g_tcp.state != TCP_STATE_CLOSED && !time_reached(deadline)) {
        picow_net_poll();
        sleep_ms(2);
    }
    return (g_tcp.rx_len != 0 || g_tcp.state == TCP_STATE_CLOSE_WAIT || g_tcp.state == TCP_STATE_CLOSED) ? 1 : 0;
}

void picow_tcp_get_debug(PicowTcpDebug *debug_out) {
    if (debug_out == 0) return;
    *debug_out = g_tcp_debug;
    debug_out->state = (int)g_tcp.state;
    debug_out->rx_len = g_tcp.rx_len;
}