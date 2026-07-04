typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;

typedef union {
    double d;
    u64 u;
    struct {
        u32 lo;
        u32 hi;
    } w;
} D64;

#define D_SIGN 0x80000000u
#define D_EXP 0x7ff00000u
#define D_FRAC_HI 0x000fffffu
#define D_EXP_BIAS 1023

#define D_QNAN 0x7ff8000000000000ull
#define D_INF 0x7ff0000000000000ull
#define D_NEG_ZERO 0x8000000000000000ull
#define D_MANT_TOP (1ull << 52)

typedef struct {
    u64 hi;
    u64 lo;
} U128;

typedef struct {
    int sign;
    int exp;
    u64 mant;
    int is_zero;
    int is_inf;
    int is_nan;
} DParts;

static u64 pack_double_bits(int sign, int exponent, u64 frac);

static int d_is_nan(D64 value) {
    return (value.w.hi & D_EXP) == D_EXP && ((value.w.hi & D_FRAC_HI) != 0u || value.w.lo != 0u);
}

static int d_is_zero(D64 value) {
    return (value.w.hi & 0x7fffffffu) == 0u && value.w.lo == 0u;
}

static int d_abs_cmp(D64 left, D64 right) {
    u32 left_hi = left.w.hi & 0x7fffffffu;
    u32 right_hi = right.w.hi & 0x7fffffffu;
    if (left_hi < right_hi) return -1;
    if (left_hi > right_hi) return 1;
    if (left.w.lo < right.w.lo) return -1;
    if (left.w.lo > right.w.lo) return 1;
    return 0;
}

static int d_ordered_cmp(D64 left, D64 right, int unordered_value) {
    int left_neg;
    int right_neg;
    int cmp;
    if (d_is_nan(left) || d_is_nan(right)) return unordered_value;
    if (d_is_zero(left) && d_is_zero(right)) return 0;
    left_neg = (left.w.hi & D_SIGN) != 0u;
    right_neg = (right.w.hi & D_SIGN) != 0u;
    if (left_neg != right_neg) return left_neg ? -1 : 1;
    cmp = d_abs_cmp(left, right);
    return left_neg ? -cmp : cmp;
}

static int u64_high_bit(u64 value) {
    int bit = 63;
    while (bit > 0 && ((value >> bit) & 1ull) == 0ull) bit -= 1;
    return bit;
}

static u64 d_quiet_nan(void) {
    return D_QNAN;
}

static u64 d_inf_bits(int sign) {
    return sign ? (D_NEG_ZERO | D_INF) : D_INF;
}

static u64 d_zero_bits(int sign) {
    return sign ? D_NEG_ZERO : 0ull;
}

static u64 shift_right_sticky_u64(u64 value, int shift) {
    u64 lost_mask;
    if (shift <= 0) return value;
    if (shift >= 64) return value != 0ull ? 1ull : 0ull;
    lost_mask = (1ull << shift) - 1ull;
    return (value >> shift) | ((value & lost_mask) != 0ull ? 1ull : 0ull);
}

static u64 shift_right_sticky_u128(U128 value, int shift) {
    u64 out;
    u64 sticky = 0ull;
    if (shift <= 0) {
        if (shift < 0 && -shift < 64) return value.lo << -shift;
        return value.lo;
    }
    if (shift < 64) {
        if ((value.lo & ((1ull << shift) - 1ull)) != 0ull) sticky = 1ull;
        out = (value.lo >> shift) | (value.hi << (64 - shift));
        return out | sticky;
    }
    if (shift == 64) return value.hi | (value.lo != 0ull ? 1ull : 0ull);
    if (shift < 128) {
        int high_shift = shift - 64;
        if (value.lo != 0ull) sticky = 1ull;
        if ((value.hi & ((1ull << high_shift) - 1ull)) != 0ull) sticky = 1ull;
        return (value.hi >> high_shift) | sticky;
    }
    return (value.hi != 0ull || value.lo != 0ull) ? 1ull : 0ull;
}

