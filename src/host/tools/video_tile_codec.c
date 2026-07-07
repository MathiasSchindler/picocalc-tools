#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PCV_W 320u
#define PCV_H 240u
#define PCV_TILE_W 16u
#define PCV_TILE_H 8u
#define PCV_TILES_X (PCV_W / PCV_TILE_W)
#define PCV_TILES_Y (PCV_H / PCV_TILE_H)
#define PCV_TILE_COUNT (PCV_TILES_X * PCV_TILES_Y)
#define PCV_FRAME_BYTES ((PCV_W * PCV_H) / 8u)
#define PCV_TILE_BYTES ((PCV_TILE_W * PCV_TILE_H) / 8u)
#define PCV_MAX_FRAME_ENCODED 16384u
#define PCV_LCD_FULL_US 119304u
#define PCV_LCD_FULL_PIXELS (PCV_W * PCV_H)
#define PCV_LZ_WINDOW 4096u
#define PCV_LZ_MIN_MATCH 3u
#define PCV_LZ_MAX_MATCH 273u

enum {
    PCV_CMD_XOR_NIBS1 = 0,
    PCV_CMD_SOLID0 = 1,
    PCV_CMD_SOLID1 = 2,
    PCV_CMD_RAW1 = 3,
    PCV_CMD_RLE1 = 4,
    PCV_CMD_XOR_RAW1 = 5,
    PCV_CMD_XOR_RLE1 = 6,
    PCV_CMD_XOR_BITS1 = 7
};

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Buffer;

typedef struct {
    unsigned int frames;
    uint64_t payload_bytes;
    uint64_t stream_bytes;
    uint64_t changed_tiles;
    uint64_t changed_pixels;
    uint32_t worst_frame_bytes;
    uint32_t worst_changed_pixels;
} Stats;

static void die(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) die("out of memory");
    return ptr;
}

static void buffer_reserve(Buffer *buffer, size_t add) {
    size_t need = buffer->size + add;
    if (need <= buffer->capacity) return;
    if (buffer->capacity == 0) buffer->capacity = 4096;
    while (buffer->capacity < need) buffer->capacity *= 2u;
    buffer->data = (uint8_t *)realloc(buffer->data, buffer->capacity);
    if (buffer->data == NULL) die("out of memory");
}

static void buffer_u8(Buffer *buffer, uint8_t value) {
    buffer_reserve(buffer, 1);
    buffer->data[buffer->size++] = value;
}

static void buffer_u16(Buffer *buffer, uint32_t value) {
    buffer_u8(buffer, (uint8_t)value);
    buffer_u8(buffer, (uint8_t)(value >> 8));
}

static void buffer_bytes(Buffer *buffer, const uint8_t *data, size_t size) {
    buffer_reserve(buffer, size);
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
}

static void buffer_set_u8(Buffer *buffer, size_t offset, uint8_t value) {
    if (offset >= buffer->size) die("internal buffer patch out of range");
    buffer->data[offset] = value;
}

static void set_bit(uint8_t *bits, unsigned int index, int on) {
    uint8_t mask = (uint8_t)(0x80u >> (index & 7u));
    if (on) bits[index >> 3] |= mask;
    else bits[index >> 3] &= (uint8_t)~mask;
}

static int get_bit(const uint8_t *bits, unsigned int index) {
    return (bits[index >> 3] & (uint8_t)(0x80u >> (index & 7u))) != 0;
}

static int read_token(FILE *file, char *token, size_t capacity) {
    int ch;
    size_t used = 0;
    do {
        ch = fgetc(file);
        if (ch == '#') {
            while (ch != '\n' && ch != EOF) ch = fgetc(file);
        }
    } while (ch != EOF && isspace((unsigned char)ch));
    if (ch == EOF) return 0;
    while (ch != EOF && !isspace((unsigned char)ch)) {
        if (used + 1u < capacity) token[used++] = (char)ch;
        ch = fgetc(file);
    }
    token[used] = 0;
    return 1;
}

