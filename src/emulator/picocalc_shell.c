#include "png_writer.h"

typedef unsigned char u8;
typedef unsigned long usize;

#define SCREEN_W 320
#define SCREEN_H 320
#define SCREEN_SCALE 2
#define OUT_W 900
#define OUT_H 1260

#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512

static u8 g_screen[SCREEN_W * SCREEN_H * 3u];
static u8 g_canvas[OUT_W * OUT_H * 3u];
static u8 g_png_work[PNG_RGB8_WORK_SIZE(OUT_W, OUT_H)];

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

void sys_exit(int status) {
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

static int byte_eq(const char *a, const char *b, usize n) {
    usize i;
    for (i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static void set_pixel(int x, int y, u8 r, u8 g, u8 b) {
    usize off;
    if (x < 0 || y < 0 || x >= OUT_W || y >= OUT_H) return;
    off = ((usize)y * OUT_W + (usize)x) * 3u;
    g_canvas[off + 0u] = r;
    g_canvas[off + 1u] = g;
    g_canvas[off + 2u] = b;
}

static void fill_rect(int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b) {
    int x;
    int y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= OUT_W) x1 = OUT_W - 1;
    if (y1 >= OUT_H) y1 = OUT_H - 1;
    for (y = y0; y <= y1; ++y) {
        for (x = x0; x <= x1; ++x) {
            set_pixel(x, y, r, g, b);
        }
    }
}

static void frame_rect(int x0, int y0, int x1, int y1, int t, u8 r, u8 g, u8 b) {
    fill_rect(x0, y0, x1, y0 + t - 1, r, g, b);
    fill_rect(x0, y1 - t + 1, x1, y1, r, g, b);
    fill_rect(x0, y0, x0 + t - 1, y1, r, g, b);
    fill_rect(x1 - t + 1, y0, x1, y1, r, g, b);
}

static void draw_screen(int x0, int y0) {
    int x;
    int y;
    for (y = 0; y < SCREEN_H; ++y) {
        for (x = 0; x < SCREEN_W; ++x) {
            usize src = ((usize)y * SCREEN_W + (usize)x) * 3u;
            int sx;
            int sy;
            for (sy = 0; sy < SCREEN_SCALE; ++sy) {
                for (sx = 0; sx < SCREEN_SCALE; ++sx) {
                    set_pixel(x0 + x * SCREEN_SCALE + sx,
                              y0 + y * SCREEN_SCALE + sy,
                              g_screen[src],
                              g_screen[src + 1u],
                              g_screen[src + 2u]);
                }
            }
        }
    }
}

static void draw_key(int x, int y, int w, int h, int accent) {
    fill_rect(x, y, x + w - 1, y + h - 1, 19, 23, 24);
    frame_rect(x, y, x + w - 1, y + h - 1, 2, 4, 6, 6);
    fill_rect(x + 5, y + 5, x + w - 7, y + h - 7, accent ? 35 : 48, accent ? 63 : 54, accent ? 46 : 58);
}

static void draw_key_grid(void) {
    int row;
    int col;
    int y = 914;
    for (row = 0; row < 6; ++row) {
        int x = row == 3 ? 118 : 90;
        int count = row == 5 ? 5 : 7;
        for (col = 0; col < count; ++col) {
            int w = row == 5 && col == 3 ? 220 : 74;
            draw_key(x, y, w, 54, ((row + col) & 3) == 0);
            x += w + 16;
        }
        y += 66;
    }
}

static void draw_dpad(void) {
    fill_rect(88, 794, 300, 862, 31, 36, 37);
    frame_rect(88, 794, 300, 862, 2, 13, 16, 16);
    draw_key(166, 724, 70, 80, 0);
    draw_key(166, 852, 70, 80, 0);
    draw_key(88, 794, 86, 68, 0);
    draw_key(226, 794, 86, 68, 0);
}

static void draw_shell(void) {
    fill_rect(0, 0, OUT_W - 1, OUT_H - 1, 210, 224, 225);
    fill_rect(40, 26, 860, 1232, 36, 43, 43);
    frame_rect(40, 26, 860, 1232, 8, 68, 78, 77);
    fill_rect(90, 80, 810, 782, 7, 9, 10);
    frame_rect(90, 80, 810, 782, 6, 92, 101, 99);
    fill_rect(130, 112, 769, 751, 0, 0, 0);
    draw_screen(130, 112);
    fill_rect(124, 44, 308, 66, 202, 208, 197);
    fill_rect(650, 44, 738, 58, 42, 48, 48);
    fill_rect(776, 156, 792, 172, 0, 220, 76);
    draw_dpad();
    draw_key_grid();
}

static int load_ppm(const char *path) {
    char header[15];
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize got_total = 0;
    if (fd < 0) return -1;
    if (sys_read((int)fd, header, sizeof(header)) != (long)sizeof(header)) return -1;
    if (!byte_eq(header, "P6\n320 320\n255\n", sizeof(header))) return -1;
    while (got_total < sizeof(g_screen)) {
        long got = sys_read((int)fd, g_screen + got_total, sizeof(g_screen) - got_total);
        if (got <= 0) return -1;
        got_total += (usize)got;
    }
    return 0;
}

static int write_ppm(const char *path) {
    static const char header[] = "P6\n900 1260\n255\n";
    long fd = sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    if (sys_write((int)fd, header, sizeof(header) - 1u) != (long)(sizeof(header) - 1u)) return -1;
    if (sys_write((int)fd, g_canvas, sizeof(g_canvas)) != (long)sizeof(g_canvas)) return -1;
    return 0;
}

static int png_fd_write(void *ctx, const void *data, png_usize count) {
    int fd = *(int *)ctx;
    return sys_write(fd, data, count) == (long)count ? 0 : -1;
}

static int str_ends(const char *s, const char *suffix) {
    usize s_len = 0;
    usize suffix_len = 0;
    usize i;
    while (s[s_len] != 0) s_len += 1u;
    while (suffix[suffix_len] != 0) suffix_len += 1u;
    if (suffix_len > s_len) return 0;
    for (i = 0; i < suffix_len; ++i) if (s[s_len - suffix_len + i] != suffix[i]) return 0;
    return 1;
}

static int write_png(const char *path) {
    int fd = (int)sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    return png_write_rgb8(png_fd_write, &fd, g_canvas, OUT_W, OUT_H, g_png_work, sizeof(g_png_work));
}

static int write_image(const char *path) {
    if (str_ends(path, ".ppm")) return write_ppm(path);
    return write_png(path);
}

__attribute__((used)) int shell_main(int argc, char **argv) {
    if (argc < 3) {
        out("usage: picocalc_shell input-screen.ppm output-device.png\n");
        return 1;
    }
    if (load_ppm(argv[1]) != 0) {
        out("failed to load 320x320 PPM\n");
        return 1;
    }
    draw_shell();
    if (write_image(argv[2]) != 0) {
        out("failed to write device image\n");
        return 1;
    }
    out("wrote "); out(argv[2]); out("\n");
    return 0;
}

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "and $-16, %rsp\n"
        "call shell_main\n"
        "mov %eax, %edi\n"
        "call sys_exit\n");
}