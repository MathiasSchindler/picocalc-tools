#include "picocalc_lcd_bare.h"

typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;

typedef union {
    double d;
    u64 u;
} D64;

static u32 arithmetic_fail_mask;

u64 __aeabi_i2d(int value);
u64 __aeabi_ui2d(u32 value);
u64 __aeabi_l2d(s64 value);
u64 __aeabi_ul2d(u64 value);
int __aeabi_d2iz(u64 bits);
u32 __aeabi_d2uiz(u64 bits);
s64 __aeabi_d2lz(u64 bits);
u64 __aeabi_d2ulz(u64 bits);
int __aeabi_dcmpeq(u64 left, u64 right);
int __aeabi_dcmplt(u64 left, u64 right);
int __aeabi_dcmple(u64 left, u64 right);
int __aeabi_dcmpgt(u64 left, u64 right);
int __aeabi_dcmpge(u64 left, u64 right);
u64 __aeabi_dadd(u64 left, u64 right);
u64 __aeabi_dsub(u64 left, u64 right);
u64 __aeabi_dmul(u64 left, u64 right);
u64 __aeabi_ddiv(u64 left, u64 right);

static D64 d_from_double(double value) {
    D64 out;
    out.d = value;
    return out;
}

static int check_u64(u64 got, u64 expected) {
    return got == expected;
}

static int check_i32(int got, int expected) {
    return got == expected;
}

static int check_u32(u32 got, u32 expected) {
    return got == expected;
}

static int check_s64(s64 got, s64 expected) {
    return got == expected;
}

static int check_bool(int got, int expected) {
    return (got != 0) == expected;
}

static int check_nan(u64 bits) {
    return (bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0x000fffffffffffffull) != 0ull;
}

static int check_arith(int bit, int passed) {
    if (!passed) arithmetic_fail_mask |= 1u << bit;
    return passed;
}

static int conversion_tests(void) {
    int ok = 1;
    ok &= check_u64(__aeabi_i2d(0), d_from_double(0.0).u);
    ok &= check_u64(__aeabi_i2d(1), d_from_double(1.0).u);
    ok &= check_u64(__aeabi_i2d(-1), d_from_double(-1.0).u);
    ok &= check_u64(__aeabi_i2d(123456789), d_from_double(123456789.0).u);
    ok &= check_u64(__aeabi_i2d(-2147483647 - 1), d_from_double(-2147483648.0).u);
    ok &= check_u64(__aeabi_ui2d(0xffffffffu), d_from_double(4294967295.0).u);
    ok &= check_u64(__aeabi_l2d(0x123456789abcll), d_from_double(0x123456789abcll).u);
    ok &= check_u64(__aeabi_l2d(-0x123456789abcll), d_from_double(-0x123456789abcll).u);
    ok &= check_u64(__aeabi_ul2d(9007199254740992ull), d_from_double(9007199254740992.0).u);
    ok &= check_u64(__aeabi_ul2d(9007199254740993ull), d_from_double(9007199254740992.0).u);
    ok &= check_u64(__aeabi_ul2d(9007199254740994ull), d_from_double(9007199254740994.0).u);
    ok &= check_u64(__aeabi_ul2d(0xffffffffffffffffull), d_from_double(18446744073709551615.0).u);
    ok &= check_i32(__aeabi_d2iz(d_from_double(12345.75).u), 12345);
    ok &= check_i32(__aeabi_d2iz(d_from_double(-12345.75).u), -12345);
    ok &= check_i32(__aeabi_d2iz(d_from_double(2147483648.0).u), 2147483647);
    ok &= check_i32(__aeabi_d2iz(d_from_double(-2147483649.0).u), -2147483647 - 1);
    ok &= check_u32(__aeabi_d2uiz(d_from_double(4000000000.0).u), 4000000000u);
    ok &= check_u32(__aeabi_d2uiz(d_from_double(4294967296.0).u), 0xffffffffu);
    ok &= check_s64(__aeabi_d2lz(d_from_double(0x123456789abcll).u), 0x123456789abcll);
    ok &= check_s64(__aeabi_d2lz(d_from_double(-0x123456789abcll).u), -0x123456789abcll);
    ok &= check_u64(__aeabi_d2ulz(d_from_double(0x123456789abcll).u), 0x123456789abcll);
    ok &= check_s64(__aeabi_d2lz(0x7ff0000000000000ull), 0x7fffffffffffffffll);
    ok &= check_s64(__aeabi_d2lz(0xfff0000000000000ull), (s64)0x8000000000000000ull);
    ok &= check_u64(__aeabi_d2ulz(0x7ff0000000000000ull), 0xffffffffffffffffull);
    ok &= check_i32(__aeabi_d2iz(0x7ff8000000000000ull), 0);
    ok &= check_u32(__aeabi_d2uiz(0xbff0000000000000ull), 0u);
    return ok;
}

