#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#ifdef PICOCALC_VIDEO_STREAM_HEADER
#include PICOCALC_VIDEO_STREAM_HEADER
#else
#include "video_stream_sample.h"
#endif

#define VIDEO_W 320u
#define VIDEO_H 240u
#define TILE_W 16u
#define TILE_H 8u
#define TILES_X (VIDEO_W / TILE_W)
#define TILES_Y (VIDEO_H / TILE_H)
#define TILE_BYTES ((TILE_W * TILE_H) / 8u)
#define FRAME_BYTES ((VIDEO_W * VIDEO_H) / 8u)
#define VIDEO_FRAME_MAX 16384u
#define VIDEO_Y0 40
#define LZ_MIN_MATCH 3u

enum {
    CMD_XOR_NIBS1 = 0,
    CMD_SOLID0 = 1,
    CMD_SOLID1 = 2,
    CMD_RAW1 = 3,
    CMD_RLE1 = 4,
    CMD_XOR_RAW1 = 5,
    CMD_XOR_RLE1 = 6,
    CMD_XOR_BITS1 = 7
};

static uint8_t g_frame_bits[FRAME_BYTES];
static uint8_t g_frame_payload[VIDEO_FRAME_MAX];
static uint8_t g_tile_rgb[TILE_W * TILE_H * 3u];
static volatile uint32_t g_sink;

static uint32_t timer_us(void) {
    return reg_time_us();
}

static uint32_t elapsed_us(uint32_t start, uint32_t end) {
    uint32_t elapsed = end - start;
    return elapsed == 0u ? 1u : elapsed;
}

static uint16_t read_u16(const unsigned char *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static void set_bit(uint8_t *bits, uint32_t index, int on) {
    uint8_t mask = (uint8_t)(0x80u >> (index & 7u));
    if (on) bits[index >> 3] |= mask;
    else bits[index >> 3] &= (uint8_t)~mask;
}

static int get_bit(const uint8_t *bits, uint32_t index) {
    return (bits[index >> 3] & (uint8_t)(0x80u >> (index & 7u))) != 0;
}

static int lzss_decompress(const unsigned char *input, uint32_t input_size, uint8_t *out, uint32_t out_size) {
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    while (in_pos < input_size && out_pos < out_size) {
        uint8_t flags = input[in_pos++];
        uint32_t bit;
        for (bit = 0; bit < 8u && out_pos < out_size; ++bit) {
            if ((flags & (uint8_t)(1u << bit)) == 0u) {
                if (in_pos >= input_size) return 0;
                out[out_pos++] = input[in_pos++];
            } else {
                uint32_t b0;
                uint32_t b1;
                uint32_t offset;
                uint32_t length;
                uint32_t i;
                if (in_pos + 2u > input_size) return 0;
                b0 = input[in_pos++];
                b1 = input[in_pos++];
                offset = ((b1 & 15u) << 8) | b0;
                offset += 1u;
                if ((b1 >> 4) == 15u) {
                    if (in_pos >= input_size) return 0;
                    length = LZ_MIN_MATCH + 15u + input[in_pos++];
                } else {
                    length = (b1 >> 4) + LZ_MIN_MATCH;
                }
                if (offset > out_pos) return 0;
                for (i = 0; i < length && out_pos < out_size; ++i) {
                    out[out_pos] = out[out_pos - offset];
                    out_pos += 1u;
                }
            }
        }
    }
    return in_pos == input_size && out_pos == out_size;
}

static void set_tile_pixel(uint16_t tile_index, uint32_t pixel, int on) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint32_t x = tx * TILE_W + pixel % TILE_W;
    uint32_t y = ty * TILE_H + pixel / TILE_W;
    set_bit(g_frame_bits, y * VIDEO_W + x, on);
}

static void set_tile_solid(uint16_t tile_index, int on) {
    uint32_t i;
    for (i = 0; i < TILE_W * TILE_H; ++i) set_tile_pixel(tile_index, i, on);
}