static DParts d_unpack(u64 bits) {
    D64 value;
    DParts out;
    u32 raw_exp;
    u64 frac;
    value.u = bits;
    raw_exp = (value.w.hi & D_EXP) >> 20;
    frac = (((u64)(value.w.hi & D_FRAC_HI)) << 32) | value.w.lo;
    out.sign = (value.w.hi & D_SIGN) != 0u;
    out.exp = 0;
    out.mant = 0ull;
    out.is_zero = 0;
    out.is_inf = 0;
    out.is_nan = 0;
    if (raw_exp == 0x7ffu) {
        if (frac != 0ull) out.is_nan = 1;
        else out.is_inf = 1;
        return out;
    }
    if (raw_exp == 0u) {
        if (frac == 0ull) {
            out.is_zero = 1;
            return out;
        }
        out.exp = -1022;
        out.mant = frac;
        while ((out.mant & D_MANT_TOP) == 0ull) {
            out.mant <<= 1;
            out.exp -= 1;
        }
        return out;
    }
    out.exp = (int)raw_exp - D_EXP_BIAS;
    out.mant = D_MANT_TOP | frac;
    return out;
}

static u64 pack_rounded(int sign, int exp, u64 mant_ext) {
    u64 mant;
    u64 round_bits;
    if (mant_ext == 0ull) return d_zero_bits(sign);
    while (mant_ext >= (1ull << 56)) {
        mant_ext = shift_right_sticky_u64(mant_ext, 1);
        exp += 1;
    }
    while (mant_ext < (1ull << 55) && exp > -1075) {
        mant_ext <<= 1;
        exp -= 1;
    }
    if (exp < -1022) {
        mant_ext = shift_right_sticky_u64(mant_ext, -1022 - exp);
        exp = -1022;
    }
    mant = mant_ext >> 3;
    round_bits = mant_ext & 7ull;
    if (round_bits > 4ull || (round_bits == 4ull && (mant & 1ull) != 0ull)) mant += 1ull;
    if (mant == 0ull) return d_zero_bits(sign);
    if (mant >= (1ull << 53)) {
        mant >>= 1;
        exp += 1;
    }
    if (exp > 1023) return d_inf_bits(sign);
    if (exp == -1022 && mant < D_MANT_TOP) return pack_double_bits(sign, 0, mant);
    return pack_double_bits(sign, exp + D_EXP_BIAS, mant & 0x000fffffffffffffull);
}

static U128 mul_u64_53(u64 left, u64 right) {
    u64 left_lo = (u32)left;
    u64 left_hi = left >> 32;
    u64 right_lo = (u32)right;
    u64 right_hi = right >> 32;
    u64 low = left_lo * right_lo;
    u64 cross = left_lo * right_hi + left_hi * right_lo;
    u64 high = left_hi * right_hi;
    u64 shifted = cross << 32;
    U128 out;
    out.lo = low + shifted;
    out.hi = high + (cross >> 32) + (out.lo < low ? 1ull : 0ull);
    return out;
}

static int u128_high_bit(U128 value) {
    if (value.hi != 0ull) return 64 + u64_high_bit(value.hi);
    return u64_high_bit(value.lo);
}

static u64 div_shifted_u64(u64 numerator, u64 denominator, int shift, u64 *remainder_out) {
    int top = u64_high_bit(numerator) + shift;
    int bit;
    u64 quotient = 0ull;
    u64 remainder = 0ull;
    for (bit = top; bit >= 0; bit -= 1) {
        remainder <<= 1;
        if (bit >= shift && ((numerator >> (bit - shift)) & 1ull) != 0ull) remainder |= 1ull;
        if (remainder >= denominator) {
            remainder -= denominator;
            if (bit < 64) quotient |= 1ull << bit;
        }
    }
    *remainder_out = remainder;
    return quotient;
}

static u64 d_add_parts(DParts left, DParts right) {
    u64 left_mant;
    u64 right_mant;
    u64 result_mant;
    int exp;
    if (left.is_nan || right.is_nan) return d_quiet_nan();
    if (left.is_inf || right.is_inf) {
        if (left.is_inf && right.is_inf && left.sign != right.sign) return d_quiet_nan();
        return d_inf_bits(left.is_inf ? left.sign : right.sign);
    }
    if (left.is_zero && right.is_zero) return d_zero_bits(left.sign & right.sign);
    if (left.is_zero) return pack_rounded(right.sign, right.exp, right.mant << 3);
    if (right.is_zero) return pack_rounded(left.sign, left.exp, left.mant << 3);
    exp = left.exp;
    left_mant = left.mant << 3;
    right_mant = right.mant << 3;
    if (left.exp > right.exp) right_mant = shift_right_sticky_u64(right_mant, left.exp - right.exp);
    else if (right.exp > left.exp) {
        left_mant = shift_right_sticky_u64(left_mant, right.exp - left.exp);
        exp = right.exp;
    }
    if (left.sign == right.sign) {
        result_mant = left_mant + right_mant;
        return pack_rounded(left.sign, exp, result_mant);
    }
    if (left_mant > right_mant) return pack_rounded(left.sign, exp, left_mant - right_mant);
    if (right_mant > left_mant) return pack_rounded(right.sign, exp, right_mant - left_mant);
    return d_zero_bits(0);
}