static void load_pgm_1bpp(const char *path, uint8_t *bits) {
    FILE *file = fopen(path, "rb");
    char token[64];
    unsigned int width;
    unsigned int height;
    unsigned int maxval;
    unsigned int pixel;
    if (file == NULL) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    if (!read_token(file, token, sizeof(token)) || strcmp(token, "P5") != 0) die("expected binary PGM/P5 input");
    if (!read_token(file, token, sizeof(token))) die("missing PGM width");
    width = (unsigned int)strtoul(token, NULL, 10);
    if (!read_token(file, token, sizeof(token))) die("missing PGM height");
    height = (unsigned int)strtoul(token, NULL, 10);
    if (!read_token(file, token, sizeof(token))) die("missing PGM max value");
    maxval = (unsigned int)strtoul(token, NULL, 10);
    if (width != PCV_W || height != PCV_H || maxval == 0u || maxval > 255u) die("PGM must be P5 320x240 with max value 1..255");
    memset(bits, 0, PCV_FRAME_BYTES);
    for (pixel = 0; pixel < PCV_W * PCV_H; ++pixel) {
        int value = fgetc(file);
        if (value == EOF) die("truncated PGM data");
        set_bit(bits, pixel, (unsigned int)value >= (maxval + 1u) / 2u);
    }
    fclose(file);
}

static void get_tile_bytes(const uint8_t *frame, unsigned int tile_index, uint8_t *tile) {
    unsigned int tx = tile_index % PCV_TILES_X;
    unsigned int ty = tile_index / PCV_TILES_X;
    unsigned int row;
    memset(tile, 0, PCV_TILE_BYTES);
    for (row = 0; row < PCV_TILE_H; ++row) {
        unsigned int y = ty * PCV_TILE_H + row;
        unsigned int x0 = tx * PCV_TILE_W;
        unsigned int byte0 = (y * PCV_W + x0) / 8u;
        tile[row * 2u + 0u] = frame[byte0 + 0u];
        tile[row * 2u + 1u] = frame[byte0 + 1u];
    }
}

static int tile_is_solid(const uint8_t *tile, uint8_t value) {
    unsigned int i;
    for (i = 0; i < PCV_TILE_BYTES; ++i) {
        if (tile[i] != value) return 0;
    }
    return 1;
}

static unsigned int encode_tile_rle(const uint8_t *tile, uint8_t *out) {
    unsigned int pos = 0;
    unsigned int out_pos = 0;
    while (pos < PCV_TILE_W * PCV_TILE_H) {
        int bit = get_bit(tile, pos);
        unsigned int run = 1;
        while (pos + run < PCV_TILE_W * PCV_TILE_H && run < 127u && get_bit(tile, pos + run) == bit) run += 1u;
        out[out_pos++] = (uint8_t)(run | (bit ? 0x80u : 0u));
        pos += run;
    }
    return out_pos;
}

static unsigned int encode_xor_bits(const uint8_t *tile, uint8_t *out) {
    unsigned int pos;
    unsigned int out_pos = 1;
    for (pos = 0; pos < PCV_TILE_W * PCV_TILE_H; ++pos) {
        if (get_bit(tile, pos)) out[out_pos++] = (uint8_t)pos;
    }
    out[0] = (uint8_t)(out_pos - 1u);
    return out_pos;
}

static unsigned int encode_xor_nibbles(const uint8_t *tile, uint8_t *out) {
    unsigned int pos;
    unsigned int count = 0;
    unsigned int out_pos = 1;
    unsigned int previous = 0;
    int have_previous = 0;
    for (pos = 0; pos < PCV_TILE_W * PCV_TILE_H; ++pos) {
        if (get_bit(tile, pos)) {
            unsigned int gap = have_previous ? pos - previous - 1u : pos;
            if (gap > 15u) return 0;
            if ((count & 1u) == 0u) {
                out[out_pos++] = (uint8_t)(gap << 4);
            } else {
                out[out_pos - 1u] |= (uint8_t)gap;
            }
            previous = pos;
            have_previous = 1;
            count += 1u;
        }
    }
    out[0] = (uint8_t)count;
    return out_pos;
}

