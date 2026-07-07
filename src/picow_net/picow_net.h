#ifndef PICOW_NET_H
#define PICOW_NET_H

#include <stddef.h>
#include <stdint.h>

typedef void (*PicowNetLogFn)(void *user_data, const char *message);
typedef void (*PicowNetIpv4Handler)(const uint8_t *packet, size_t length, void *user_data);

#define PICOW_NET_RX_DST_OTHER 0u
#define PICOW_NET_RX_DST_ME 1u
#define PICOW_NET_RX_DST_BROADCAST 2u

typedef struct {
    uint32_t length;
    uint32_t ethernet_type;
    uint32_t dst_class;
    uint32_t ip_src;
    uint32_t ip_dst;
    uint32_t ip_proto;
    uint32_t udp_src;
    uint32_t udp_dst;
    uint32_t udp_len;
    uint32_t frame_hash;
    uint32_t udp_payload_len;
    uint32_t udp_payload_hash;
    uint32_t rxtraffic_counter;
} PicowNetRxTrace;

typedef void (*PicowNetRxTraceHandler)(const PicowNetRxTrace *trace, void *user_data);

typedef struct {
    const char *ssid;
    const char *password;
    PicowNetLogFn log;
    void *log_user_data;
} PicowNetConfig;

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t mask[4];
    uint8_t gateway[4];
    uint8_t dns[4];
    uint8_t dhcp_server[4];
} PicowNetInfo;

typedef struct {
    uint32_t ethernet_frames;
    uint32_t arp_frames;
    uint32_t ipv4_frames;
    uint32_t udp_frames;
    uint32_t dhcp_frames;
    uint32_t ipv4_handler_calls;
    uint32_t tx_ethernet;
    uint32_t tx_udp;
    uint32_t tx_udp_errors;
    uint32_t ethernet_to_me;
    uint32_t ethernet_broadcast;
    uint32_t ethernet_other;
    uint32_t last_ethernet_len;
    uint32_t last_ethernet_type;
    uint32_t last_ip_src;
    uint32_t last_ip_dst;
    uint32_t last_ip_proto;
    uint32_t last_udp_src;
    uint32_t last_udp_dst;
    uint32_t last_udp_len;
} PicowNetStats;

int picow_net_init_join_dhcp(const PicowNetConfig *config, PicowNetInfo *info_out);
int picow_net_join_dhcp(const PicowNetConfig *config, PicowNetInfo *info_out);
void picow_net_poll(void);
int picow_net_send_ethernet(const uint8_t *frame, size_t length);
int picow_net_send_udp(const uint8_t dst_ip[4], uint16_t dst_port, uint16_t src_port, const void *payload, size_t payload_len);
int picow_net_arp_resolve(const uint8_t target_ip[4], uint8_t mac_out[6]);
const uint8_t *picow_net_next_hop_for(const uint8_t dst_ip[4]);
const PicowNetInfo *picow_net_info(void);
const PicowNetStats *picow_net_stats(void);
void picow_net_set_ipv4_handler(PicowNetIpv4Handler handler, void *user_data);
void picow_net_set_rx_trace_handler(PicowNetRxTraceHandler handler, void *user_data);
int picow_net_parse_ipv4(const char *text, uint8_t ip_out[4]);
void picow_net_format_ipv4(char *out, size_t out_size, const uint8_t ip[4]);

#endif