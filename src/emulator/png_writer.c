#include "png_writer.h"

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct {
    u8 *data;
    png_usize capacity;
    png_usize byte_offset;
    u32 bit_buffer;
    u32 bit_count;
} PngBitWriter;

static u32 png_crc32_update(u32 crc, const u8 *data, png_usize length) {
    static const u32 table[16] = {
        0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
        0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
        0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
        0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu
    };
    png_usize index;
    for (index = 0; index < length; ++index) {
        crc ^= (u32)data[index];
        crc = (crc >> 4u) ^ table[crc & 0x0fu];
        crc = (crc >> 4u) ^ table[crc & 0x0fu];
    }
    return crc;
}

static u32 png_crc32(const u8 *type, const u8 *data, png_usize length) {
    u32 crc = 0xffffffffu;
    crc = png_crc32_update(crc, type, 4u);
    crc = png_crc32_update(crc, data, length);
    return crc ^ 0xffffffffu;
}

static u32 png_adler32(const u8 *data, png_usize length) {
    u32 s1 = 1u;
    u32 s2 = 0u;
    while (length != 0u) {
        png_usize chunk = length > 5552u ? 5552u : length;
        png_usize index;
        for (index = 0; index < chunk; ++index) {
            s1 += (u32)data[index];
            s2 += s1;
        }
        s1 %= 65521u;
        s2 %= 65521u;
        data += chunk;
        length -= chunk;
    }
    return (s2 << 16u) | s1;
}

static void png_store_be32(u8 *out, u32 value) {
    out[0] = (u8)(value >> 24u);
    out[1] = (u8)(value >> 16u);
    out[2] = (u8)(value >> 8u);
    out[3] = (u8)value;
}

static u32 png_reverse_bits(u32 value, u32 count) {
    u32 reversed = 0;
    u32 index;
    for (index = 0; index < count; ++index) {
        reversed = (reversed << 1u) | (value & 1u);
        value >>= 1u;
    }
    return reversed;
}

static void png_bit_writer_init(PngBitWriter *writer, u8 *data, png_usize capacity) {
    writer->data = data;
    writer->capacity = capacity;
    writer->byte_offset = 0;
    writer->bit_buffer = 0;
    writer->bit_count = 0;
}

static int png_write_bits(PngBitWriter *writer, u32 value, u32 count) {
    if (count > 16u) return -1;
    writer->bit_buffer |= value << writer->bit_count;
    writer->bit_count += count;
    while (writer->bit_count >= 8u) {
        if (writer->byte_offset >= writer->capacity) return -1;
        writer->data[writer->byte_offset++] = (u8)(writer->bit_buffer & 0xffu);
        writer->bit_buffer >>= 8u;
        writer->bit_count -= 8u;
    }
    return 0;
}

static int png_flush_bits(PngBitWriter *writer) {
    if (writer->bit_count != 0u) {
        if (writer->byte_offset >= writer->capacity) return -1;
        writer->data[writer->byte_offset++] = (u8)(writer->bit_buffer & 0xffu);
        writer->bit_buffer = 0;
        writer->bit_count = 0;
    }
    return 0;
}

static int png_write_fixed_symbol(PngBitWriter *writer, u32 symbol) {
    u32 code;
    u32 length;
    if (symbol <= 143u) {
        code = 0x30u + symbol;
        length = 8u;
    } else if (symbol <= 255u) {
        code = 0x190u + (symbol - 144u);
        length = 9u;
    } else if (symbol <= 279u) {
        code = symbol - 256u;
        length = 7u;
    } else if (symbol <= 287u) {
        code = 0xc0u + (symbol - 280u);
        length = 8u;
    } else {
        return -1;
    }
    return png_write_bits(writer, png_reverse_bits(code, length), length);
}

