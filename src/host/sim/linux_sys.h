#ifndef PICOCALC_SIM_LINUX_SYS_H
#define PICOCALC_SIM_LINUX_SYS_H

#include "runtime.h"

long sim_linux_read(int fd, void *data, size_t count);
long sim_linux_write(int fd, const void *data, size_t count);
long sim_linux_openat(int dirfd, const char *path, int flags, int mode);
long sim_linux_close(int fd);
void sim_linux_exit(int status) __attribute__((noreturn));

#endif