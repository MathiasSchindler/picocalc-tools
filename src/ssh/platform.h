#ifndef PICOCALC_SSH_PLATFORM_H
#define PICOCALC_SSH_PLATFORM_H

#include <stddef.h>

#define PLATFORM_NAME_CAPACITY 256
#define PLATFORM_TERMINAL_STATE_CAPACITY 128

typedef struct {
    char name[PLATFORM_NAME_CAPACITY];
    unsigned long long device;
    unsigned int mode;
    unsigned int uid;
    unsigned int gid;
    unsigned long long size;
    unsigned long long inode;
    unsigned long nlink;
    long long atime;
    long long mtime;
    long long ctime;
    unsigned int atime_nanos;
    unsigned int mtime_nanos;
    unsigned int ctime_nanos;
    char owner[32];
    char group[32];
    int is_dir;
    int is_hidden;
} PlatformDirEntry;

typedef struct {
    unsigned int uid;
    unsigned int gid;
    char username[PLATFORM_NAME_CAPACITY];
    char groupname[PLATFORM_NAME_CAPACITY];
} PlatformIdentity;

typedef struct {
    unsigned char bytes[PLATFORM_TERMINAL_STATE_CAPACITY];
} PlatformTerminalState;

const char *platform_getenv(const char *name);
int platform_isatty(int fd);
long platform_read(int fd, void *buffer, size_t count);
long platform_write(int fd, const void *buffer, size_t count);
int platform_close(int fd);
int platform_connect_tcp(const char *host, unsigned int port, int *socket_fd_out);
int platform_poll_fds(const int *fds, size_t fd_count, size_t *ready_index_out, int timeout_milliseconds);
int platform_random_bytes(unsigned char *buffer, size_t count);
int platform_terminal_enable_raw_mode(int fd, PlatformTerminalState *state_out);
int platform_terminal_restore_mode(int fd, const PlatformTerminalState *state);
int platform_ignore_signal(int signal_number);

int platform_open_read(const char *path);
int platform_open_read_secure(const char *path, PlatformDirEntry *entry_out);
int platform_open_append(const char *path, unsigned int mode);
int platform_make_directory(const char *path, unsigned int mode);
int platform_path_is_directory(const char *path, int *is_directory_out);
int platform_get_identity(PlatformIdentity *identity_out);
int platform_get_path_info(const char *path, PlatformDirEntry *entry_out);
int platform_read_symlink(const char *path, char *buffer, size_t buffer_size);

void picow_ssh_write_console(const char *text, size_t length);

#endif