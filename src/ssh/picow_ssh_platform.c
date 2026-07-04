#include "platform.h"

#include "pico/rand.h"
#include "pico/stdlib.h"

#include <string.h>

void __attribute__((weak)) picow_ssh_write_console(const char *text, size_t length) {
    (void)text;
    (void)length;
}

const char *platform_getenv(const char *name) {
    (void)name;
    return 0;
}

int platform_isatty(int fd) {
    return fd >= 0 && fd <= 2;
}

long platform_read(int fd, void *buffer, size_t count) {
    (void)fd;
    (void)buffer;
    (void)count;
    return -1;
}

long platform_write(int fd, const void *buffer, size_t count) {
    (void)fd;
    picow_ssh_write_console((const char *)buffer, count);
    return (long)count;
}

int platform_close(int fd) {
    (void)fd;
    return 0;
}

int platform_connect_tcp(const char *host, unsigned int port, int *socket_fd_out) {
    (void)host;
    (void)port;
    if (socket_fd_out != 0) *socket_fd_out = -1;
    return -1;
}

int platform_poll_fds(const int *fds, size_t fd_count, size_t *ready_index_out, int timeout_milliseconds) {
    (void)fds;
    (void)fd_count;
    if (ready_index_out != 0) *ready_index_out = 0;
    if (timeout_milliseconds > 0) sleep_ms((uint32_t)timeout_milliseconds);
    return 0;
}

int platform_random_bytes(unsigned char *buffer, size_t count) {
    size_t i = 0;
    if (buffer == 0) return -1;
    while (i < count) {
        uint32_t value = get_rand_32();
        for (unsigned int j = 0; j < 4u && i < count; ++j) {
            buffer[i++] = (unsigned char)(value >> (j * 8u));
        }
    }
    return 0;
}

int platform_terminal_enable_raw_mode(int fd, PlatformTerminalState *state_out) {
    (void)fd;
    if (state_out != 0) memset(state_out, 0, sizeof(*state_out));
    return 0;
}

int platform_terminal_restore_mode(int fd, const PlatformTerminalState *state) {
    (void)fd;
    (void)state;
    return 0;
}

int platform_ignore_signal(int signal_number) {
    (void)signal_number;
    return 0;
}

int platform_open_read(const char *path) {
    (void)path;
    return -1;
}

int platform_open_read_secure(const char *path, PlatformDirEntry *entry_out) {
    (void)path;
    (void)entry_out;
    return -1;
}

int platform_open_append(const char *path, unsigned int mode) {
    (void)path;
    (void)mode;
    return -1;
}

int platform_make_directory(const char *path, unsigned int mode) {
    (void)path;
    (void)mode;
    return -1;
}

int platform_path_is_directory(const char *path, int *is_directory_out) {
    (void)path;
    if (is_directory_out != 0) *is_directory_out = 0;
    return -1;
}

int platform_get_identity(PlatformIdentity *identity_out) {
    if (identity_out == 0) return -1;
    memset(identity_out, 0, sizeof(*identity_out));
    return 0;
}

int platform_get_path_info(const char *path, PlatformDirEntry *entry_out) {
    (void)path;
    (void)entry_out;
    return -1;
}

int platform_read_symlink(const char *path, char *buffer, size_t buffer_size) {
    (void)path;
    (void)buffer;
    (void)buffer_size;
    return -1;
}