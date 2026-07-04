#include "emu_util.h"

u32 read_le32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

u16 read_le16(const u8 *p) {
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

void write_le32(u8 *p, u32 value) {
    p[0] = (u8)value;
    p[1] = (u8)(value >> 8);
    p[2] = (u8)(value >> 16);
    p[3] = (u8)(value >> 24);
}

u32 count_leading_zeroes(u32 value) {
    u32 count = 0;
    u32 bit = 0x80000000u;
    while (bit != 0u && (value & bit) == 0u) { count += 1u; bit >>= 1; }
    return count;
}

u32 count_trailing_zeroes(u32 value) {
    u32 count = 0;
    u32 bit = 1u;
    while (bit != 0u && (value & bit) == 0u) { count += 1u; bit <<= 1; }
    return count;
}

u32 count_bits(u32 value) {
    u32 count = 0;
    while (value != 0u) { count += value & 1u; value >>= 1; }
    return count;
}

u32 reverse_bits(u32 value) {
    u32 out_value = 0;
    int i;
    for (i = 0; i < 32; ++i) {
        out_value = (out_value << 1) | (value & 1u);
        value >>= 1;
    }
    return out_value;
}

float bits_to_float(u32 value) {
    union { u32 u; float f; } conv;
    conv.u = value;
    return conv.f;
}

u32 float_to_bits(float value) {
    union { float f; u32 u; } conv;
    conv.f = value;
    return conv.u;
}

u32 sx(u32 value, int bits) {
    u32 sign = 1u << (bits - 1);
    return (value ^ sign) - sign;
}