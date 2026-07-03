#include "gif_writer.h"

typedef unsigned char u8;
typedef unsigned int u32;

static int gif_write_all(GifWriter *writer, const void *data, gif_usize count) {
    if (writer->failed) return -1;
    if (writer->write_fn(writer->ctx, data, count) != 0) {
        writer->failed = 1;
        return -1;
    }
    return 0;
}

static int gif_write_byte(GifWriter *writer, u8 byte) {
    return gif_write_all(writer, &byte, 1u);
}

static int gif_write_le16(GifWriter *writer, unsigned int value) {
    u8 data[2];
    data[0] = (u8)value;
    data[1] = (u8)(value >> 8u);
    return gif_write_all(writer, data, sizeof(data));
}

static int gif_flush_block(GifWriter *writer) {
    if (writer->block_count == 0u) return 0;
    if (gif_write_byte(writer, (u8)writer->block_count) != 0) return -1;
    if (gif_write_all(writer, writer->block, writer->block_count) != 0) return -1;
    writer->block_count = 0;
    return 0;
}

static int gif_put_lzw_byte(GifWriter *writer, u8 byte) {
    writer->block[writer->block_count++] = byte;
    if (writer->block_count == sizeof(writer->block)) return gif_flush_block(writer);
    return 0;
}

static int gif_write_lzw_code9(GifWriter *writer, unsigned int code) {
    writer->bit_buffer |= code << writer->bit_count;
    writer->bit_count += 9u;
    while (writer->bit_count >= 8u) {
        if (gif_put_lzw_byte(writer, (u8)(writer->bit_buffer & 0xffu)) != 0) return -1;
        writer->bit_buffer >>= 8u;
        writer->bit_count -= 8u;
    }
    return 0;
}

static int gif_flush_lzw_bits(GifWriter *writer) {
    while (writer->bit_count != 0u) {
        if (gif_put_lzw_byte(writer, (u8)(writer->bit_buffer & 0xffu)) != 0) return -1;
        if (writer->bit_count <= 8u) {
            writer->bit_buffer = 0;
            writer->bit_count = 0;
        } else {
            writer->bit_buffer >>= 8u;
            writer->bit_count -= 8u;
        }
    }
    return gif_flush_block(writer);
}

static u8 gif_rgb332(const u8 *rgb) {
    return (u8)((rgb[0] & 0xe0u) | ((rgb[1] >> 3u) & 0x1cu) | (rgb[2] >> 6u));
}

static int gif_write_palette(GifWriter *writer) {
    u8 entry[3];
    unsigned int index;
    for (index = 0; index < 256u; ++index) {
        unsigned int r = (index >> 5u) & 7u;
        unsigned int g = (index >> 2u) & 7u;
        unsigned int b = index & 3u;
        entry[0] = (u8)((r * 255u + 3u) / 7u);
        entry[1] = (u8)((g * 255u + 3u) / 7u);
        entry[2] = (u8)((b * 255u + 1u) / 3u);
        if (gif_write_all(writer, entry, sizeof(entry)) != 0) return -1;
    }
    return 0;
}

int gif_begin_rgb8(GifWriter *writer, GifWriteFn write_fn, void *ctx, unsigned int width, unsigned int height, unsigned int fps) {
    static const u8 header[] = { 'G', 'I', 'F', '8', '9', 'a' };
    static const u8 loop_ext[] = { 0x21u, 0xffu, 0x0bu, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03u, 0x01u, 0x00u, 0x00u, 0x00u };
    writer->write_fn = write_fn;
    writer->ctx = ctx;
    writer->width = width;
    writer->height = height;
    writer->delay_cs = fps == 0u ? 7u : (100u + fps / 2u) / fps;
    writer->block_count = 0;
    writer->bit_buffer = 0;
    writer->bit_count = 0;
    writer->failed = 0;
    if (writer->delay_cs == 0u) writer->delay_cs = 1u;

    if (gif_write_all(writer, header, sizeof(header)) != 0) return -1;
    if (gif_write_le16(writer, width) != 0) return -1;
    if (gif_write_le16(writer, height) != 0) return -1;
    if (gif_write_byte(writer, 0xf7u) != 0) return -1;
    if (gif_write_byte(writer, 0u) != 0) return -1;
    if (gif_write_byte(writer, 0u) != 0) return -1;
    if (gif_write_palette(writer) != 0) return -1;
    return gif_write_all(writer, loop_ext, sizeof(loop_ext));
}

int gif_write_frame_rgb8(GifWriter *writer, const unsigned char *rgb) {
    static const u8 gce_head[] = { 0x21u, 0xf9u, 0x04u, 0x00u };
    static const u8 image_sep = 0x2cu;
    gif_usize pixel_count = (gif_usize)writer->width * (gif_usize)writer->height;
    gif_usize pixel;
    unsigned int emitted = 0;

    if (gif_write_all(writer, gce_head, sizeof(gce_head)) != 0) return -1;
    if (gif_write_le16(writer, writer->delay_cs) != 0) return -1;
    if (gif_write_byte(writer, 0u) != 0) return -1;
    if (gif_write_byte(writer, 0u) != 0) return -1;

    if (gif_write_all(writer, &image_sep, 1u) != 0) return -1;
    if (gif_write_le16(writer, 0u) != 0) return -1;
    if (gif_write_le16(writer, 0u) != 0) return -1;
    if (gif_write_le16(writer, writer->width) != 0) return -1;
    if (gif_write_le16(writer, writer->height) != 0) return -1;
    if (gif_write_byte(writer, 0u) != 0) return -1;
    if (gif_write_byte(writer, 8u) != 0) return -1;

    writer->block_count = 0;
    writer->bit_buffer = 0;
    writer->bit_count = 0;
    if (gif_write_lzw_code9(writer, 256u) != 0) return -1;
    for (pixel = 0; pixel < pixel_count; ++pixel) {
        if (emitted >= 250u) {
            if (gif_write_lzw_code9(writer, 256u) != 0) return -1;
            emitted = 0;
        }
        if (gif_write_lzw_code9(writer, gif_rgb332(rgb + pixel * 3u)) != 0) return -1;
        emitted += 1u;
    }
    if (gif_write_lzw_code9(writer, 257u) != 0) return -1;
    if (gif_flush_lzw_bits(writer) != 0) return -1;
    return gif_write_byte(writer, 0u);
}

int gif_end(GifWriter *writer) {
    if (gif_write_byte(writer, 0x3bu) != 0) return -1;
    return writer->failed ? -1 : 0;
}