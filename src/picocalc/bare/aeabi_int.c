typedef unsigned int u32;

typedef struct {
    u32 lo;
    u32 hi;
} U64Parts;

static U64Parts g_div_n;
static U64Parts g_div_d;
static U64Parts g_div_q;
static U64Parts g_div_r;

static int u64_is_zero(U64Parts value) {
    return value.lo == 0u && value.hi == 0u;
}

static U64Parts u64_neg(U64Parts value) {
    U64Parts out;
    out.lo = ~value.lo + 1u;
    out.hi = ~value.hi + (out.lo == 0u ? 1u : 0u);
    return out;
}

static int u64_ge(U64Parts left, U64Parts right) {
    if (left.hi != right.hi) return left.hi > right.hi;
    return left.lo >= right.lo;
}

static U64Parts u64_sub(U64Parts left, U64Parts right) {
    U64Parts out;
    out.lo = left.lo - right.lo;
    out.hi = left.hi - right.hi - (left.lo < right.lo ? 1u : 0u);
    return out;
}

static U64Parts u64_shl1(U64Parts value) {
    U64Parts out;
    out.hi = (value.hi << 1) | (value.lo >> 31);
    out.lo = value.lo << 1;
    return out;
}

static u32 u64_bit(U64Parts value, int bit) {
    if (bit < 32) return (value.lo >> bit) & 1u;
    return (value.hi >> (bit - 32)) & 1u;
}

static void u64_set_bit(U64Parts *value, int bit) {
    if (bit < 32) value->lo |= 1u << bit;
    else value->hi |= 1u << (bit - 32);
}

__attribute__((used)) static void udivmod64_compute(void) {
    U64Parts q = {0u, 0u};
    U64Parts r = {0u, 0u};
    if (u64_is_zero(g_div_d)) {
        g_div_q.lo = 0xffffffffu;
        g_div_q.hi = 0xffffffffu;
        g_div_r = g_div_n;
        return;
    }
    for (int bit = 63; bit >= 0; --bit) {
        r = u64_shl1(r);
        r.lo |= u64_bit(g_div_n, bit);
        if (u64_ge(r, g_div_d)) {
            r = u64_sub(r, g_div_d);
            u64_set_bit(&q, bit);
        }
    }
    g_div_q = q;
    g_div_r = r;
}

__attribute__((used)) static void ldivmod64_compute(void) {
    int n_neg = (g_div_n.hi & 0x80000000u) != 0u;
    int d_neg = (g_div_d.hi & 0x80000000u) != 0u;
    if (n_neg) g_div_n = u64_neg(g_div_n);
    if (d_neg) g_div_d = u64_neg(g_div_d);
    udivmod64_compute();
    if (n_neg != d_neg) g_div_q = u64_neg(g_div_q);
    if (n_neg) g_div_r = u64_neg(g_div_r);
}

unsigned long long __aeabi_lmul(unsigned long long left, unsigned long long right) {
    union { unsigned long long whole; U64Parts parts; } a;
    union { unsigned long long whole; U64Parts parts; } b;
    union { unsigned long long whole; U64Parts parts; } out;
    u32 a0;
    u32 a1;
    u32 b0;
    u32 b1;
    u32 p0;
    u32 p1;
    u32 p2;
    u32 p3;
    u32 carry;
    a.whole = left;
    b.whole = right;
    a0 = a.parts.lo & 0xffffu;
    a1 = a.parts.lo >> 16;
    b0 = b.parts.lo & 0xffffu;
    b1 = b.parts.lo >> 16;
    p0 = a0 * b0;
    p1 = a1 * b0;
    p2 = a0 * b1;
    p3 = a1 * b1;
    carry = (p0 >> 16) + (p1 & 0xffffu) + (p2 & 0xffffu);
    out.parts.lo = (p0 & 0xffffu) | (carry << 16);
    out.parts.hi = (p1 >> 16) + (p2 >> 16) + p3 + (carry >> 16) + a.parts.lo * b.parts.hi + a.parts.hi * b.parts.lo;
    return out.whole;
}

__attribute__((naked)) void __aeabi_uldivmod(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "push {r4, lr}\n"
        "ldr r4, =g_div_n\n"
        "str r0, [r4, #0]\n"
        "str r1, [r4, #4]\n"
        "ldr r4, =g_div_d\n"
        "str r2, [r4, #0]\n"
        "str r3, [r4, #4]\n"
        "bl udivmod64_compute\n"
        "ldr r4, =g_div_q\n"
        "ldr r0, [r4, #0]\n"
        "ldr r1, [r4, #4]\n"
        "ldr r4, =g_div_r\n"
        "ldr r2, [r4, #0]\n"
        "ldr r3, [r4, #4]\n"
        "pop {r4, pc}\n"
    );
}

__attribute__((naked)) void __aeabi_ldivmod(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "push {r4, lr}\n"
        "ldr r4, =g_div_n\n"
        "str r0, [r4, #0]\n"
        "str r1, [r4, #4]\n"
        "ldr r4, =g_div_d\n"
        "str r2, [r4, #0]\n"
        "str r3, [r4, #4]\n"
        "bl ldivmod64_compute\n"
        "ldr r4, =g_div_q\n"
        "ldr r0, [r4, #0]\n"
        "ldr r1, [r4, #4]\n"
        "ldr r4, =g_div_r\n"
        "ldr r2, [r4, #0]\n"
        "ldr r3, [r4, #4]\n"
        "pop {r4, pc}\n"
    );
}

__attribute__((naked)) void __gnu_thumb1_case_uqi(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "push {r1}\n"
        "mov r1, lr\n"
        "lsrs r1, r1, #1\n"
        "lsls r1, r1, #1\n"
        "ldrb r1, [r1, r0]\n"
        "lsls r1, r1, #1\n"
        "add lr, r1\n"
        "pop {r1}\n"
        "bx lr\n"
    );
}

__attribute__((naked)) void __aeabi_llsl(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "cmp r2, #0\n"
        "beq 2f\n"
        "1:\n"
        "lsls r0, r0, #1\n"
        "adcs r1, r1\n"
        "subs r2, r2, #1\n"
        "bne 1b\n"
        "2:\n"
        "bx lr\n"
    );
}

__attribute__((naked)) void __aeabi_llsr(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "cmp r2, #0\n"
        "beq 3f\n"
        "1:\n"
        "movs r3, #1\n"
        "ands r3, r1\n"
        "lsrs r1, r1, #1\n"
        "lsrs r0, r0, #1\n"
        "cmp r3, #0\n"
        "beq 2f\n"
        "movs r3, #128\n"
        "lsls r3, r3, #24\n"
        "orrs r0, r3\n"
        "2:\n"
        "subs r2, r2, #1\n"
        "bne 1b\n"
        "3:\n"
        "bx lr\n"
    );
}