static unsigned int lz_hash(const uint8_t *data, size_t pos, size_t size) {
    uint32_t value;
    if (pos + 2u >= size) return 0;
    value = ((uint32_t)data[pos] << 4) ^ ((uint32_t)data[pos + 1u] << 2) ^ (uint32_t)data[pos + 2u];
    return value & 4095u;
}

static void lz_insert(const uint8_t *data, size_t pos, size_t size, int *head, int *prev) {
    unsigned int hash;
    if (pos + PCV_LZ_MIN_MATCH > size) return;
    hash = lz_hash(data, pos, size);
    prev[pos] = head[hash];
    head[hash] = (int)pos;
}

static size_t lzss_compress(const uint8_t *input, size_t input_size, Buffer *out) {
    int head[4096];
    int *prev;
    size_t pos = 0;
    unsigned int i;
    out->size = 0;
    if (input_size == 0u) return 0;
    prev = (int *)xmalloc(input_size * sizeof(prev[0]));
    for (i = 0; i < 4096u; ++i) head[i] = -1;
    for (pos = 0; pos < input_size; ) {
        size_t flags_pos = out->size;
        uint8_t flags = 0;
        unsigned int bit;
        buffer_u8(out, 0);
        for (bit = 0; bit < 8u && pos < input_size; ++bit) {
            size_t best_len = 0;
            size_t best_offset = 0;
            if (pos + PCV_LZ_MIN_MATCH <= input_size) {
                int candidate = head[lz_hash(input, pos, input_size)];
                unsigned int checked = 0;
                while (candidate >= 0 && checked < 128u) {
                    size_t candidate_pos = (size_t)candidate;
                    size_t offset = pos - candidate_pos;
                    if (offset == 0u || offset > PCV_LZ_WINDOW) break;
                    if (input[candidate_pos] == input[pos] && input[candidate_pos + 1u] == input[pos + 1u] && input[candidate_pos + 2u] == input[pos + 2u]) {
                        size_t len = PCV_LZ_MIN_MATCH;
                        while (len < PCV_LZ_MAX_MATCH && pos + len < input_size && input[candidate_pos + len] == input[pos + len]) len += 1u;
                        if (len > best_len) {
                            best_len = len;
                            best_offset = offset;
                            if (len == PCV_LZ_MAX_MATCH) break;
                        }
                    }
                    candidate = prev[candidate_pos];
                    checked += 1u;
                }
            }
            if (best_len >= PCV_LZ_MIN_MATCH) {
                size_t encoded_offset = best_offset - 1u;
                flags |= (uint8_t)(1u << bit);
                buffer_u8(out, (uint8_t)encoded_offset);
                if (best_len >= PCV_LZ_MIN_MATCH + 15u) {
                    buffer_u8(out, (uint8_t)(0xf0u | (encoded_offset >> 8)));
                    buffer_u8(out, (uint8_t)(best_len - (PCV_LZ_MIN_MATCH + 15u)));
                } else {
                    buffer_u8(out, (uint8_t)(((best_len - PCV_LZ_MIN_MATCH) << 4) | (encoded_offset >> 8)));
                }
                for (i = 0; i < best_len; ++i) lz_insert(input, pos + i, input_size, head, prev);
                pos += best_len;
            } else {
                buffer_u8(out, input[pos]);
                lz_insert(input, pos, input_size, head, prev);
                pos += 1u;
            }
        }
        buffer_set_u8(out, flags_pos, flags);
    }
    free(prev);
    return out->size;
}

static int lzss_decompress(const uint8_t *input, size_t input_size, uint8_t *out, size_t out_size) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    while (in_pos < input_size && out_pos < out_size) {
        uint8_t flags = input[in_pos++];
        unsigned int bit;
        for (bit = 0; bit < 8u && out_pos < out_size; ++bit) {
            if ((flags & (uint8_t)(1u << bit)) == 0u) {
                if (in_pos >= input_size) return 0;
                out[out_pos++] = input[in_pos++];
            } else {
                unsigned int b0;
                unsigned int b1;
                size_t offset;
                size_t len;
                size_t i;
                if (in_pos + 2u > input_size) return 0;
                b0 = input[in_pos++];
                b1 = input[in_pos++];
                offset = (((size_t)b1 & 15u) << 8) | (size_t)b0;
                offset += 1u;
                if ((b1 >> 4) == 15u) {
                    if (in_pos >= input_size) return 0;
                    len = PCV_LZ_MIN_MATCH + 15u + input[in_pos++];
                } else {
                    len = (b1 >> 4) + PCV_LZ_MIN_MATCH;
                }
                if (offset > out_pos) return 0;
                for (i = 0; i < len && out_pos < out_size; ++i) {
                    out[out_pos] = out[out_pos - offset];
                    out_pos += 1u;
                }
            }
        }
    }
    return in_pos == input_size && out_pos == out_size;
}

