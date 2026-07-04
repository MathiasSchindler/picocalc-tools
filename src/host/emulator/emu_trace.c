#include "emu_trace.h"

static long trace_sys_write(int fd, const void *data, usize count) {
    long result;
    register long n __asm__("rax") = 1;
    register long a0 __asm__("rdi") = fd;
    register const void *a1 __asm__("rsi") = data;
    register usize a2 __asm__("rdx") = count;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static usize trace_str_len(const char *s) {
    usize n = 0;
    while (s != 0 && s[n] != 0) n += 1u;
    return n;
}

static int token_eq(const char *start, usize len, const char *word) {
    usize i;
    for (i = 0; i < len; ++i) {
        if (word[i] == 0 || start[i] != word[i]) return 0;
    }
    return word[len] == 0;
}

int emu_trace_parse_kinds(const char *s, u32 *out_mask) {
    u32 mask = 0;
    usize start = 0;
    usize pos = 0;
    while (1) {
        char ch = s[pos];
        if (ch == ',' || ch == '+' || ch == 0) {
            usize len = pos - start;
            if (len == 0u) return 0;
            if (token_eq(s + start, len, "base")) mask |= TRACE_BASE;
            else if (token_eq(s + start, len, "calls")) mask |= TRACE_CALLS;
            else if (token_eq(s + start, len, "unknown-mmio")) mask |= TRACE_UNKNOWN_MMIO;
            else if (token_eq(s + start, len, "xip")) mask |= TRACE_XIP;
            else if (token_eq(s + start, len, "cyw43")) mask |= TRACE_CYW43;
            else if (token_eq(s + start, len, "all")) mask |= TRACE_BASE | TRACE_CALLS | TRACE_UNKNOWN_MMIO | TRACE_XIP | TRACE_CYW43;
            else return 0;
            if (ch == 0) break;
            start = pos + 1u;
        }
        pos += 1u;
    }
    *out_mask = mask;
    return 1;
}

void emu_trace_text(EmuState *emu, const char *s) {
    if (emu->trace_fd < 0 || emu->trace_mask == 0u) return;
    (void)trace_sys_write(emu->trace_fd, s, trace_str_len(s));
}

int emu_trace_enabled(EmuState *emu, u32 kind) {
    return emu->trace_fd >= 0 && (emu->trace_mask & kind) != 0u;
}

void emu_trace_hex32(EmuState *emu, u32 value) {
    char buf[10];
    int i;
    if (emu->trace_fd < 0 || emu->trace_mask == 0u) return;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i) {
        u32 nibble = (value >> (28 - i * 4)) & 15u;
        buf[2 + i] = (char)(nibble < 10u ? '0' + nibble : 'a' + nibble - 10u);
    }
    (void)trace_sys_write(emu->trace_fd, buf, sizeof(buf));
}

void emu_trace_pair(EmuState *emu, const char *name, u32 value) {
    if (emu->trace_fd < 0 || emu->trace_mask == 0u) return;
    emu_trace_text(emu, name);
    emu_trace_text(emu, "=");
    emu_trace_hex32(emu, value);
}

void emu_trace_mmio(EmuState *emu, const char *kind, u32 addr, u32 value) {
    if (!emu_trace_enabled(emu, TRACE_BASE)) return;
    emu_trace_text(emu, kind);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "addr", addr);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "value", value);
    emu_trace_text(emu, " cycles=");
    emu_trace_hex32(emu, emu->cycles);
    emu_trace_text(emu, "\n");
}

void emu_trace_unknown_mmio(EmuState *emu, const char *kind, u32 addr, u32 value, u32 pc) {
    if (!emu_trace_enabled(emu, TRACE_UNKNOWN_MMIO)) return;
    if (pc == 0u) pc = emu->current_pc;
    emu_trace_text(emu, kind);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "addr", addr);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "value", value);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "pc", pc);
    emu_trace_text(emu, " cycles=");
    emu_trace_hex32(emu, emu->cycles);
    emu_trace_text(emu, "\n");
}

void emu_trace_xip_mmio(EmuState *emu, const char *kind, u32 addr, u32 value) {
    if (!emu_trace_enabled(emu, TRACE_XIP)) return;
    emu_trace_text(emu, kind);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "addr", addr);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "value", value);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "pc", emu->current_pc);
    emu_trace_text(emu, " cycles=");
    emu_trace_hex32(emu, emu->cycles);
    emu_trace_text(emu, "\n");
}

void emu_trace_branch(EmuState *emu, const char *kind, u32 pc, u32 target, u32 lr, u16 op) {
    if (!emu_trace_enabled(emu, TRACE_CALLS)) return;
    emu_trace_text(emu, kind);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "pc", pc);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "target", target);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "lr", lr);
    emu_trace_text(emu, " ");
    emu_trace_pair(emu, "op", (u32)op);
    emu_trace_text(emu, " cycles=");
    emu_trace_hex32(emu, emu->cycles);
    emu_trace_text(emu, "\n");
}