static int png_write_fixed_match(PngBitWriter *writer, u32 length, u32 distance) {
    static const u32 length_base[29] = {
        3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 13u, 15u, 17u, 19u, 23u, 27u, 31u,
        35u, 43u, 51u, 59u, 67u, 83u, 99u, 115u, 131u, 163u, 195u, 227u, 258u
    };
    static const u8 length_extra[29] = {
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u,
        3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u
    };
    static const u32 dist_base[30] = {
        1u, 2u, 3u, 4u, 5u, 7u, 9u, 13u, 17u, 25u, 33u, 49u, 65u, 97u, 129u, 193u,
        257u, 385u, 513u, 769u, 1025u, 1537u, 2049u, 3073u, 4097u, 6145u, 8193u, 12289u, 16385u, 24577u
    };
    static const u8 dist_extra[30] = {
        0u, 0u, 0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 6u, 6u,
        7u, 7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u
    };
    u32 length_index;
    u32 dist_index;
    if (length < 3u || length > 258u || distance == 0u || distance > 32768u) return -1;
    for (length_index = 0; length_index < 29u; ++length_index) {
        u32 max_length = length_base[length_index] + ((1u << length_extra[length_index]) - 1u);
        if (length <= max_length) break;
    }
    if (length_index >= 29u) return -1;
    for (dist_index = 0; dist_index < 30u; ++dist_index) {
        u32 max_distance = dist_base[dist_index] + ((1u << dist_extra[dist_index]) - 1u);
        if (distance <= max_distance) break;
    }
    if (dist_index >= 30u) return -1;
    if (png_write_fixed_symbol(writer, 257u + length_index) != 0) return -1;
    if (length_extra[length_index] != 0u && png_write_bits(writer, length - length_base[length_index], length_extra[length_index]) != 0) return -1;
    if (png_write_bits(writer, png_reverse_bits(dist_index, 5u), 5u) != 0) return -1;
    if (dist_extra[dist_index] != 0u && png_write_bits(writer, distance - dist_base[dist_index], dist_extra[dist_index]) != 0) return -1;
    return 0;
}

static png_usize png_zlib_fixed_rle_bound(png_usize input_size) {
    return PNG_ZLIB_FIXED_RLE_BOUND(input_size);
}

static int png_zlib_fixed_rle(const u8 *input, png_usize input_size, u8 *output, png_usize output_capacity, png_usize *output_size_out) {
    PngBitWriter writer;
    png_usize input_offset = 0;
    u32 adler;
    if (output_capacity < png_zlib_fixed_rle_bound(input_size) || output_capacity < 6u) return -1;
    output[0] = 0x78u;
    output[1] = 0x01u;
    png_bit_writer_init(&writer, output + 2u, output_capacity - 6u);
    if (png_write_bits(&writer, 1u, 1u) != 0 || png_write_bits(&writer, 1u, 2u) != 0) return -1;
    while (input_offset < input_size) {
        png_usize run = 1;
        while (input_offset + run < input_size && run < 259u && input[input_offset + run] == input[input_offset]) run += 1u;
        if (run >= 4u) {
            png_usize remaining;
            if (png_write_fixed_symbol(&writer, input[input_offset]) != 0) return -1;
            input_offset += 1u;
            remaining = run - 1u;
            while (remaining != 0u) {
                u32 length = remaining > 258u ? 258u : (u32)remaining;
                if (length < 3u) {
                    u32 index;
                    for (index = 0; index < length; ++index) {
                        if (png_write_fixed_symbol(&writer, input[input_offset + index]) != 0) return -1;
                    }
                    input_offset += length;
                    remaining = 0;
                } else {
                    if (png_write_fixed_match(&writer, length, 1u) != 0) return -1;
                    input_offset += length;
                    remaining -= length;
                }
            }
        } else {
            if (png_write_fixed_symbol(&writer, input[input_offset]) != 0) return -1;
            input_offset += 1u;
        }
    }
    if (png_write_fixed_symbol(&writer, 256u) != 0 || png_flush_bits(&writer) != 0) return -1;
    *output_size_out = 2u + writer.byte_offset;
    if (*output_size_out + 4u > output_capacity) return -1;
    adler = png_adler32(input, input_size);
    output[(*output_size_out)++] = (u8)(adler >> 24u);
    output[(*output_size_out)++] = (u8)(adler >> 16u);
    output[(*output_size_out)++] = (u8)(adler >> 8u);
    output[(*output_size_out)++] = (u8)adler;
    return 0;
}

static u32 png_abs_byte(u32 value) {
    return value < 128u ? value : 256u - value;
}

static u32 png_filter_score(const u8 *rgb, unsigned int width, unsigned int row, unsigned int filter) {
    png_usize stride = (png_usize)width * 3u;
    const u8 *src = rgb + (png_usize)row * stride;
    const u8 *prev = row == 0 ? 0 : src - stride;
    png_usize index;
    u32 score = 0;
    for (index = 0; index < stride; ++index) {
        u32 left = index >= 3u ? src[index - 3u] : 0u;
        u32 up = prev != 0 ? prev[index] : 0u;
        u32 value = src[index];
        u32 residual = value;
        if (filter == 1u) residual = (value - left) & 0xffu;
        else if (filter == 2u) residual = (value - up) & 0xffu;
        score += png_abs_byte(residual);
    }
    return score;
}