static void write_pcv_header(FILE *out, const char *magic, unsigned int frames, unsigned int fps10) {
    fputc('P', out);
    fputc('C', out);
    fputc('V', out);
    fputc(magic[3], out);
    fputc((int)PCV_W, out);
    fputc((int)(PCV_W >> 8), out);
    fputc((int)PCV_H, out);
    fputc((int)(PCV_H >> 8), out);
    fputc((int)PCV_TILE_W, out);
    fputc((int)PCV_TILE_H, out);
    fputc((int)frames, out);
    fputc((int)(frames >> 8), out);
    fputc((int)fps10, out);
    fputc((int)(fps10 >> 8), out);
}

static void emit_tile_command(Buffer *frame_out, unsigned int tile_index, unsigned int mode, unsigned int *next_tile) {
    unsigned int delta = tile_index - *next_tile;
    if (tile_index >= *next_tile && delta < 31u) {
        buffer_u8(frame_out, (uint8_t)((mode << 5) | delta));
    } else {
        buffer_u8(frame_out, (uint8_t)((mode << 5) | 31u));
        buffer_u16(frame_out, tile_index);
    }
    *next_tile = tile_index + 1u;
}

static void encode_frame(Buffer *frame_out, const uint8_t *current, const uint8_t *previous, Stats *stats) {
    uint8_t tile[PCV_TILE_BYTES];
    uint8_t old_tile[PCV_TILE_BYTES];
    uint8_t xor_tile[PCV_TILE_BYTES];
    uint8_t rle[PCV_TILE_W * PCV_TILE_H];
    uint8_t xor_rle[PCV_TILE_W * PCV_TILE_H];
    uint8_t xor_bits[PCV_TILE_W * PCV_TILE_H + 1u];
    uint8_t xor_nibbles[PCV_TILE_W * PCV_TILE_H / 2u + 1u];
    unsigned int tile_index;
    unsigned int next_tile = 0;
    uint32_t changed_pixels = 0;
    frame_out->size = 0;
    for (tile_index = 0; tile_index < PCV_TILE_COUNT; ++tile_index) {
        get_tile_bytes(current, tile_index, tile);
        get_tile_bytes(previous, tile_index, old_tile);
        if (memcmp(tile, old_tile, sizeof(tile)) == 0) continue;
        if (tile_is_solid(tile, 0x00u)) {
            emit_tile_command(frame_out, tile_index, PCV_CMD_SOLID0, &next_tile);
        } else if (tile_is_solid(tile, 0xffu)) {
            emit_tile_command(frame_out, tile_index, PCV_CMD_SOLID1, &next_tile);
        } else {
            unsigned int i;
            unsigned int rle_len = encode_tile_rle(tile, rle);
            unsigned int xor_rle_len;
            unsigned int xor_bits_len;
            unsigned int xor_nibbles_len;
            for (i = 0; i < PCV_TILE_BYTES; ++i) xor_tile[i] = (uint8_t)(tile[i] ^ old_tile[i]);
            xor_rle_len = encode_tile_rle(xor_tile, xor_rle);
            xor_bits_len = encode_xor_bits(xor_tile, xor_bits);
            xor_nibbles_len = encode_xor_nibbles(xor_tile, xor_nibbles);
            if (xor_nibbles_len != 0u && xor_nibbles_len < xor_bits_len && xor_nibbles_len < xor_rle_len + 1u && xor_nibbles_len < rle_len + 1u && xor_nibbles_len < PCV_TILE_BYTES) {
                emit_tile_command(frame_out, tile_index, PCV_CMD_XOR_NIBS1, &next_tile);
                buffer_bytes(frame_out, xor_nibbles, xor_nibbles_len);
            } else if (xor_bits_len < xor_rle_len + 1u && xor_bits_len < rle_len + 1u && xor_bits_len < PCV_TILE_BYTES) {
                emit_tile_command(frame_out, tile_index, PCV_CMD_XOR_BITS1, &next_tile);
                buffer_bytes(frame_out, xor_bits, xor_bits_len);
            } else if (xor_rle_len + 1u < rle_len + 1u && xor_rle_len + 1u < PCV_TILE_BYTES) {
                emit_tile_command(frame_out, tile_index, PCV_CMD_XOR_RLE1, &next_tile);
                buffer_u8(frame_out, xor_rle_len);
                buffer_bytes(frame_out, xor_rle, xor_rle_len);
            } else if (rle_len + 1u < PCV_TILE_BYTES) {
                emit_tile_command(frame_out, tile_index, PCV_CMD_RLE1, &next_tile);
                buffer_u8(frame_out, rle_len);
                buffer_bytes(frame_out, rle, rle_len);
            } else {
                if (xor_bits_len < PCV_TILE_BYTES) {
                    emit_tile_command(frame_out, tile_index, PCV_CMD_XOR_RAW1, &next_tile);
                    buffer_bytes(frame_out, xor_tile, PCV_TILE_BYTES);
                } else {
                    emit_tile_command(frame_out, tile_index, PCV_CMD_RAW1, &next_tile);
                    buffer_bytes(frame_out, tile, PCV_TILE_BYTES);
                }
            }
        }
        stats->changed_tiles += 1u;
        changed_pixels += PCV_TILE_W * PCV_TILE_H;
    }
    stats->changed_pixels += changed_pixels;
    if (changed_pixels > stats->worst_changed_pixels) stats->worst_changed_pixels = changed_pixels;
}

