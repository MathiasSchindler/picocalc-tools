#ifndef PICOW_NET_H
#define PICOW_NET_H

#include <stddef.h>
#include <stdint.h>

typedef void (*PicowNetLogFn)(void *user_data, const char *message);
typedef void (*PicowNetIpv4Handler)(const uint8_t *packet, size_t length, void *user_data);

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

int picow_net_init_join_dhcp(const PicowNetConfig *config, PicowNetInfo *info_out);
void picow_net_poll(void);
int picow_net_send_ethernet(const uint8_t *frame, size_t length);
int picow_net_arp_resolve(const uint8_t target_ip[4], uint8_t mac_out[6]);
const uint8_t *picow_net_next_hop_for(const uint8_t dst_ip[4]);
const PicowNetInfo *picow_net_info(void);
void picow_net_set_ipv4_handler(PicowNetIpv4Handler handler, void *user_data);
int picow_net_parse_ipv4(const char *text, uint8_t ip_out[4]);
void picow_net_format_ipv4(char *out, size_t out_size, const uint8_t ip[4]);

#endif