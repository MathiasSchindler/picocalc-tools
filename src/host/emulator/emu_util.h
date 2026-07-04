#ifndef PICOCALC_EMU_UTIL_H
#define PICOCALC_EMU_UTIL_H

#ifndef PICOCALC_HOST_NOLIBC_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long usize;
#endif

u32 read_le32(const u8 *p);
u16 read_le16(const u8 *p);
void write_le32(u8 *p, u32 value);
u32 count_leading_zeroes(u32 value);
u32 count_trailing_zeroes(u32 value);
u32 count_bits(u32 value);
u32 reverse_bits(u32 value);
float bits_to_float(u32 value);
u32 float_to_bits(float value);
u32 sx(u32 value, int bits);

#endif