static void stats_add_stored_frame(Stats *stats, uint32_t stored_payload_bytes, uint32_t stored_total_bytes) {
    stats->payload_bytes += stored_payload_bytes;
    stats->stream_bytes += stored_total_bytes;
    if (stored_total_bytes >= 2u && stored_total_bytes - 2u > stats->worst_frame_bytes) stats->worst_frame_bytes = stored_total_bytes - 2u;
}

static void print_stats(const Stats *stats, unsigned int fps10) {
    uint64_t avg_bytes = stats->frames == 0u ? 0u : stats->stream_bytes / stats->frames;
    uint64_t avg_pixels = stats->frames == 0u ? 0u : stats->changed_pixels / stats->frames;
    uint64_t avg_tiles = stats->frames == 0u ? 0u : stats->changed_tiles / stats->frames;
    uint64_t lcd_us = avg_pixels * PCV_LCD_FULL_US / PCV_LCD_FULL_PIXELS;
    uint64_t bytes_per_second = fps10 == 0u ? 0u : avg_bytes * fps10 / 10u;
    printf("frames %u\n", stats->frames);
    printf("fps %.1f\n", fps10 / 10.0);
    printf("stream_bytes %llu\n", (unsigned long long)stats->stream_bytes);
    printf("avg_bytes_per_frame %llu\n", (unsigned long long)avg_bytes);
    printf("worst_frame_bytes %u\n", stats->worst_frame_bytes + 2u);
    printf("avg_changed_tiles %llu\n", (unsigned long long)avg_tiles);
    printf("avg_changed_pixels %llu\n", (unsigned long long)avg_pixels);
    printf("worst_changed_pixels %u\n", stats->worst_changed_pixels);
    printf("estimated_lcd_us_per_frame %llu\n", (unsigned long long)lcd_us);
    printf("estimated_lcd_fps %.1f\n", lcd_us == 0u ? 0.0 : 1000000.0 / (double)lcd_us);
    printf("bytes_per_second %llu\n", (unsigned long long)bytes_per_second);
}