u64 __aeabi_dadd(u64 left_bits, u64 right_bits) {
    return d_add_parts(d_unpack(left_bits), d_unpack(right_bits));
}

u64 __aeabi_dsub(u64 left_bits, u64 right_bits) {
    DParts right = d_unpack(right_bits);
    right.sign = !right.sign;
    return d_add_parts(d_unpack(left_bits), right);
}

u64 __aeabi_dmul(u64 left_bits, u64 right_bits) {
    DParts left = d_unpack(left_bits);
    DParts right = d_unpack(right_bits);
    int sign = left.sign ^ right.sign;
    int bit;
    int exp;
    U128 product;
    u64 mant_ext;
    if (left.is_nan || right.is_nan) return d_quiet_nan();
    if ((left.is_inf && right.is_zero) || (right.is_inf && left.is_zero)) return d_quiet_nan();
    if (left.is_inf || right.is_inf) return d_inf_bits(sign);
    if (left.is_zero || right.is_zero) return d_zero_bits(sign);
    product = mul_u64_53(left.mant, right.mant);
    bit = u128_high_bit(product);
    exp = left.exp + right.exp + bit - 104;
    mant_ext = shift_right_sticky_u128(product, bit - 55);
    return pack_rounded(sign, exp, mant_ext);
}

u64 __aeabi_ddiv(u64 left_bits, u64 right_bits) {
    DParts left = d_unpack(left_bits);
    DParts right = d_unpack(right_bits);
    int sign = left.sign ^ right.sign;
    int exp;
    int shift;
    u64 mant_ext;
    u64 remainder;
    if (left.is_nan || right.is_nan) return d_quiet_nan();
    if (left.is_inf && right.is_inf) return d_quiet_nan();
    if (left.is_zero && right.is_zero) return d_quiet_nan();
    if (left.is_inf) return d_inf_bits(sign);
    if (right.is_inf) return d_zero_bits(sign);
    if (right.is_zero) return d_inf_bits(sign);
    if (left.is_zero) return d_zero_bits(sign);
    exp = left.exp - right.exp;
    shift = 55;
    if (left.mant < right.mant) {
        shift = 56;
        exp -= 1;
    }
    mant_ext = div_shifted_u64(left.mant, right.mant, shift, &remainder);
    if (remainder != 0ull) mant_ext |= 1ull;
    while (mant_ext < (1ull << 55)) {
        mant_ext <<= 1;
        exp -= 1;
    }
    return pack_rounded(sign, exp, mant_ext);
}

static u64 pack_double_bits(int sign, int exponent, u64 frac) {
    D64 out;
    out.w.lo = (u32)frac;
    out.w.hi = (sign ? D_SIGN : 0u) | ((u32)exponent << 20) | (u32)(frac >> 32);
    return out.u;
}

static u64 u64_to_double_bits(u64 value, int sign) {
    int bit;
    int exponent;
    u64 top;
    u64 frac;
    if (value == 0ull) return sign ? 0x8000000000000000ull : 0ull;
    bit = u64_high_bit(value);
    exponent = bit + D_EXP_BIAS;
    if (bit <= 52) {
        top = value << (52 - bit);
    } else {
        int shift = bit - 52;
        u64 remainder = value & ((1ull << shift) - 1ull);
        u64 half = 1ull << (shift - 1);
        top = value >> shift;
        if (remainder > half || (remainder == half && (top & 1ull) != 0ull)) {
            top += 1ull;
            if (top == (1ull << 53)) {
                top >>= 1;
                exponent += 1;
            }
        }
    }
    frac = top & 0x000fffffffffffffull;
    return pack_double_bits(sign, exponent, frac);
}

static u64 double_bits_to_u64(D64 value) {
    int exponent;
    u64 mantissa;
    if (d_is_nan(value) || (value.w.hi & D_SIGN) != 0u) return 0ull;
    if ((value.w.hi & D_EXP) == D_EXP) return 0xffffffffffffffffull;
    exponent = (int)((value.w.hi & D_EXP) >> 20) - D_EXP_BIAS;
    if (exponent < 0) return 0ull;
    if (exponent > 63) return 0xffffffffffffffffull;
    if ((value.w.hi & D_EXP) == 0u) return 0ull;
    mantissa = (1ull << 52) | (((u64)(value.w.hi & D_FRAC_HI)) << 32) | value.w.lo;
    if (exponent >= 52) return mantissa << (exponent - 52);
    return mantissa >> (52 - exponent);
}

