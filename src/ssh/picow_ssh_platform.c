#include "platform.h"

#include "i2ckbd.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "picow_tcp.h"

#include <stdio.h>
#include <string.h>

#define PICOW_SSH_KEY_BACKSPACE 0x08
#define PICOW_SSH_KEY_ENTER     0x0a
#define PICOW_SSH_KEY_ESC       0xb1
#define PICOW_SSH_KEY_DEL       0xd4

static int g_pending_stdin = -1;

static int platform_translate_key(int key) {
    if (key == PICOW_SSH_KEY_ENTER) return '\n';
    if (key == PICOW_SSH_KEY_BACKSPACE || key == PICOW_SSH_KEY_DEL) return 0x7f;
    if (key == PICOW_SSH_KEY_ESC) return 27;
    if (key >= 0 && key < 256) return key;
    return -1;
}

static int platform_poll_stdin(void) {
    int key;
    if (g_pending_stdin >= 0) return 1;
    key = platform_translate_key(read_i2c_kbd());
    if (key < 0) return 0;
    g_pending_stdin = key;
    return 1;
}

void __attribute__((weak)) picow_ssh_write_console(const char *text, size_t length) {
    (void)text;
    (void)length;
}

static void platform_log_tcp_debug(const char *prefix) {
    PicowTcpDebug debug;
    char line[160];
    picow_tcp_get_debug(&debug);
    snprintf(line, sizeof(line),
             "\n[%s st=%d rx=%u ovf=%u drop=%u retx=%u wto=%u rto=%u rst=%u]\n",
             prefix,
             debug.state,
             (unsigned int)debug.rx_len,
             (unsigned int)debug.rx_overflow_events,
             (unsigned int)debug.rx_dropped_bytes,
             (unsigned int)debug.tx_retransmits,
             (unsigned int)debug.write_ack_timeouts,
             (unsigned int)debug.read_timeouts,
             (unsigned int)debug.resets);
    picow_ssh_write_console(line, strlen(line));
}

const char *platform_getenv(const char *name) {
    (void)name;
    return 0;
}

int platform_isatty(int fd) {
    return fd >= 0 && fd <= 2;
}

long platform_read(int fd, void *buffer, size_t count) {
    if (picow_tcp_is_socket_fd(fd)) {
        long ret = picow_tcp_read(fd, buffer, count);
        if (ret <= 0) platform_log_tcp_debug(ret == 0 ? "tcp closed" : "tcp read fail");
        return ret;
    }
    if (fd == 0 && buffer != 0 && count != 0U) {
        unsigned char *out = (unsigned char *)buffer;
        size_t done = 0U;

        while (done < count) {
            if (g_pending_stdin < 0 && !platform_poll_stdin()) break;
            out[done++] = (unsigned char)g_pending_stdin;
            g_pending_stdin = -1;
        }
        return (long)done;
    }
    return -1;
}

long platform_write(int fd, const void *buffer, size_t count) {
    if (picow_tcp_is_socket_fd(fd)) {
        long ret = picow_tcp_write(fd, buffer, count);
        if (ret < 0 || (size_t)ret != count) platform_log_tcp_debug("tcp write fail");
        return ret;
    }
    (void)fd;
    picow_ssh_write_console((const char *)buffer, count);
    return (long)count;
}

int platform_close(int fd) {
    if (picow_tcp_is_socket_fd(fd)) return picow_tcp_close(fd);
    return 0;
}

int platform_connect_tcp(const char *host, unsigned int port, int *socket_fd_out) {
    return picow_tcp_connect(host, port, socket_fd_out);
}

int platform_poll_fds(const int *fds, size_t fd_count, size_t *ready_index_out, int timeout_milliseconds) {
    size_t i;
    for (i = 0; i < fd_count; ++i) {
        if (picow_tcp_is_socket_fd(fds[i]) && picow_tcp_poll_fd(fds[i], 0) > 0) {
            if (ready_index_out != 0) *ready_index_out = i;
            return 1;
        }
        if (fds[i] == 0 && platform_poll_stdin()) {
            if (ready_index_out != 0) *ready_index_out = i;
            return 1;
        }
    }
    if (fd_count == 1 && picow_tcp_is_socket_fd(fds[0])) {
        int ret = picow_tcp_poll_fd(fds[0], timeout_milliseconds);
        if (ret > 0 && ready_index_out != 0) *ready_index_out = 0;
        return ret;
    }
    if (timeout_milliseconds != 0) {
        absolute_time_t deadline = timeout_milliseconds < 0 ? at_the_end_of_time : make_timeout_time_ms((uint32_t)timeout_milliseconds);
        while (timeout_milliseconds < 0 || !time_reached(deadline)) {
            for (i = 0; i < fd_count; ++i) {
                if (picow_tcp_is_socket_fd(fds[i]) && picow_tcp_poll_fd(fds[i], 0) > 0) {
                    if (ready_index_out != 0) *ready_index_out = i;
                    return 1;
                }
                if (fds[i] == 0 && platform_poll_stdin()) {
                    if (ready_index_out != 0) *ready_index_out = i;
                    return 1;
                }
            }
            sleep_ms(2);
        }
    }
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