static void write_header_file(const char *path, const char *name, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    size_t i;
    if (file == NULL) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    fprintf(file, "#ifndef PICOCALC_GENERATED_VIDEO_STREAM_H\n#define PICOCALC_GENERATED_VIDEO_STREAM_H\n\n");
    fprintf(file, "static const unsigned char %s[] = {\n", name);
    for (i = 0; i < size; ++i) {
        if ((i % 12u) == 0u) fprintf(file, "   ");
        fprintf(file, " 0x%02x%s", data[i], i + 1u == size ? "" : ",");
        if ((i % 12u) == 11u || i + 1u == size) fprintf(file, "\n");
    }
    fprintf(file, "};\nstatic const unsigned int %s_size = %u;\n\n#endif\n", name, (unsigned int)size);
    fclose(file);
}

static const char *arg_value(int *index, int argc, char **argv) {
    *index += 1;
    if (*index >= argc) die("missing option value");
    return argv[*index];
}

static int command_encode(int argc, char **argv) {
    const char *input_pattern = NULL;
    const char *output_path = NULL;
    const char *header_path = NULL;
    const char *header_name = "g_badapple_video";
    unsigned int frames = 0;
    unsigned int fps10 = 150;
    uint8_t *current = (uint8_t *)xmalloc(PCV_FRAME_BYTES);
    uint8_t *previous = (uint8_t *)xmalloc(PCV_FRAME_BYTES);
    Buffer frame = {0};
    Buffer compressed = {0};
    Buffer stream = {0};
    Stats stats;
    FILE *out;
    unsigned int frame_index;
    int i;
    memset(&stats, 0, sizeof(stats));
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--input-pattern") == 0) input_pattern = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--output") == 0) output_path = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--header") == 0) header_path = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--name") == 0) header_name = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--frames") == 0) frames = (unsigned int)strtoul(arg_value(&i, argc, argv), NULL, 10);
        else if (strcmp(argv[i], "--fps10") == 0) fps10 = (unsigned int)strtoul(arg_value(&i, argc, argv), NULL, 10);
        else die("unknown encode option");
    }
    if (input_pattern == NULL || output_path == NULL || frames == 0u) die("usage: video_tile_codec encode --input-pattern frame_%05d.pgm --frames N --output out.pcv [--header out.h] [--fps10 150]");
    out = fopen(output_path, "wb");
    if (out == NULL) {
        fprintf(stderr, "open %s: %s\n", output_path, strerror(errno));
        exit(1);
    }
    memset(previous, 0, PCV_FRAME_BYTES);
    write_pcv_header(out, "PCV3", frames, fps10);
    buffer_reserve(&stream, 14u);
    buffer_bytes(&stream, (const uint8_t *)"PCV3", 4u);
    buffer_u16(&stream, PCV_W);
    buffer_u16(&stream, PCV_H);
    buffer_u8(&stream, PCV_TILE_W);
    buffer_u8(&stream, PCV_TILE_H);
    buffer_u16(&stream, frames);
    buffer_u16(&stream, fps10);
    stats.frames = frames;
    stats.stream_bytes = 14u;
    for (frame_index = 1; frame_index <= frames; ++frame_index) {
        char path[4096];
        snprintf(path, sizeof(path), input_pattern, frame_index);
        load_pgm_1bpp(path, current);
        encode_frame(&frame, current, previous, &stats);
        if (frame.size > 65535u) die("encoded frame exceeds 64 KiB");
        if (frame.size > PCV_MAX_FRAME_ENCODED) die("encoded frame exceeds player scratch buffer");
        lzss_compress(frame.data, frame.size, &compressed);
        if (compressed.size + 2u < frame.size && compressed.size < 32768u) {
            uint32_t stored_size = 0x8000u | (uint32_t)compressed.size;
            fputc((int)stored_size, out);
            fputc((int)(stored_size >> 8), out);
            fputc((int)frame.size, out);
            fputc((int)(frame.size >> 8), out);
            fwrite(compressed.data, 1, compressed.size, out);
            buffer_u16(&stream, stored_size);
            buffer_u16(&stream, (uint32_t)frame.size);
            buffer_bytes(&stream, compressed.data, compressed.size);
            stats_add_stored_frame(&stats, (uint32_t)compressed.size + 2u, (uint32_t)compressed.size + 4u);
        } else {
            fputc((int)frame.size, out);
            fputc((int)(frame.size >> 8), out);
            fwrite(frame.data, 1, frame.size, out);
            buffer_u16(&stream, (uint32_t)frame.size);
            buffer_bytes(&stream, frame.data, frame.size);
            stats_add_stored_frame(&stats, (uint32_t)frame.size, (uint32_t)frame.size + 2u);
        }
        memcpy(previous, current, PCV_FRAME_BYTES);
    }
    fclose(out);
    if (header_path != NULL) write_header_file(header_path, header_name, stream.data, stream.size);
    print_stats(&stats, fps10);
    free(current);
    free(previous);
    free(frame.data);
    free(compressed.data);
    free(stream.data);
    return 0;
}

