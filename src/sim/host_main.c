#include "linux_sys.h"

void bare_main(void);
void picocalc_lcd_sim_flush(void);
void picocalc_lcd_sim_write_ppm(const char *path);
const char *picocalc_lcd_sim_ppm_path(void);
int picocalc_lcd_sim_flush_at_exit(void);

long sim_linux_read(int fd, void *data, size_t count) {
    long result;
    register long syscall_number __asm__("rax") = 0;
    register long arg0 __asm__("rdi") = fd;
    register void *arg1 __asm__("rsi") = data;
    register size_t arg2 __asm__("rdx") = count;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(syscall_number), "r"(arg0), "r"(arg1), "r"(arg2)
                     : "rcx", "r11", "memory");
    return result;
}

long sim_linux_write(int fd, const void *data, size_t count) {
    long result;
    register long syscall_number __asm__("rax") = 1;
    register long arg0 __asm__("rdi") = fd;
    register const void *arg1 __asm__("rsi") = data;
    register size_t arg2 __asm__("rdx") = count;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(syscall_number), "r"(arg0), "r"(arg1), "r"(arg2)
                     : "rcx", "r11", "memory");
    return result;
}

long sim_linux_openat(int dirfd, const char *path, int flags, int mode) {
    long result;
    register long syscall_number __asm__("rax") = 257;
    register long arg0 __asm__("rdi") = dirfd;
    register const char *arg1 __asm__("rsi") = path;
    register long arg2 __asm__("rdx") = flags;
    register long arg3 __asm__("r10") = mode;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(syscall_number), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3)
                     : "rcx", "r11", "memory");
    return result;
}

long sim_linux_close(int fd) {
    long result;
    register long syscall_number __asm__("rax") = 3;
    register long arg0 __asm__("rdi") = fd;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(syscall_number), "r"(arg0)
                     : "rcx", "r11", "memory");
    return result;
}

void sim_linux_exit(int status) {
    register long syscall_number __asm__("rax") = 60;
    register long arg0 __asm__("rdi") = status;
    __asm__ volatile("syscall"
                     :
                     : "a"(syscall_number), "r"(arg0)
                     : "rcx", "r11", "memory");
    while (1) {
    }
}

__attribute__((weak)) const char *picocalc_lcd_sim_ppm_path(void) {
    return "build/sim/sim_solve_fixed.ppm";
}

__attribute__((weak)) int picocalc_lcd_sim_flush_at_exit(void) {
    return 1;
}

__attribute__((used)) int sim_main(void) {
    const char *ppm_path;
    bare_main();
    if (picocalc_lcd_sim_flush_at_exit() != 0) {
        picocalc_lcd_sim_flush();
    }
    ppm_path = picocalc_lcd_sim_ppm_path();
    if (ppm_path != 0) {
        picocalc_lcd_sim_write_ppm(ppm_path);
    }
    return 0;
}

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile(
        "and $-16, %rsp\n"
        "call sim_main\n"
        "mov %eax, %edi\n"
        "call sim_linux_exit\n");
}