static int comparison_tests(void) {
    u64 neg_zero = 0x8000000000000000ull;
    u64 pos_zero = 0x0000000000000000ull;
    u64 one = d_from_double(1.0).u;
    u64 two = d_from_double(2.0).u;
    u64 minus_one = d_from_double(-1.0).u;
    u64 inf = 0x7ff0000000000000ull;
    u64 minus_inf = 0xfff0000000000000ull;
    u64 min_subnormal = 0x0000000000000001ull;
    u64 nan = 0x7ff8000000000001ull;
    int ok = 1;
    ok &= check_bool(__aeabi_dcmpeq(pos_zero, neg_zero), 1);
    ok &= check_bool(__aeabi_dcmpeq(one, one), 1);
    ok &= check_bool(__aeabi_dcmpeq(one, two), 0);
    ok &= check_bool(__aeabi_dcmpeq(nan, nan), 0);
    ok &= check_bool(__aeabi_dcmplt(minus_one, pos_zero), 1);
    ok &= check_bool(__aeabi_dcmplt(one, two), 1);
    ok &= check_bool(__aeabi_dcmplt(two, one), 0);
    ok &= check_bool(__aeabi_dcmplt(minus_inf, minus_one), 1);
    ok &= check_bool(__aeabi_dcmplt(pos_zero, min_subnormal), 1);
    ok &= check_bool(__aeabi_dcmplt(nan, one), 0);
    ok &= check_bool(__aeabi_dcmple(one, one), 1);
    ok &= check_bool(__aeabi_dcmple(two, one), 0);
    ok &= check_bool(__aeabi_dcmple(nan, one), 0);
    ok &= check_bool(__aeabi_dcmpgt(two, one), 1);
    ok &= check_bool(__aeabi_dcmpgt(inf, two), 1);
    ok &= check_bool(__aeabi_dcmpgt(one, two), 0);
    ok &= check_bool(__aeabi_dcmpgt(nan, one), 0);
    ok &= check_bool(__aeabi_dcmpge(one, one), 1);
    ok &= check_bool(__aeabi_dcmpge(one, two), 0);
    ok &= check_bool(__aeabi_dcmpge(nan, one), 0);
    return ok;
}

static int arithmetic_tests(void) {
    u64 neg_zero = 0x8000000000000000ull;
    u64 one = 0x3ff0000000000000ull;
    u64 two = 0x4000000000000000ull;
    u64 three = 0x4008000000000000ull;
    u64 six = 0x4018000000000000ull;
    u64 minus_one = 0xbff0000000000000ull;
    u64 minus_two = 0xc000000000000000ull;
    u64 minus_six = 0xc018000000000000ull;
    u64 half = 0x3fe0000000000000ull;
    u64 one_third = 0x3fd5555555555555ull;
    u64 two_to_53 = 0x4340000000000000ull;
    u64 min_subnormal = 0x0000000000000001ull;
    u64 neg_min_subnormal = 0x8000000000000001ull;
    u64 min_normal = 0x0010000000000000ull;
    u64 max_finite = 0x7fefffffffffffffull;
    u64 inf = 0x7ff0000000000000ull;
    u64 minus_inf = 0xfff0000000000000ull;
    int ok = 1;
    arithmetic_fail_mask = 0u;
    ok &= check_arith(0, check_u64(__aeabi_dadd(one, two), three));
    ok &= check_arith(1, check_u64(__aeabi_dadd(two_to_53, one), two_to_53));
    ok &= check_arith(2, check_u64(__aeabi_dadd(min_subnormal, min_subnormal), 0x0000000000000002ull));
    ok &= check_arith(3, check_u64(__aeabi_dadd(neg_zero, neg_zero), neg_zero));
    ok &= check_arith(4, check_u64(__aeabi_dadd(one, minus_one), 0ull));
    ok &= check_arith(5, check_nan(__aeabi_dadd(inf, minus_inf)));
    ok &= check_arith(6, check_u64(__aeabi_dadd(max_finite, max_finite), inf));
    ok &= check_arith(7, check_u64(__aeabi_dsub(one, two), minus_one));
    ok &= check_arith(8, check_u64(__aeabi_dsub(minus_one, one), minus_two));
    ok &= check_arith(9, check_u64(__aeabi_dmul(two, three), six));
    ok &= check_arith(10, check_u64(__aeabi_dmul(minus_two, three), minus_six));
    ok &= check_arith(11, check_u64(__aeabi_dmul(min_normal, half), 0x0008000000000000ull));
    ok &= check_arith(12, check_u64(__aeabi_dmul(max_finite, two), inf));
    ok &= check_arith(13, check_nan(__aeabi_dmul(0ull, inf)));
    ok &= check_arith(14, check_u64(__aeabi_ddiv(six, three), two));
    ok &= check_arith(15, check_u64(__aeabi_ddiv(one, three), one_third));
    ok &= check_arith(16, check_u64(__aeabi_ddiv(min_normal, two), 0x0008000000000000ull));
    ok &= check_arith(17, check_u64(__aeabi_ddiv(one, 0ull), inf));
    ok &= check_arith(18, check_u64(__aeabi_ddiv(minus_one, 0ull), minus_inf));
    ok &= check_arith(19, check_nan(__aeabi_ddiv(0ull, 0ull)));
    ok &= check_arith(20, check_u64(__aeabi_dmul(neg_min_subnormal, half), neg_zero));
    ok &= check_arith(21, check_u64(__aeabi_ddiv(neg_min_subnormal, two), neg_zero));
    return ok;
}