static void png_write_filtered_row(const u8 *rgb, unsigned int width, unsigned int row, unsigned int filter, u8 *dst) {
    png_usize stride = (png_usize)width * 3u;
    const u8 *src = rgb + (png_usize)row * stride;
    const u8 *prev = row == 0 ? 0 : src - stride;
    png_usize index;
    dst[0] = (u8)filter;
    for (index = 0; index < stride; ++index) {
        u32 left = index >= 3u ? src[index - 3u] : 0u;
        u32 up = prev != 0 ? prev[index] : 0u;
        u32 value = src[index];
        if (filter == 1u) value = (value - left) & 0xffu;
        else if (filter == 2u) value = (value - up) & 0xffu;
        dst[1u + index] = (u8)value;
    }
}

static int png_filter_rgb8(const u8 *rgb, unsigned int width, unsigned int height, u8 *filtered, png_usize filtered_size) {
    png_usize stride = (png_usize)width * 3u;
    png_usize row_size = stride + 1u;
    unsigned int row;
    if (filtered_size < row_size * (png_usize)height) return -1;
    for (row = 0; row < height; ++row) {
        u32 score_none = png_filter_score(rgb, width, row, 0u);
        u32 score_sub = png_filter_score(rgb, width, row, 1u);
        u32 score_up = png_filter_score(rgb, width, row, 2u);
        unsigned int filter = 0;
        u32 best = score_none;
        if (score_sub < best) { best = score_sub; filter = 1u; }
        if (score_up < best) filter = 2u;
        png_write_filtered_row(rgb, width, row, filter, filtered + (png_usize)row * row_size);
    }
    return 0;
}

static int png_write_all(PngWriteFn write_fn, void *ctx, const void *data, png_usize count) {
    return write_fn(ctx, data, count) == 0 ? 0 : -1;
}

static int png_write_chunk(PngWriteFn write_fn, void *ctx, const u8 type[4], const u8 *data, png_usize length) {
    u8 header[8];
    u8 crc_bytes[4];
    u32 crc;
    if (length > 0xffffffffu) return -1;
    png_store_be32(header, (u32)length);
    header[4] = type[0];
    header[5] = type[1];
    header[6] = type[2];
    header[7] = type[3];
    if (png_write_all(write_fn, ctx, header, sizeof(header)) != 0) return -1;
    if (length != 0u && png_write_all(write_fn, ctx, data, length) != 0) return -1;
    crc = png_crc32(type, data, length);
    png_store_be32(crc_bytes, crc);
    return png_write_all(write_fn, ctx, crc_bytes, sizeof(crc_bytes));
}

int png_write_rgb8(PngWriteFn write_fn, void *ctx, const unsigned char *rgb, unsigned int width, unsigned int height, unsigned char *work, png_usize work_capacity) {
    static const u8 signature[8] = { 0x89u, 'P', 'N', 'G', '\r', '\n', 0x1au, '\n' };
    static const u8 ihdr_type[4] = { 'I', 'H', 'D', 'R' };
    static const u8 idat_type[4] = { 'I', 'D', 'A', 'T' };
    static const u8 iend_type[4] = { 'I', 'E', 'N', 'D' };
    u8 ihdr[13];
    png_usize filtered_size;
    png_usize compressed_capacity;
    png_usize compressed_size;
    u8 *filtered;
    u8 *compressed;

    if (write_fn == 0 || rgb == 0 || width == 0u || height == 0u || work == 0) return -1;
    filtered_size = PNG_RGB8_FILTERED_SIZE(width, height);
    if (work_capacity < filtered_size) return -1;
    compressed_capacity = work_capacity - filtered_size;
    if (compressed_capacity < png_zlib_fixed_rle_bound(filtered_size)) return -1;
    filtered = work;
    compressed = work + filtered_size;
    if (png_filter_rgb8(rgb, width, height, filtered, filtered_size) != 0) return -1;
    if (png_zlib_fixed_rle(filtered, filtered_size, compressed, compressed_capacity, &compressed_size) != 0) return -1;

    png_store_be32(ihdr + 0u, width);
    png_store_be32(ihdr + 4u, height);
    ihdr[8] = 8u;
    ihdr[9] = 2u;
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = 0u;

    if (png_write_all(write_fn, ctx, signature, sizeof(signature)) != 0) return -1;
    if (png_write_chunk(write_fn, ctx, ihdr_type, ihdr, sizeof(ihdr)) != 0) return -1;
    if (png_write_chunk(write_fn, ctx, idat_type, compressed, compressed_size) != 0) return -1;
    if (png_write_chunk(write_fn, ctx, iend_type, 0, 0) != 0) return -1;
    return 0;
}
