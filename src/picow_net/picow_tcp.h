#ifndef PICOW_TCP_H
#define PICOW_TCP_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	int state;
	size_t rx_len;
	uint32_t rx_segments;
	uint32_t rx_bytes;
	uint32_t rx_overflow_events;
	uint32_t rx_dropped_bytes;
	uint32_t tx_segments;
	uint32_t tx_retransmits;
	uint32_t write_ack_timeouts;
	uint32_t read_timeouts;
	uint32_t resets;
} PicowTcpDebug;

int picow_tcp_connect(const char *host, unsigned int port, int *socket_fd_out);
long picow_tcp_read(int fd, void *buffer, size_t count);
long picow_tcp_write(int fd, const void *buffer, size_t count);
int picow_tcp_close(int fd);
int picow_tcp_poll_fd(int fd, int timeout_milliseconds);
int picow_tcp_is_socket_fd(int fd);
void picow_tcp_get_debug(PicowTcpDebug *debug_out);

#endif