static uint32_t read_u16(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8);
}

static int command_analyze(int argc, char **argv) {
    const char *input_path = NULL;
    FILE *file;
    uint8_t header[14];
    unsigned int frames;
    unsigned int fps10;
    unsigned int frame_index;
    Stats stats;
    int i;
    memset(&stats, 0, sizeof(stats));
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--input") == 0) input_path = arg_value(&i, argc, argv);
        else die("unknown analyze option");
    }
    if (input_path == NULL) die("usage: video_tile_codec analyze --input stream.pcv");
    file = fopen(input_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "open %s: %s\n", input_path, strerror(errno));
        exit(1);
    }
    if (fread(header, 1, sizeof(header), file) != sizeof(header) || (memcmp(header, "PCV1", 4) != 0 && memcmp(header, "PCV2", 4) != 0 && memcmp(header, "PCV3", 4) != 0)) die("bad PCV stream");
    frames = (unsigned int)read_u16(header + 10);
    fps10 = (unsigned int)read_u16(header + 12);
    stats.frames = frames;
    stats.stream_bytes = sizeof(header);
    for (frame_index = 0; frame_index < frames; ++frame_index) {
        uint8_t size_bytes[2];
        uint8_t frame_data[PCV_MAX_FRAME_ENCODED];
        uint32_t frame_size;
        uint32_t raw_frame_size;
        const uint8_t *parse_data;
        uint32_t frame_changed_pixels = 0;
        uint32_t next_tile = 0;
        uint32_t pos = 0;
        if (fread(size_bytes, 1, 2, file) != 2) die("truncated frame size");
        frame_size = read_u16(size_bytes);
        raw_frame_size = frame_size;
        if (header[3] == '3' && (frame_size & 0x8000u) != 0u) {
            uint8_t raw_size_bytes[2];
            uint8_t *compressed_data;
            uint32_t compressed_size = frame_size & 0x7fffu;
            if (fread(raw_size_bytes, 1, 2, file) != 2) die("truncated compressed frame raw size");
            raw_frame_size = read_u16(raw_size_bytes);
            if (raw_frame_size > sizeof(frame_data)) die("compressed frame exceeds analyzer scratch buffer");
            compressed_data = (uint8_t *)xmalloc(compressed_size == 0u ? 1u : compressed_size);
            if (fread(compressed_data, 1, compressed_size, file) != compressed_size) die("truncated compressed frame");
            if (!lzss_decompress(compressed_data, compressed_size, frame_data, raw_frame_size)) die("bad compressed frame");
            free(compressed_data);
            parse_data = frame_data;
            stats_add_stored_frame(&stats, compressed_size + 2u, compressed_size + 4u);
        } else {
            if (raw_frame_size > sizeof(frame_data)) die("frame exceeds analyzer scratch buffer");
            if (fread(frame_data, 1, raw_frame_size, file) != raw_frame_size) die("truncated frame");
            parse_data = frame_data;
            stats_add_stored_frame(&stats, raw_frame_size, raw_frame_size + 2u);
        }
        while (pos < raw_frame_size) {
            uint32_t tile_index;
            unsigned int mode;
            if (header[3] == '2' || header[3] == '3') {
                int control;
                unsigned int delta;
                if (pos >= raw_frame_size) die("truncated command");
                control = parse_data[pos++];
                mode = ((unsigned int)control) >> 5;
                delta = ((unsigned int)control) & 31u;
                if (delta == 31u) {
                    if (pos + 2u > raw_frame_size) die("truncated absolute tile index");
                    tile_index = read_u16(parse_data + pos);
                    pos += 2u;
                } else {
                    tile_index = next_tile + delta;
                }
                next_tile = tile_index + 1u;
            } else {
                if (pos + 3u > raw_frame_size) die("truncated command");
                tile_index = read_u16(parse_data + pos);
                pos += 3u;
                mode = parse_data[pos - 1u];
            }
            if (tile_index >= PCV_TILE_COUNT) die("bad tile index");
            stats.changed_tiles += 1u;
            stats.changed_pixels += PCV_TILE_W * PCV_TILE_H;
            frame_changed_pixels += PCV_TILE_W * PCV_TILE_H;
            if (mode == PCV_CMD_RAW1 || mode == PCV_CMD_XOR_RAW1) {
                if (pos + PCV_TILE_BYTES > raw_frame_size) die("truncated raw tile");
                pos += PCV_TILE_BYTES;
            } else if (mode == PCV_CMD_RLE1 || mode == PCV_CMD_XOR_RLE1 || mode == PCV_CMD_XOR_BITS1 || mode == PCV_CMD_XOR_NIBS1) {
                int length;
                uint32_t payload_len;
                if (pos >= raw_frame_size) die("truncated variable tile length");
                length = parse_data[pos++];
                if ((mode == PCV_CMD_XOR_BITS1 || mode == PCV_CMD_XOR_NIBS1) && length > 128) die("bad xor bits length");
                payload_len = mode == PCV_CMD_XOR_NIBS1 ? ((uint32_t)length + 1u) / 2u : (uint32_t)length;
                if (pos + payload_len > raw_frame_size) die("truncated variable tile payload");
                pos += payload_len;
            } else if (mode != PCV_CMD_SOLID0 && mode != PCV_CMD_SOLID1) {
                die("unknown PCV command");
            }
        }
        if (pos != raw_frame_size) die("bad frame command lengths");
        if (frame_changed_pixels > stats.worst_changed_pixels) stats.worst_changed_pixels = frame_changed_pixels;
    }
    fclose(file);
    print_stats(&stats, fps10);
    return 0;
}

