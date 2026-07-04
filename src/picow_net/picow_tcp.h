#ifndef PICOW_TCP_H
#define PICOW_TCP_H

#include <stddef.h>

int picow_tcp_connect(const char *host, unsigned int port, int *socket_fd_out);
long picow_tcp_read(int fd, void *buffer, size_t count);
long picow_tcp_write(int fd, const void *buffer, size_t count);
int picow_tcp_close(int fd);
int picow_tcp_poll_fd(int fd, int timeout_milliseconds);
int picow_tcp_is_socket_fd(int fd);

#endif