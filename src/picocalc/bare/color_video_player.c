#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#ifdef PICOCALC_COLOR_VIDEO_STREAM_HEADER
#include PICOCALC_COLOR_VIDEO_STREAM_HEADER
#else
#include "astley_color_stream.h"
#endif

#define VIDEO_W 320u
#define VIDEO_H 240u
#define TILE_W 16u
#define TILE_H 8u
#define TILES_X (VIDEO_W / TILE_W)
#define TILES_Y (VIDEO_H / TILE_H)
#define TILE_PIXELS (TILE_W * TILE_H)
#define TILE_MASK_BYTES (TILE_PIXELS / 8u)
#define VIDEO_Y0 40
#define FRAME_PAYLOAD_MAX 16384u
#define LZ_MIN_MATCH 3u

#define MODE_SOLID 1u
#define MODE_PAIR 2u

static uint8_t g_frame_payload[FRAME_PAYLOAD_MAX];
static uint8_t g_tile_rgb[TILE_PIXELS * 3u];
static volatile uint32_t g_sink;

static uint32_t timer_us(void) {
    return reg_time_us();
}

static uint16_t read_u16(const unsigned char *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static int get_mask_bit(const unsigned char *mask, uint32_t index) {
    return (mask[index >> 3] & (uint8_t)(0x80u >> (index & 7u))) != 0;
}

static void rgb332(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t rv = index >> 5;
    uint8_t gv = (index >> 2) & 7u;
    uint8_t bv = index & 3u;
    *r = (uint8_t)((rv << 5) | (rv << 2) | (rv >> 1));
    *g = (uint8_t)((gv << 5) | (gv << 2) | (gv >> 1));
    *b = (uint8_t)((bv << 6) | (bv << 4) | (bv << 2) | bv);
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

static void draw_solid_tile(uint16_t tile_index, uint8_t color) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint32_t i;
    rgb332(color, &r, &g, &b);
    for (i = 0; i < TILE_PIXELS; ++i) {
        uint32_t out = i * 3u;
        g_tile_rgb[out + 0u] = r;
        g_tile_rgb[out + 1u] = g;
        g_tile_rgb[out + 2u] = b;
    }
    picocalc_lcd_blit_rgb((int)(tx * TILE_W), VIDEO_Y0 + (int)(ty * TILE_H), TILE_W, TILE_H, g_tile_rgb);
}

static void draw_pair_tile(uint16_t tile_index, uint8_t color0, uint8_t color1, const unsigned char *mask) {
    uint32_t tx = tile_index % TILES_X;
    uint32_t ty = tile_index / TILES_X;
    uint8_t r0;
    uint8_t g0;
    uint8_t b0;
    uint8_t r1;
    uint8_t g1;
    uint8_t b1;
    uint32_t i;
    rgb332(color0, &r0, &g0, &b0);
    rgb332(color1, &r1, &g1, &b1);
    for (i = 0; i < TILE_PIXELS; ++i) {
        uint32_t out = i * 3u;
        if (get_mask_bit(mask, i)) {
            g_tile_rgb[out + 0u] = r1;
            g_tile_rgb[out + 1u] = g1;
            g_tile_rgb[out + 2u] = b1;
        } else {
            g_tile_rgb[out + 0u] = r0;
            g_tile_rgb[out + 1u] = g0;
            g_tile_rgb[out + 2u] = b0;
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
    const unsigned char *data = picocalc_color_video_stream;
    const unsigned char *end = picocalc_color_video_stream + picocalc_color_video_stream_size;
    uint16_t width;
    uint16_t height;
    uint16_t frames;
    uint16_t fps10;
    uint32_t frame_us;
    uint32_t start;
    uint32_t tiles = 0;
    uint32_t payload = 0;
    uint16_t frame;
    if (end - data < 14 || data[0] != 'P' || data[1] != 'C' || data[2] != 'V' || data[3] != 'C') return 0;
    width = read_u16(data + 4);
    height = read_u16(data + 6);
    frames = read_u16(data + 10);
    fps10 = read_u16(data + 12);
    if (width != VIDEO_W || height != VIDEO_H || data[8] != TILE_W || data[9] != TILE_H || fps10 == 0u) return 0;
    data += 14;
    frame_us = 10000000u / fps10;
    start = timer_us();
    for (frame = 0; frame < frames; ++frame) {
        const unsigned char *cursor;
        const unsigned char *frame_end;
        uint16_t stored_size;
        uint16_t next_tile = 0;
        if (end - data < 2) return 0;
        stored_size = read_u16(data);
        data += 2;
        if ((stored_size & 0x8000u) != 0u) {
            uint16_t compressed_size = stored_size & 0x7fffu;
            uint16_t raw_size;
            if (end - data < 2) return 0;
            raw_size = read_u16(data);
            data += 2;
            if (raw_size > FRAME_PAYLOAD_MAX || end - data < compressed_size) return 0;
            if (!lzss_decompress(data, compressed_size, g_frame_payload, raw_size)) return 0;
            data += compressed_size;
            cursor = g_frame_payload;
            frame_end = g_frame_payload + raw_size;
            payload += compressed_size + 4u;
        } else {
            if (stored_size > FRAME_PAYLOAD_MAX || end - data < stored_size) return 0;
            cursor = data;
            frame_end = data + stored_size;
            data = frame_end;
            payload += stored_size + 2u;
        }
        while (cursor < frame_end) {
            uint8_t control;
            uint8_t mode;
            uint8_t delta;
            uint16_t tile_index;
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
            if (tile_index >= TILES_X * TILES_Y) return 0;
            if (mode == MODE_SOLID) {
                if (frame_end - cursor < 1) return 0;
                draw_solid_tile(tile_index, *cursor++);
            } else if (mode == MODE_PAIR) {
                uint8_t color0;
                uint8_t color1;
                if ((uint32_t)(frame_end - cursor) < 2u + TILE_MASK_BYTES) return 0;
                color0 = *cursor++;
                color1 = *cursor++;
                draw_pair_tile(tile_index, color0, color1, cursor);
                cursor += TILE_MASK_BYTES;
            } else {
                return 0;
            }
            tiles += 1u;
        }
        wait_until_us(start + (uint32_t)(frame + 1u) * frame_us);
    }
    *frames_out = frames;
    *tiles_out = tiles;
    *payload_out = payload;
    *elapsed_out = timer_us() - start;
    return 1;
}

void bare_main(void) {
    uint32_t frames = 0;
    uint32_t tiles = 0;
    uint32_t payload = 0;
    uint32_t elapsed = 0;
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    draw_text_line(0, "Astley color video", 0xffffffu);
    if (play_stream(&frames, &tiles, &payload, &elapsed)) {
        uint32_t fps10 = elapsed == 0u ? 0u : frames * 10000000u / elapsed;
        draw_text_line(0, "Astley done", 0x00ff00u);
        draw_u32_line(1, "frames ", frames, "", 0xffffffu);
        draw_u32_line(2, "tiles ", tiles, "", 0xffffffu);
        draw_u32_line(3, "payload ", payload / 1024u, " KiB", 0xffffffu);
        draw_u32_line(4, "fps ", fps10 / 10u, "", 0xffffffu);
    } else {
        draw_text_line(1, "bad PCVC stream", 0xff4040u);
    }
    while (1) {
        g_sink += timer_us();
    }
}