static void set_tile_raw(uint16_t tile_index, const unsigned char *tile) {
    uint32_t i;
    for (i = 0; i < TILE_W * TILE_H; ++i) set_tile_pixel(tile_index, i, get_bit(tile, i));
}

static void xor_tile_raw(uint16_t tile_index, const unsigned char *tile) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint32_t i;
    for (i = 0; i < TILE_W * TILE_H; ++i) {
        if (get_bit(tile, i)) {
            uint32_t x = tx * TILE_W + i % TILE_W;
            uint32_t y = ty * TILE_H + i / TILE_W;
            uint32_t pixel = y * VIDEO_W + x;
            set_bit(g_frame_bits, pixel, !get_bit(g_frame_bits, pixel));
        }
    }
}

static const unsigned char *xor_tile_bits(uint16_t tile_index, const unsigned char *data, const unsigned char *end) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint32_t i;
    uint8_t length;
    if (data >= end) return end;
    length = *data++;
    if ((uint32_t)(end - data) < length) return end;
    for (i = 0; i < length; ++i) {
        uint32_t bit = *data++;
        uint32_t x = tx * TILE_W + bit % TILE_W;
        uint32_t y = ty * TILE_H + bit / TILE_W;
        uint32_t pixel = y * VIDEO_W + x;
        if (bit >= TILE_W * TILE_H) return end;
        set_bit(g_frame_bits, pixel, !get_bit(g_frame_bits, pixel));
    }
    return data;
}

static const unsigned char *xor_tile_nibbles(uint16_t tile_index, const unsigned char *data, const unsigned char *end) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint32_t i;
    uint32_t bit = 0;
    uint8_t length;
    if (data >= end) return end;
    length = *data++;
    if ((uint32_t)(end - data) < ((uint32_t)length + 1u) / 2u) return end;
    for (i = 0; i < length; ++i) {
        uint8_t packed = data[i / 2u];
        uint32_t gap = (i & 1u) == 0u ? packed >> 4 : packed & 15u;
        bit += gap;
        if (bit >= TILE_W * TILE_H) return end;
        {
            uint32_t x = tx * TILE_W + bit % TILE_W;
            uint32_t y = ty * TILE_H + bit / TILE_W;
            uint32_t pixel = y * VIDEO_W + x;
            set_bit(g_frame_bits, pixel, !get_bit(g_frame_bits, pixel));
        }
        bit += 1u;
    }
    return data + ((uint32_t)length + 1u) / 2u;
}

static const unsigned char *set_tile_rle(uint16_t tile_index, const unsigned char *data, const unsigned char *end) {
    uint32_t pixel = 0;
    uint32_t i;
    uint8_t length;
    if (data >= end) return end;
    length = *data++;
    for (i = 0; i < length && data < end; ++i) {
        uint8_t token = *data++;
        uint32_t run = token & 0x7fu;
        int on = (token & 0x80u) != 0u;
        while (run-- != 0u && pixel < TILE_W * TILE_H) set_tile_pixel(tile_index, pixel++, on);
    }
    return data;
}

static const unsigned char *xor_tile_rle(uint16_t tile_index, const unsigned char *data, const unsigned char *end) {
    uint8_t tile[TILE_BYTES];
    uint32_t pixel = 0;
    uint32_t i;
    uint8_t length;
    if (data >= end) return end;
    length = *data++;
    for (i = 0; i < TILE_BYTES; ++i) tile[i] = 0;
    for (i = 0; i < length && data < end; ++i) {
        uint8_t token = *data++;
        uint32_t run = token & 0x7fu;
        int on = (token & 0x80u) != 0u;
        while (run-- != 0u && pixel < TILE_W * TILE_H) set_bit(tile, pixel++, on);
    }
    xor_tile_raw(tile_index, tile);
    return data;
}

