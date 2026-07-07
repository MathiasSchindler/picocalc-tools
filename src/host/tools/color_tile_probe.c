#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_W 320u
#define VIDEO_H 240u
#define TILE_W 16u
#define TILE_H 8u
#define TILES_X (VIDEO_W / TILE_W)
#define TILES_Y (VIDEO_H / TILE_H)
#define TILE_COUNT (TILES_X * TILES_Y)
#define TILE_PIXELS (TILE_W * TILE_H)
#define TILE_MASK_BYTES (TILE_PIXELS / 8u)
#define FRAME_RGB_BYTES (VIDEO_W * VIDEO_H * 3u)
#define LCD_FULL_US 119304u
#define LZ_WINDOW 4096u
#define LZ_MIN_MATCH 3u
#define LZ_MAX_MATCH 273u

enum {
    MODE_SOLID = 1,
    MODE_PAIR = 2
};

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Buffer;

typedef struct {
    uint64_t stream_bytes;
    uint64_t raw_payload_bytes;
    uint64_t changed_tiles;
    uint64_t skipped_tiles;
    uint64_t solid_tiles;
    uint64_t pair_tiles;
    uint64_t sse;
    uint32_t worst_frame_bytes;
    uint32_t worst_changed_tiles;
} Stats;

static void die(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static void *xmalloc(size_t size) {
    void *ptr = malloc(size == 0u ? 1u : size);
    if (ptr == NULL) die("out of memory");
    return ptr;
}

static void buffer_reserve(Buffer *buffer, size_t add) {
    size_t need = buffer->size + add;
    if (need <= buffer->capacity) return;
    if (buffer->capacity == 0u) buffer->capacity = 4096u;
    while (buffer->capacity < need) buffer->capacity *= 2u;
    buffer->data = (uint8_t *)realloc(buffer->data, buffer->capacity);
    if (buffer->data == NULL) die("out of memory");
}

static void buffer_u8(Buffer *buffer, uint8_t value) {
    buffer_reserve(buffer, 1u);
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

static int read_token(FILE *file, char *token, size_t capacity) {
    int ch;
    size_t used = 0u;
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

static void load_ppm_rgb24(const char *path, uint8_t *rgb) {
    FILE *file = fopen(path, "rb");
    char token[64];
    unsigned int width;
    unsigned int height;
    unsigned int maxval;
    if (file == NULL) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    if (!read_token(file, token, sizeof(token)) || strcmp(token, "P6") != 0) die("expected binary PPM/P6 input");
    if (!read_token(file, token, sizeof(token))) die("missing PPM width");
    width = (unsigned int)strtoul(token, NULL, 10);
    if (!read_token(file, token, sizeof(token))) die("missing PPM height");
    height = (unsigned int)strtoul(token, NULL, 10);
    if (!read_token(file, token, sizeof(token))) die("missing PPM max value");
    maxval = (unsigned int)strtoul(token, NULL, 10);
    if (width != VIDEO_W || height != VIDEO_H || maxval != 255u) die("PPM must be P6 320x240 with max value 255");
    if (fread(rgb, 1u, FRAME_RGB_BYTES, file) != FRAME_RGB_BYTES) die("truncated PPM data");
    fclose(file);
}

static uint8_t quant_rgb332(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)((r & 0xe0u) | ((g & 0xe0u) >> 3) | (b >> 6));
}

static void expand_rgb332(uint8_t index, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t rv = index >> 5;
    uint8_t gv = (index >> 2) & 7u;
    uint8_t bv = index & 3u;
    *r = (uint8_t)((rv << 5) | (rv << 2) | (rv >> 1));
    *g = (uint8_t)((gv << 5) | (gv << 2) | (gv >> 1));
    *b = (uint8_t)((bv << 6) | (bv << 4) | (bv << 2) | bv);
}

static uint16_t rgb332_to_rgb565(uint8_t index) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    expand_rgb332(index, &r, &g, &b);
    return (uint16_t)(((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (uint16_t)(b >> 3));
}

static unsigned int color_distance(uint8_t r, uint8_t g, uint8_t b, uint8_t index) {
    uint8_t pr;
    uint8_t pg;
    uint8_t pb;
    int dr;
    int dg;
    int db;
    expand_rgb332(index, &pr, &pg, &pb);
    dr = (int)r - (int)pr;
    dg = (int)g - (int)pg;
    db = (int)b - (int)pb;
    return (unsigned int)(dr * dr * 3 + dg * dg * 4 + db * db * 2);
}

static void emit_tile_command(Buffer *frame, unsigned int tile_index, unsigned int mode, unsigned int *next_tile) {
    unsigned int delta = tile_index - *next_tile;
    if (tile_index >= *next_tile && delta < 31u) {
        buffer_u8(frame, (uint8_t)((mode << 5) | delta));
    } else {
        buffer_u8(frame, (uint8_t)((mode << 5) | 31u));
        buffer_u16(frame, tile_index);
    }
    *next_tile = tile_index + 1u;
}

static void add_tile_sse(const uint8_t *rgb, unsigned int tile_index, const uint16_t *display, Stats *stats) {
    unsigned int tx = tile_index % TILES_X;
    unsigned int ty = tile_index / TILES_X;
    unsigned int y;
    for (y = 0u; y < TILE_H; ++y) {
        unsigned int x;
        for (x = 0u; x < TILE_W; ++x) {
            unsigned int local = y * TILE_W + x;
            unsigned int pixel = ((ty * TILE_H + y) * VIDEO_W + tx * TILE_W + x) * 3u;
            uint16_t out = display[local];
            int rr = (int)(((out >> 11) & 31u) * 255u / 31u);
            int rg = (int)(((out >> 5) & 63u) * 255u / 63u);
            int rb = (int)((out & 31u) * 255u / 31u);
            int dr = (int)rgb[pixel] - rr;
            int dg = (int)rgb[pixel + 1u] - rg;
            int db = (int)rgb[pixel + 2u] - rb;
            stats->sse += (uint64_t)(dr * dr + dg * dg + db * db);
        }
    }
}

static unsigned int lz_hash(const uint8_t *data, size_t pos, size_t size) {
    uint32_t value;
    if (pos + 2u >= size) return 0u;
    value = ((uint32_t)data[pos] << 4) ^ ((uint32_t)data[pos + 1u] << 2) ^ (uint32_t)data[pos + 2u];
    return value & 4095u;
}

static void lz_insert(const uint8_t *data, size_t pos, size_t size, int *head, int *prev) {
    unsigned int hash;
    if (pos + LZ_MIN_MATCH > size) return;
    hash = lz_hash(data, pos, size);
    prev[pos] = head[hash];
    head[hash] = (int)pos;
}

static size_t lzss_compress(const uint8_t *input, size_t input_size, Buffer *out) {
    int head[4096];
    int *prev;
    size_t pos = 0u;
    unsigned int i;
    out->size = 0u;
    prev = (int *)xmalloc(input_size * sizeof(prev[0]));
    for (i = 0u; i < 4096u; ++i) head[i] = -1;
    while (pos < input_size) {
        size_t flags_pos = out->size;
        uint8_t flags = 0u;
        unsigned int bit;
        buffer_u8(out, 0u);
        for (bit = 0u; bit < 8u && pos < input_size; ++bit) {
            size_t best_len = 0u;
            size_t best_offset = 0u;
            if (pos + LZ_MIN_MATCH <= input_size) {
                int candidate = head[lz_hash(input, pos, input_size)];
                unsigned int checked = 0u;
                while (candidate >= 0 && checked < 96u) {
                    size_t candidate_pos = (size_t)candidate;
                    size_t offset = pos - candidate_pos;
                    if (offset == 0u || offset > LZ_WINDOW) break;
                    if (input[candidate_pos] == input[pos] && input[candidate_pos + 1u] == input[pos + 1u] && input[candidate_pos + 2u] == input[pos + 2u]) {
                        size_t len = LZ_MIN_MATCH;
                        while (len < LZ_MAX_MATCH && pos + len < input_size && input[candidate_pos + len] == input[pos + len]) len += 1u;
                        if (len > best_len) {
                            best_len = len;
                            best_offset = offset;
                            if (len == LZ_MAX_MATCH) break;
                        }
                    }
                    candidate = prev[candidate_pos];
                    checked += 1u;
                }
            }
            if (best_len >= LZ_MIN_MATCH) {
                size_t encoded_offset = best_offset - 1u;
                flags |= (uint8_t)(1u << bit);
                buffer_u8(out, (uint8_t)encoded_offset);
                if (best_len >= LZ_MIN_MATCH + 15u) {
                    buffer_u8(out, (uint8_t)(0xf0u | (encoded_offset >> 8)));
                    buffer_u8(out, (uint8_t)(best_len - (LZ_MIN_MATCH + 15u)));
                } else {
                    buffer_u8(out, (uint8_t)(((best_len - LZ_MIN_MATCH) << 4) | (encoded_offset >> 8)));
                }
                for (i = 0u; i < best_len; ++i) lz_insert(input, pos + i, input_size, head, prev);
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

static void encode_frame(const uint8_t *rgb, uint16_t *previous, Buffer *frame, Buffer *compressed, Stats *stats, unsigned int min_changed_pixels) {
    unsigned int tile_index;
    unsigned int next_tile = 0u;
    uint32_t frame_changed = 0u;
    frame->size = 0u;
    for (tile_index = 0u; tile_index < TILE_COUNT; ++tile_index) {
        unsigned int tx = tile_index % TILES_X;
        unsigned int ty = tile_index / TILES_X;
        unsigned int histogram[256];
        uint8_t top0 = 0u;
        uint8_t top1 = 0u;
        unsigned int y;
        unsigned int i;
        uint8_t mask[TILE_MASK_BYTES];
        uint16_t rendered[TILE_PIXELS];
        unsigned int changed_pixels = 0u;
        memset(histogram, 0, sizeof(histogram));
        for (y = 0u; y < TILE_H; ++y) {
            unsigned int x;
            for (x = 0u; x < TILE_W; ++x) {
                unsigned int pixel = ((ty * TILE_H + y) * VIDEO_W + tx * TILE_W + x) * 3u;
                histogram[quant_rgb332(rgb[pixel], rgb[pixel + 1u], rgb[pixel + 2u])] += 1u;
            }
        }
        for (i = 0u; i < 256u; ++i) {
            if (histogram[i] > histogram[top0]) {
                top1 = top0;
                top0 = (uint8_t)i;
            } else if (i != top0 && histogram[i] > histogram[top1]) {
                top1 = (uint8_t)i;
            }
        }
        if (histogram[top1] == 0u) top1 = top0;
        memset(mask, 0, sizeof(mask));
        for (y = 0u; y < TILE_H; ++y) {
            unsigned int x;
            for (x = 0u; x < TILE_W; ++x) {
                unsigned int local = y * TILE_W + x;
                unsigned int pixel = ((ty * TILE_H + y) * VIDEO_W + tx * TILE_W + x) * 3u;
                unsigned int d0 = color_distance(rgb[pixel], rgb[pixel + 1u], rgb[pixel + 2u], top0);
                unsigned int d1 = color_distance(rgb[pixel], rgb[pixel + 1u], rgb[pixel + 2u], top1);
                uint8_t chosen = d1 < d0 ? top1 : top0;
                uint16_t out = rgb332_to_rgb565(chosen);
                if (chosen == top1) mask[local >> 3] |= (uint8_t)(0x80u >> (local & 7u));
                rendered[local] = out;
            }
        }
        for (i = 0u; i < TILE_PIXELS; ++i) {
            if (previous[tile_index * TILE_PIXELS + i] != rendered[i]) changed_pixels += 1u;
        }
        if (changed_pixels < min_changed_pixels) {
            add_tile_sse(rgb, tile_index, previous + tile_index * TILE_PIXELS, stats);
            stats->skipped_tiles += 1u;
            continue;
        }
        add_tile_sse(rgb, tile_index, rendered, stats);
        emit_tile_command(frame, tile_index, top0 == top1 ? MODE_SOLID : MODE_PAIR, &next_tile);
        if (top0 == top1) {
            buffer_u8(frame, top0);
            stats->solid_tiles += 1u;
        } else {
            buffer_u8(frame, top0);
            buffer_u8(frame, top1);
            for (i = 0u; i < TILE_MASK_BYTES; ++i) buffer_u8(frame, mask[i]);
            stats->pair_tiles += 1u;
        }
        memcpy(previous + tile_index * TILE_PIXELS, rendered, sizeof(rendered));
        stats->changed_tiles += 1u;
        frame_changed += 1u;
    }
    lzss_compress(frame->data, frame->size, compressed);
    stats->raw_payload_bytes += frame->size;
    if (compressed->size + 2u < frame->size) {
        stats->stream_bytes += compressed->size + 4u;
        if (compressed->size + 2u > stats->worst_frame_bytes) stats->worst_frame_bytes = (uint32_t)(compressed->size + 2u);
    } else {
        stats->stream_bytes += frame->size + 2u;
        if (frame->size > stats->worst_frame_bytes) stats->worst_frame_bytes = (uint32_t)frame->size;
    }
    if (frame_changed > stats->worst_changed_tiles) stats->worst_changed_tiles = frame_changed;
}

static const char *arg_value(int *index, int argc, char **argv) {
    *index += 1;
    if (*index >= argc) die("missing option value");
    return argv[*index];
}

static void print_stats(const Stats *stats, unsigned int frames, unsigned int fps10) {
    double pixels = (double)frames * (double)VIDEO_W * (double)VIDEO_H;
    double mse = stats->sse / (pixels * 3.0);
    double psnr = mse <= 0.0 ? 99.0 : 10.0 * log10((255.0 * 255.0) / mse);
    uint64_t avg_bytes = frames == 0u ? 0u : stats->stream_bytes / frames;
    uint64_t avg_tiles = frames == 0u ? 0u : stats->changed_tiles / frames;
    uint64_t lcd_us = avg_tiles * TILE_PIXELS * LCD_FULL_US / (VIDEO_W * VIDEO_H);
    uint64_t bytes_per_second = fps10 == 0u ? 0u : avg_bytes * fps10 / 10u;
    printf("frames %u\n", frames);
    printf("fps %.1f\n", fps10 / 10.0);
    printf("stream_bytes %llu\n", (unsigned long long)stats->stream_bytes);
    printf("raw_payload_bytes %llu\n", (unsigned long long)stats->raw_payload_bytes);
    printf("avg_bytes_per_frame %llu\n", (unsigned long long)avg_bytes);
    printf("worst_frame_bytes %u\n", stats->worst_frame_bytes + 2u);
    printf("avg_changed_tiles %llu\n", (unsigned long long)avg_tiles);
    printf("worst_changed_tiles %u\n", stats->worst_changed_tiles);
    printf("skipped_tiles %llu\n", (unsigned long long)stats->skipped_tiles);
    printf("solid_tiles %llu\n", (unsigned long long)stats->solid_tiles);
    printf("pair_tiles %llu\n", (unsigned long long)stats->pair_tiles);
    printf("estimated_lcd_us_per_frame %llu\n", (unsigned long long)lcd_us);
    printf("estimated_lcd_fps %.1f\n", lcd_us == 0u ? 0.0 : 1000000.0 / (double)lcd_us);
    printf("bytes_per_second %llu\n", (unsigned long long)bytes_per_second);
    printf("rgb_psnr_db %.2f\n", psnr);
}

static void write_stream_header(Buffer *stream, FILE *file, unsigned int frames, unsigned int fps10) {
    const uint8_t magic[4] = {'P', 'C', 'V', 'C'};
    if (file != NULL) fwrite(magic, 1u, sizeof(magic), file);
    buffer_bytes(stream, magic, sizeof(magic));
    buffer_u16(stream, VIDEO_W);
    buffer_u16(stream, VIDEO_H);
    buffer_u8(stream, TILE_W);
    buffer_u8(stream, TILE_H);
    buffer_u16(stream, frames);
    buffer_u16(stream, fps10);
    if (file != NULL) {
        fputc((int)VIDEO_W, file);
        fputc((int)(VIDEO_W >> 8), file);
        fputc((int)VIDEO_H, file);
        fputc((int)(VIDEO_H >> 8), file);
        fputc((int)TILE_W, file);
        fputc((int)TILE_H, file);
        fputc((int)frames, file);
        fputc((int)(frames >> 8), file);
        fputc((int)fps10, file);
        fputc((int)(fps10 >> 8), file);
    }
}

static void write_header_file(const char *path, const char *name, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    size_t i;
    if (file == NULL) {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    fprintf(file, "#ifndef PICOCALC_GENERATED_COLOR_VIDEO_STREAM_H\n#define PICOCALC_GENERATED_COLOR_VIDEO_STREAM_H\n\n");
    fprintf(file, "static const unsigned char %s[] = {\n", name);
    for (i = 0u; i < size; ++i) {
        if ((i % 12u) == 0u) fprintf(file, "   ");
        fprintf(file, " 0x%02x%s", data[i], i + 1u == size ? "" : ",");
        if ((i % 12u) == 11u || i + 1u == size) fprintf(file, "\n");
    }
    fprintf(file, "};\nstatic const unsigned int %s_size = %u;\n\n#endif\n", name, (unsigned int)size);
    fclose(file);
}

static void write_frame(FILE *file, Buffer *stream, const Buffer *frame, const Buffer *compressed) {
    if (compressed->size + 2u < frame->size && compressed->size < 32768u) {
        uint32_t stored_size = 0x8000u | (uint32_t)compressed->size;
        if (file != NULL) {
            fputc((int)stored_size, file);
            fputc((int)(stored_size >> 8), file);
            fputc((int)frame->size, file);
            fputc((int)(frame->size >> 8), file);
            fwrite(compressed->data, 1u, compressed->size, file);
        }
        buffer_u16(stream, stored_size);
        buffer_u16(stream, (uint32_t)frame->size);
        buffer_bytes(stream, compressed->data, compressed->size);
    } else {
        if (file != NULL) {
            fputc((int)frame->size, file);
            fputc((int)(frame->size >> 8), file);
            fwrite(frame->data, 1u, frame->size, file);
        }
        buffer_u16(stream, (uint32_t)frame->size);
        buffer_bytes(stream, frame->data, frame->size);
    }
}

int main(int argc, char **argv) {
    const char *input_pattern = NULL;
    const char *output_path = NULL;
    const char *header_path = NULL;
    const char *header_name = "picocalc_color_video_stream";
    unsigned int frames = 0u;
    unsigned int fps10 = 150u;
    unsigned int min_changed_pixels = 1u;
    uint8_t *rgb = (uint8_t *)xmalloc(FRAME_RGB_BYTES);
    uint16_t *previous = (uint16_t *)xmalloc(TILE_COUNT * TILE_PIXELS * sizeof(previous[0]));
    Buffer frame = {0};
    Buffer compressed = {0};
    Buffer stream = {0};
    Stats stats;
    FILE *out = NULL;
    unsigned int frame_index;
    int i;
    memset(&stats, 0, sizeof(stats));
    memset(previous, 0, TILE_COUNT * TILE_PIXELS * sizeof(previous[0]));
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--input-pattern") == 0) input_pattern = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--output") == 0) output_path = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--header") == 0) header_path = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--name") == 0) header_name = arg_value(&i, argc, argv);
        else if (strcmp(argv[i], "--frames") == 0) frames = (unsigned int)strtoul(arg_value(&i, argc, argv), NULL, 10);
        else if (strcmp(argv[i], "--fps10") == 0) fps10 = (unsigned int)strtoul(arg_value(&i, argc, argv), NULL, 10);
        else if (strcmp(argv[i], "--min-changed-pixels") == 0) min_changed_pixels = (unsigned int)strtoul(arg_value(&i, argc, argv), NULL, 10);
        else die("usage: color_tile_probe --input-pattern frame_%05d.ppm --frames N [--fps10 150] [--output out.pcvc] [--header out.h]");
    }
    if (input_pattern == NULL || frames == 0u) die("usage: color_tile_probe --input-pattern frame_%05d.ppm --frames N [--fps10 150] [--output out.pcvc] [--header out.h]");
    if (output_path != NULL) {
        out = fopen(output_path, "wb");
        if (out == NULL) {
            fprintf(stderr, "open %s: %s\n", output_path, strerror(errno));
            exit(1);
        }
    }
    stats.stream_bytes = 14u;
    write_stream_header(&stream, out, frames, fps10);
    for (frame_index = 1u; frame_index <= frames; ++frame_index) {
        char path[4096];
        snprintf(path, sizeof(path), input_pattern, frame_index);
        load_ppm_rgb24(path, rgb);
        encode_frame(rgb, previous, &frame, &compressed, &stats, min_changed_pixels);
        write_frame(out, &stream, &frame, &compressed);
    }
    if (out != NULL) fclose(out);
    if (header_path != NULL) write_header_file(header_path, header_name, stream.data, stream.size);
    print_stats(&stats, frames, fps10);
    free(rgb);
    free(previous);
    free(frame.data);
    free(compressed.data);
    free(stream.data);
    return 0;
}