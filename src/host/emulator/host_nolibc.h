#ifndef PICOCALC_HOST_NOLIBC_H
#define PICOCALC_HOST_NOLIBC_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long usize;

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512
#define O_NONBLOCK 2048
#define F_GETFL 3
#define F_SETFL 4
#define TCGETS 0x5401u
#define TCSETS 0x5402u
#define TERM_ICANON 0000002u
#define TERM_ECHO 0000010u
#define TERM_VTIME 5
#define TERM_VMIN 6

typedef struct {
    u32 c_iflag;
    u32 c_oflag;
    u32 c_cflag;
    u32 c_lflag;
    u8 c_line;
    u8 c_cc[32];
    u32 c_ispeed;
    u32 c_ospeed;
} Termios;

static long sys_read(int fd, void *data, usize count) {
    long result;
    register long n __asm__("rax") = 0;
    register long a0 __asm__("rdi") = fd;
    register void *a1 __asm__("rsi") = data;
    register usize a2 __asm__("rdx") = count;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static long sys_write(int fd, const void *data, usize count) {
    long result;
    register long n __asm__("rax") = 1;
    register long a0 __asm__("rdi") = fd;
    register const void *a1 __asm__("rsi") = data;
    register usize a2 __asm__("rdx") = count;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static long sys_close(int fd) {
    long result;
    register long n __asm__("rax") = 3;
    register long a0 __asm__("rdi") = fd;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0) : "rcx", "r11", "memory");
    return result;
}

static long sys_openat(int dirfd, const char *path, int flags, int mode) {
    long result;
    register long n __asm__("rax") = 257;
    register long a0 __asm__("rdi") = dirfd;
    register const char *a1 __asm__("rsi") = path;
    register long a2 __asm__("rdx") = flags;
    register long a3 __asm__("r10") = mode;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2), "r"(a3) : "rcx", "r11", "memory");
    return result;
}

static long sys_fcntl(int fd, int cmd, long arg) {
    long result;
    register long n __asm__("rax") = 72;
    register long a0 __asm__("rdi") = fd;
    register long a1 __asm__("rsi") = cmd;
    register long a2 __asm__("rdx") = arg;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static long sys_ioctl(int fd, unsigned long request, void *arg) {
    long result;
    register long n __asm__("rax") = 16;
    register long a0 __asm__("rdi") = fd;
    register unsigned long a1 __asm__("rsi") = request;
    register void *a2 __asm__("rdx") = arg;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static __attribute__((used, noreturn)) void sys_exit(int status) {
    register long n __asm__("rax") = 60;
    register long a0 __asm__("rdi") = status;
    __asm__ volatile("syscall" : : "a"(n), "r"(a0) : "rcx", "r11", "memory");
    while (1) {}
}

static void out(const char *s) {
    usize n = 0;
    while (s[n] != 0) n += 1u;
    (void)sys_write(1, s, n);
}

static void out_hex(u32 value) {
    char buf[10];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i) {
        u32 nibble = (value >> (28 - i * 4)) & 15u;
        buf[2 + i] = (char)(nibble < 10u ? '0' + nibble : 'a' + nibble - 10u);
    }
    (void)sys_write(1, buf, sizeof(buf));
}

static usize str_len(const char *s) {
    usize n = 0;
    while (s != 0 && s[n] != 0) n += 1u;
    return n;
}

static int str_eq(const char *a, const char *b) {
    usize i = 0;
    while (a[i] != 0 && b[i] != 0) {
        if (a[i] != b[i]) return 0;
        i += 1u;
    }
    return a[i] == b[i];
}

static int str_starts(const char *s, const char *prefix) {
    usize i = 0;
    while (prefix[i] != 0) {
        if (s[i] != prefix[i]) return 0;
        i += 1u;
    }
    return 1;
}

static int str_ends(const char *s, const char *suffix) {
    usize s_len = str_len(s);
    usize suffix_len = str_len(suffix);
    usize i;
    if (suffix_len > s_len) return 0;
    for (i = 0; i < suffix_len; ++i) {
        if (s[s_len - suffix_len + i] != suffix[i]) return 0;
    }
    return 1;
}

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int parse_dec(const char *s, int *out_value) {
    int value = 0;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        any = 1;
        s += 1;
    }
    if (!any || *s != 0) return 0;
    *out_value = value;
    return 1;
}

static int parse_hex32(const char *s, u32 *out_value) {
    u32 value = 0;
    int any = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s != 0) {
        int digit = hex_value(*s);
        if (digit < 0) return 0;
        value = (value << 4) | (u32)digit;
        any = 1;
        s += 1;
    }
    if (!any) return 0;
    *out_value = value;
    return 1;
}

#endif