static int compiler_compare_tests(void) {
    volatile double a = 1.25;
    volatile double b = -2.5;
    volatile int i = -12345;
    volatile unsigned int u = 4000000000u;
    double di = (double)i;
    double du = (double)u;
    int ok = 1;
    ok &= (a > b);
    ok &= (b < a);
    ok &= (a >= 1.25);
    ok &= (b <= -2.5);
    ok &= (a != b);
    ok &= ((int)di == -12345);
    ok &= ((unsigned int)du == 4000000000u);
    return ok;
}

static int compiler_arithmetic_tests(void) {
    volatile double a = 1.5;
    volatile double b = 2.25;
    D64 out;
    int ok = 1;
    out.d = a + b;
    ok &= check_u64(out.u, 0x400e000000000000ull);
    out.d = b - a;
    ok &= check_u64(out.u, 0x3fe8000000000000ull);
    out.d = a * b;
    ok &= check_u64(out.u, 0x400b000000000000ull);
    out.d = b / a;
    ok &= check_u64(out.u, 0x3ff8000000000000ull);
    return ok;
}

void bare_main(void) {
    int conversions_ok;
    int comparisons_ok;
    int arithmetic_ok;
    int compiler_ok;
    int ok;
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    conversions_ok = conversion_tests();
    comparisons_ok = comparison_tests();
    arithmetic_ok = arithmetic_tests();
    compiler_ok = compiler_compare_tests() && compiler_arithmetic_tests();
    ok = conversions_ok && comparisons_ok && arithmetic_ok && compiler_ok;
    picocalc_lcd_fill_rect(20, 52, 299, 214, ok ? 0x102040u : 0x401000u);
    picocalc_lcd_puts_scale(44, 84, ok ? "AEABI DOUBLE OK" : "AEABI DOUBLE BAD", 0xffffffu, ok ? 0x102040u : 0x401000u, 1);
    picocalc_lcd_puts_scale(44, 112, conversions_ok ? "CONV OK" : "CONV BAD", conversions_ok ? 0x80ff80u : 0xff8080u, ok ? 0x102040u : 0x401000u, 1);
    picocalc_lcd_puts_scale(44, 140, comparisons_ok ? "CMP OK" : "CMP BAD", comparisons_ok ? 0x80ff80u : 0xff8080u, ok ? 0x102040u : 0x401000u, 1);
    picocalc_lcd_puts_scale(44, 168, arithmetic_ok ? "ARITH OK" : "ARITH BAD", arithmetic_ok ? 0x80ff80u : 0xff8080u, ok ? 0x102040u : 0x401000u, 1);
    picocalc_lcd_puts_scale(44, 196, compiler_ok ? "GCC OK" : "GCC BAD", compiler_ok ? 0x80ff80u : 0xff8080u, ok ? 0x102040u : 0x401000u, 1);
    for (u32 bit = 0; bit < 22u; bit += 1u) {
        int x = 44 + (int)bit * 9;
        picocalc_lcd_fill_rect(x, 224, x + 6, 234, (arithmetic_fail_mask & (1u << bit)) != 0u ? 0xff8080u : 0x204020u);
    }
    while (1) {
    }
}