static s64 double_bits_to_s64(D64 value) {
    int negative = (value.w.hi & D_SIGN) != 0u;
    D64 mag = value;
    u64 unsigned_value;
    if (d_is_nan(value)) return 0;
    mag.w.hi &= ~D_SIGN;
    unsigned_value = double_bits_to_u64(mag);
    if (negative) {
        if (unsigned_value >= 0x8000000000000000ull) return (s64)0x8000000000000000ull;
        return -(s64)unsigned_value;
    }
    if (unsigned_value >= 0x8000000000000000ull) return (s64)0x7fffffffffffffffull;
    return (s64)unsigned_value;
}

u64 __aeabi_i2d(int value) {
    int sign = value < 0;
    u32 magnitude = sign ? (u32)(0u - (u32)value) : (u32)value;
    return u64_to_double_bits((u64)magnitude, sign);
}

u64 __aeabi_ui2d(u32 value) {
    return u64_to_double_bits((u64)value, 0);
}

u64 __aeabi_l2d(s64 value) {
    int sign = value < 0;
    u64 magnitude = sign ? 0ull - (u64)value : (u64)value;
    return u64_to_double_bits(magnitude, sign);
}

u64 __aeabi_ul2d(u64 value) {
    return u64_to_double_bits(value, 0);
}

int __aeabi_d2iz(u64 bits) {
    D64 value;
    s64 out;
    value.u = bits;
    out = double_bits_to_s64(value);
    if (out > 2147483647ll) return 2147483647;
    if (out < -2147483647ll - 1ll) return -2147483647 - 1;
    return (int)out;
}

u32 __aeabi_d2uiz(u64 bits) {
    D64 value;
    u64 out;
    value.u = bits;
    out = double_bits_to_u64(value);
    if (out > 0xffffffffull) return 0xffffffffu;
    return (u32)out;
}

s64 __aeabi_d2lz(u64 bits) {
    D64 value;
    value.u = bits;
    return double_bits_to_s64(value);
}

u64 __aeabi_d2ulz(u64 bits) {
    D64 value;
    value.u = bits;
    return double_bits_to_u64(value);
}

int __aeabi_dcmpeq(u64 left_bits, u64 right_bits) {
    D64 left;
    D64 right;
    left.u = left_bits;
    right.u = right_bits;
    return d_ordered_cmp(left, right, 1) == 0;
}

int __aeabi_dcmplt(u64 left_bits, u64 right_bits) {
    D64 left;
    D64 right;
    left.u = left_bits;
    right.u = right_bits;
    return d_ordered_cmp(left, right, 1) < 0;
}

int __aeabi_dcmple(u64 left_bits, u64 right_bits) {
    D64 left;
    D64 right;
    left.u = left_bits;
    right.u = right_bits;
    return d_ordered_cmp(left, right, 1) <= 0;
}

int __aeabi_dcmpgt(u64 left_bits, u64 right_bits) {
    D64 left;
    D64 right;
    left.u = left_bits;
    right.u = right_bits;
    return d_ordered_cmp(left, right, -1) > 0;
}

int __aeabi_dcmpge(u64 left_bits, u64 right_bits) {
    D64 left;
    D64 right;
    left.u = left_bits;
    right.u = right_bits;
    return d_ordered_cmp(left, right, -1) >= 0;
}

__attribute__((used)) static int dcmp_le_raw(u64 left_bits, u64 right_bits) {
    D64 left;
    D64 right;
    left.u = left_bits;
    right.u = right_bits;
    return d_ordered_cmp(left, right, 1);
}

__attribute__((naked)) void __aeabi_cdcmpeq(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "push {r0, r1, r2, r3, r4, lr}\n"
        "bl dcmp_le_raw\n"
        "cmp r0, #0\n"
        "bmi 1f\n"
        "movs r1, #0\n"
        "cmn r0, r1\n"
        "1:\n"
        "pop {r0, r1, r2, r3, r4, pc}\n"
    );
}

__attribute__((alias("__aeabi_cdcmpeq"))) void __aeabi_cdcmple(void);

__attribute__((naked)) void __aeabi_cdrcmple(void) {
    __asm__ volatile(
        ".syntax unified\n"
        "mov r12, r0\n"
        "mov r0, r2\n"
        "mov r2, r12\n"
        "mov r12, r1\n"
        "mov r1, r3\n"
        "mov r3, r12\n"
        "b __aeabi_cdcmpeq\n"
    );
}