static void draw_tile(uint16_t tile_index) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint32_t y;
    for (y = 0; y < TILE_H; ++y) {
        uint32_t x;
        for (x = 0; x < TILE_W; ++x) {
            uint32_t pixel = (ty * TILE_H + y) * VIDEO_W + tx * TILE_W + x;
            uint8_t value = get_bit(g_frame_bits, pixel) ? 255u : 0u;
            uint32_t out = (y * TILE_W + x) * 3u;
            g_tile_rgb[out + 0u] = value;
            g_tile_rgb[out + 1u] = value;
            g_tile_rgb[out + 2u] = value;
        }
    }
    picocalc_lcd_blit_rgb((int)(tx * TILE_W), VIDEO_Y0 + (int)(ty * TILE_H), TILE_W, TILE_H, g_tile_rgb);
}

static void append_char(char *buffer, int *position, int capacity, char value) {
    if (*position + 1 < capacity) buffer[(*position)++] = value;
}

static void append_text(char *buffer, int *position, int capacity, const char *text) {
    while (*text != 0) append_char(buffer, position, capacity, *text++);
}

static void append_u32(char *buffer, int *position, int capacity, uint32_t value) {
    char scratch[10];
    int count = 0;
    if (value == 0u) {
        append_char(buffer, position, capacity, '0');
        return;
    }
    while (value != 0u && count < (int)sizeof(scratch)) {
        scratch[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count > 0) append_char(buffer, position, capacity, scratch[--count]);
}

static void finish_line(char *buffer, int *position, int capacity) {
    buffer[*position < capacity ? *position : capacity - 1] = 0;
}

static void draw_text_line(int row, const char *text, uint32_t color) {
    picocalc_lcd_puts_scale(0, row * 12, text, color, 0x000000u, 1);
}

static void draw_u32_line(int row, const char *prefix, uint32_t value, const char *suffix, uint32_t color) {
    char line[64];
    int pos = 0;
    append_text(line, &pos, sizeof(line), prefix);
    append_u32(line, &pos, sizeof(line), value);
    append_text(line, &pos, sizeof(line), suffix);
    finish_line(line, &pos, sizeof(line));
    draw_text_line(row, line, color);
}

static void wait_until_us(uint32_t target) {
    while ((int32_t)(timer_us() - target) < 0) {
    }
}

static int play_stream(uint32_t *frames_out, uint32_t *tiles_out, uint32_t *payload_out, uint32_t *elapsed_out) {
    const unsigned char *data = picocalc_video_stream;
    const unsigned char *end = picocalc_video_stream + picocalc_video_stream_size;
    uint16_t width;
    uint16_t height;
    uint16_t frames;
    uint16_t fps10;
    uint16_t frame;
    uint8_t version;
    uint32_t start;
    uint32_t frame_us;
    uint32_t tiles = 0;
    uint32_t payload = 0;
    if (end - data < 14 || data[0] != 'P' || data[1] != 'C' || data[2] != 'V' || (data[3] != '1' && data[3] != '2' && data[3] != '3')) return 0;
    version = data[3];
    width = read_u16(data + 4);
    height = read_u16(data + 6);
    frames = read_u16(data + 10);
    fps10 = read_u16(data + 12);
    if (width != VIDEO_W || height != VIDEO_H || data[8] != TILE_W || data[9] != TILE_H) return 0;
    if (fps10 == 0) return 0;
    frame_us = 10000000u / fps10;
    data += 14;
    start = timer_us();
    for (frame = 0; frame < frames; ++frame) {
        const unsigned char *frame_end;
        const unsigned char *cursor;
        uint16_t frame_size;
        uint16_t next_tile = 0;
        if (end - data < 2) return 0;
        frame_size = read_u16(data);
        data += 2;
        if (version == '3' && (frame_size & 0x8000u) != 0u) {
            uint16_t compressed_size = frame_size & 0x7fffu;
            uint16_t raw_size;
            if (end - data < 2) return 0;
            raw_size = read_u16(data);
            data += 2;
            if (raw_size > VIDEO_FRAME_MAX || end - data < compressed_size) return 0;
            if (!lzss_decompress(data, compressed_size, g_frame_payload, raw_size)) return 0;
            data += compressed_size;
            cursor = g_frame_payload;
            frame_end = g_frame_payload + raw_size;
            payload += compressed_size + 4u;
        } else {
            if (end - data < frame_size) return 0;
            cursor = data;
            frame_end = data + frame_size;
            data = frame_end;
            payload += frame_size + 2u;
        }
        while (cursor < frame_end) {
            uint16_t tile_index;
            uint8_t mode;
            if (version == '2' || version == '3') {
                uint8_t control;
                uint8_t delta;
                if (frame_end - cursor < 1) return 0;
                control = *cursor++;
                mode = control >> 5;
                delta = control & 31u;
                if (delta == 31u) {
                    if (frame_end - cursor < 2) return 0;
                    tile_index = read_u16(cursor);
                    cursor += 2;
                } else {
                    tile_index = (uint16_t)(next_tile + delta);
                }
                next_tile = (uint16_t)(tile_index + 1u);
            } else {
                if (frame_end - cursor < 3) return 0;
                tile_index = read_u16(cursor);
                cursor += 2;
                mode = *cursor++;
            }
            if (tile_index >= TILES_X * TILES_Y) return 0;
            if (mode == CMD_SOLID0) set_tile_solid(tile_index, 0);
            else if (mode == CMD_SOLID1) set_tile_solid(tile_index, 1);
            else if (mode == CMD_RAW1) {
                if ((uint32_t)(frame_end - cursor) < TILE_BYTES) return 0;
                set_tile_raw(tile_index, cursor);
                cursor += TILE_BYTES;
            } else if (mode == CMD_RLE1) {
                cursor = set_tile_rle(tile_index, cursor, frame_end);
            } else if (mode == CMD_XOR_RAW1) {
                if ((uint32_t)(frame_end - cursor) < TILE_BYTES) return 0;
                xor_tile_raw(tile_index, cursor);
                cursor += TILE_BYTES;
            } else if (mode == CMD_XOR_RLE1) {
                cursor = xor_tile_rle(tile_index, cursor, frame_end);
            } else if (mode == CMD_XOR_BITS1) {
                cursor = xor_tile_bits(tile_index, cursor, frame_end);
            } else if (mode == CMD_XOR_NIBS1) {
                cursor = xor_tile_nibbles(tile_index, cursor, frame_end);
            } else return 0;
            draw_tile(tile_index);
            tiles += 1u;
        }
        wait_until_us(start + (uint32_t)(frame + 1u) * frame_us);
    }
    *elapsed_out = elapsed_us(start, timer_us());
    *frames_out = frames;
    *tiles_out = tiles;
    *payload_out = payload + 14u;
    return 1;
}

#ifdef PICOCALC_BARE_SIM
const char *picocalc_lcd_sim_ppm_path(void) {
    return "build/sim/sim_video_player.ppm";
}
#endif

void bare_main(void) {
    uint32_t frames = 0;
    uint32_t tiles = 0;
    uint32_t payload = 0;
    uint32_t elapsed = 1;
    uint32_t fps10;
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    draw_text_line(0, "PCV1 DIRTY TILE PLAYER", 0x00ff80u);
    draw_text_line(1, "320x240 1bpp changed tiles", 0x808080u);
    if (!play_stream(&frames, &tiles, &payload, &elapsed)) {
        draw_text_line(3, "stream decode failed", 0xff8080u);
        while (1) {
        }
    }
    fps10 = frames * 10000000u / elapsed;
    picocalc_lcd_fill_rect(0, 0, 319, 35, 0x000000u);
    draw_text_line(0, "PCV1 DIRTY TILE PLAYER", 0x00ff80u);
    draw_u32_line(1, "fps ", fps10 / 10u, "", 0xffffffu);
    draw_u32_line(2, "frames ", frames, "", 0x808080u);
    draw_u32_line(24, "tiles ", tiles, "", 0x808080u);
    draw_u32_line(25, "stream ", payload, "B", 0x808080u);
    g_sink ^= elapsed ^ tiles ^ payload;
    while (1) {
    }
}