static void make_synthetic_frame(uint8_t *bits, unsigned int frame) {
    unsigned int x;
    unsigned int y;
    memset(bits, 0, PCV_FRAME_BYTES);
    for (y = 0; y < PCV_H; ++y) {
        for (x = 0; x < PCV_W; ++x) {
            unsigned int cx = 32u + frame * 9u;
            unsigned int on = (x >= cx && x < cx + 48u && y >= 84u && y < 156u) || (((x + frame * 3u) ^ (y * 2u)) & 63u) < 5u;
            set_bit(bits, y * PCV_W + x, on != 0u);
        }
    }
}

static int command_selftest(void) {
    uint8_t *current = (uint8_t *)xmalloc(PCV_FRAME_BYTES);
    uint8_t *previous = (uint8_t *)xmalloc(PCV_FRAME_BYTES);
    Buffer frame = {0};
    Stats stats;
    unsigned int i;
    memset(&stats, 0, sizeof(stats));
    memset(previous, 0, PCV_FRAME_BYTES);
    stats.frames = 16u;
    stats.stream_bytes = 14u;
    for (i = 0; i < stats.frames; ++i) {
        make_synthetic_frame(current, i);
        encode_frame(&frame, current, previous, &stats);
        stats_add_stored_frame(&stats, (uint32_t)frame.size, (uint32_t)frame.size + 2u);
        memcpy(previous, current, PCV_FRAME_BYTES);
    }
    print_stats(&stats, 150u);
    free(current);
    free(previous);
    free(frame.data);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) die("usage: video_tile_codec encode|analyze|selftest ...");
    if (strcmp(argv[1], "encode") == 0) return command_encode(argc, argv);
    if (strcmp(argv[1], "analyze") == 0) return command_analyze(argc, argv);
    if (strcmp(argv[1], "selftest") == 0) return command_selftest();
    die("unknown command");
    return 1;
}