#include <stddef.h>

int solve_main(int argc, char **argv);

int main(int argc, char **argv) {
    return solve_main(argc, argv);
}

long solve_platform_write(int fd, const void *data, size_t count) {
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

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "and $-16, %rsp\n"
        "call main\n"
        "mov %eax, %edi\n"
        "mov $60, %eax\n"
        "syscall\n");
}