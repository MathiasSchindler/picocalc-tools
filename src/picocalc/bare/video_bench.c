#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#define LCD_W 320
#define LCD_H 320
#define BADGER_W 320
#define BADGER_H 240
#define LOW_W 160
#define LOW_H 120
#define TILE_W 16
#define TILE_H 8
#define TEST_FRAMES 4u

typedef struct {
    const char *name;
    uint32_t source_bytes;
    uint32_t display_pixels;
    uint32_t changed_pixels;
    uint32_t total_us;
    uint32_t cpu_us;
    uint32_t push_us;
    uint32_t fps10;
    uint32_t kpix_per_s;
} VideoBenchResult;

static uint8_t g_line_rgb[LCD_W * 3];
static uint8_t g_tile_rgb[TILE_W * 2 * 3];
static uint8_t g_1bpp_320[(BADGER_W * BADGER_H) / 8];
static uint8_t g_1bpp_a[(LOW_W * LOW_H) / 8];
static uint8_t g_1bpp_b[(LOW_W * LOW_H) / 8];
static uint8_t g_2bpp[(LOW_W * LOW_H) / 4];
static uint8_t g_rle[LOW_H * (LOW_W + 1)];
static uint8_t g_decode_sink[LOW_W * 3];
static volatile uint32_t g_sink;

static uint32_t timer_us(void) {
    return reg_time_us();
}

static uint32_t elapsed_us(uint32_t start, uint32_t end) {
    uint32_t elapsed = end - start;
    return elapsed == 0u ? 1u : elapsed;
}

static uint32_t rgb(unsigned int r, unsigned int g, unsigned int b) {
    return ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

static void rgb_to_bytes(uint32_t color, uint8_t *out) {
    out[0] = (uint8_t)(color >> 16);
    out[1] = (uint8_t)(color >> 8);
    out[2] = (uint8_t)color;
}

static int abs_i(int value) {
    return value < 0 ? -value : value;
}

static uint32_t pattern_luma(int x, int y, int w, int h, uint32_t frame) {
    int cx = (int)((frame * 11u) % (uint32_t)(w + 96)) - 48;
    int cy = h / 2 + (int)((frame * 5u) % 41u) - 20;
    int dx = x - cx;
    int dy = y - cy;
    int r = w / 6 + (int)(frame & 7u);
    int d2 = dx * dx + dy * dy;
    int wing = abs_i((x - w / 2) * 3 + (y - h / 2) * 2 - (int)(frame * 9u) % (w * 2));
    int slash = abs_i((x * 2 - y * 3 + (int)frame * 13) % 96);

    if (d2 < r * r) return 255u;
    if (d2 < (r + 5) * (r + 5)) return 170u;
    if (wing < 18 && y > h / 4 && y < h * 3 / 4) return 230u;
    if (slash < 7) return 80u;
    if (((x / 20 + y / 14 + (int)frame / 3) & 1) != 0) return 16u;
    return 0u;
}

static void set_1bpp(uint8_t *bits, int index, int on) {
    uint8_t mask = (uint8_t)(0x80u >> (index & 7));
    if (on) bits[index >> 3] |= mask;
    else bits[index >> 3] &= (uint8_t)~mask;
}

static int get_1bpp(const uint8_t *bits, int index) {
    return (bits[index >> 3] & (uint8_t)(0x80u >> (index & 7))) != 0;
}

static void set_2bpp(uint8_t *bits, int index, uint32_t value) {
    int shift = 6 - (index & 3) * 2;
    bits[index >> 2] = (uint8_t)((bits[index >> 2] & ~(3u << shift)) | ((value & 3u) << shift));
}

static uint32_t get_2bpp(const uint8_t *bits, int index) {
    int shift = 6 - (index & 3) * 2;
    return (bits[index >> 2] >> shift) & 3u;
}

static void make_1bpp(uint8_t *bits, int w, int h, uint32_t frame) {
    int x;
    int y;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            set_1bpp(bits, y * w + x, pattern_luma(x, y, w, h, frame) >= 128u);
        }
    }
}

static void make_2bpp(uint8_t *bits, int w, int h, uint32_t frame) {
    int x;
    int y;
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            uint32_t luma = pattern_luma(x, y, w, h, frame);
            set_2bpp(bits, y * w + x, luma >> 6);
        }
    }
}

static void make_rgb_line(int y, uint32_t frame) {
    int x;
    for (x = 0; x < BADGER_W; ++x) {
        uint32_t luma = pattern_luma(x, y, BADGER_W, BADGER_H, frame);
        rgb_to_bytes(rgb(luma, luma, luma), g_line_rgb + x * 3);
    }
}

static void pack_1bpp_scaled_row(const uint8_t *bits, int y, int w) {
    int x;
    for (x = 0; x < w; ++x) {
        uint32_t color = get_1bpp(bits, y * w + x) ? 0xffffffu : 0x000000u;
        rgb_to_bytes(color, g_line_rgb + x * 6);
        rgb_to_bytes(color, g_line_rgb + x * 6 + 3);
    }
}

static void push_scaled_row(int x0, int y0, int y, int w) {
    picocalc_lcd_blit_rgb(x0, y0 + y * 2, w * 2, 1, g_line_rgb);
    picocalc_lcd_blit_rgb(x0, y0 + y * 2 + 1, w * 2, 1, g_line_rgb);
}

static void pack_2bpp_scaled_row(const uint8_t *bits, int y, int w) {
    static const uint32_t colors[4] = { 0x000000u, 0x555555u, 0xaaaaaau, 0xffffffu };
    int x;
    for (x = 0; x < w; ++x) {
        uint32_t color = colors[get_2bpp(bits, y * w + x)];
        rgb_to_bytes(color, g_line_rgb + x * 6);
        rgb_to_bytes(color, g_line_rgb + x * 6 + 3);
    }
}

static void pack_1bpp_full_row(const uint8_t *bits, int y) {
    int x;
    for (x = 0; x < BADGER_W; ++x) {
        uint32_t color = get_1bpp(bits, y * BADGER_W + x) ? 0xffffffu : 0x000000u;
        rgb_to_bytes(color, g_line_rgb + x * 3);
    }
}

static void push_full_row(int y) {
    picocalc_lcd_blit_rgb(0, 40 + y, BADGER_W, 1, g_line_rgb);
}

static int tile_changed(const uint8_t *a, const uint8_t *b, int tile_x, int tile_y) {
    int x;
    int y;
    for (y = 0; y < TILE_H; ++y) {
        for (x = 0; x < TILE_W; ++x) {
            int pixel = (tile_y + y) * LOW_W + tile_x + x;
            if (get_1bpp(a, pixel) != get_1bpp(b, pixel)) return 1;
        }
    }
    return 0;
}

static void pack_scaled_tile_row(const uint8_t *bits, int tile_x, int tile_y, int row) {
    int x;
    for (x = 0; x < TILE_W; ++x) {
        int pixel = (tile_y + row) * LOW_W + tile_x + x;
        uint32_t color = get_1bpp(bits, pixel) ? 0xffffffu : 0x000000u;
        rgb_to_bytes(color, g_tile_rgb + x * 6);
        rgb_to_bytes(color, g_tile_rgb + x * 6 + 3);
    }
}

static void push_scaled_tile_row(int tile_x, int tile_y, int row) {
    picocalc_lcd_blit_rgb(tile_x * 2, 40 + (tile_y + row) * 2, TILE_W * 2, 1, g_tile_rgb);
    picocalc_lcd_blit_rgb(tile_x * 2, 40 + (tile_y + row) * 2 + 1, TILE_W * 2, 1, g_tile_rgb);
}

static uint32_t make_rle_frame(uint8_t *out, uint32_t frame) {
    uint32_t used = 0;
    int y;
    for (y = 0; y < LOW_H; ++y) {
        int x = 0;
        while (x < LOW_W) {
            int on = pattern_luma(x, y, LOW_W, LOW_H, frame) >= 128u;
            int run = 1;
            while (x + run < LOW_W && run < 127 && (pattern_luma(x + run, y, LOW_W, LOW_H, frame) >= 128u) == on) {
                run += 1;
            }
            out[used++] = (uint8_t)(run | (on ? 0x80 : 0x00));
            x += run;
        }
        out[used++] = 0u;
    }
    return used;
}

static void decode_rle_cpu(const uint8_t *rle, uint32_t frame_bytes) {
    uint32_t pos = 0;
    int y;
    for (y = 0; y < LOW_H && pos < frame_bytes; ++y) {
        int x = 0;
        while (x < LOW_W && pos < frame_bytes) {
            uint8_t token = rle[pos++];
            int run = token & 0x7f;
            uint8_t value = (token & 0x80) ? 255u : 0u;
            int i;
            if (run == 0) break;
            for (i = 0; i < run && x < LOW_W; ++i, ++x) {
                g_decode_sink[x * 3 + 0] = value;
                g_decode_sink[x * 3 + 1] = value;
                g_decode_sink[x * 3 + 2] = value;
            }
        }
        g_sink ^= g_decode_sink[(unsigned int)y % sizeof(g_decode_sink)];
    }
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

static void draw_status(const char *name, uint32_t frame, uint32_t total) {
    char line[48];
    int pos = 0;
    picocalc_lcd_fill_rect(0, 0, 319, 35, 0x000000u);
    draw_text_line(0, "VIDEO PIPE BENCH", 0x00ff80u);
    append_text(line, &pos, sizeof(line), name);
    append_text(line, &pos, sizeof(line), " f ");
    append_u32(line, &pos, sizeof(line), frame + 1u);
    append_char(line, &pos, sizeof(line), '/');
    append_u32(line, &pos, sizeof(line), total);
    finish_line(line, &pos, sizeof(line));
    draw_text_line(1, line, 0xffffffu);
}

static void finish_result(VideoBenchResult *result, uint32_t frames, uint32_t elapsed, uint32_t display_pixels) {
    result->total_us = elapsed;
    result->display_pixels = display_pixels * frames;
    result->fps10 = frames * 10000000u / elapsed;
    result->kpix_per_s = (display_pixels * frames) / elapsed;
}

static void bench_rgb_full(VideoBenchResult *result) {
    uint32_t frame;
    uint32_t start;
    result->name = "RGB24 320x240";
    result->source_bytes = BADGER_W * BADGER_H * 3u;
    result->changed_pixels = BADGER_W * BADGER_H * TEST_FRAMES;
    draw_status(result->name, 0, TEST_FRAMES);
    result->cpu_us = 0;
    result->push_us = 0;
    start = timer_us();
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        int y;
        for (y = 0; y < BADGER_H; ++y) {
            uint32_t t0 = timer_us();
            make_rgb_line(y, frame);
            result->cpu_us += elapsed_us(t0, timer_us());
            t0 = timer_us();
            picocalc_lcd_blit_rgb(0, 40 + y, BADGER_W, 1, g_line_rgb);
            result->push_us += elapsed_us(t0, timer_us());
        }
    }
    finish_result(result, TEST_FRAMES, elapsed_us(start, timer_us()), BADGER_W * BADGER_H);
}

static void bench_1bpp_low(VideoBenchResult *result) {
    uint32_t frame;
    uint32_t start;
    result->name = "1bpp 160x120 x2";
    result->source_bytes = sizeof(g_1bpp_a);
    result->changed_pixels = BADGER_W * BADGER_H * TEST_FRAMES;
    draw_status(result->name, 0, TEST_FRAMES);
    result->cpu_us = 0;
    result->push_us = 0;
    start = timer_us();
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        int y;
        uint32_t t0 = timer_us();
        make_1bpp(g_1bpp_a, LOW_W, LOW_H, frame);
        result->cpu_us += elapsed_us(t0, timer_us());
        for (y = 0; y < LOW_H; ++y) {
            t0 = timer_us();
            pack_1bpp_scaled_row(g_1bpp_a, y, LOW_W);
            result->cpu_us += elapsed_us(t0, timer_us());
            t0 = timer_us();
            push_scaled_row(0, 40, y, LOW_W);
            result->push_us += elapsed_us(t0, timer_us());
        }
    }
    finish_result(result, TEST_FRAMES, elapsed_us(start, timer_us()), BADGER_W * BADGER_H);
}

static void bench_2bpp_low(VideoBenchResult *result) {
    uint32_t frame;
    uint32_t start;
    result->name = "2bpp 160x120 x2";
    result->source_bytes = sizeof(g_2bpp);
    result->changed_pixels = BADGER_W * BADGER_H * TEST_FRAMES;
    draw_status(result->name, 0, TEST_FRAMES);
    result->cpu_us = 0;
    result->push_us = 0;
    start = timer_us();
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        int y;
        uint32_t t0 = timer_us();
        make_2bpp(g_2bpp, LOW_W, LOW_H, frame);
        result->cpu_us += elapsed_us(t0, timer_us());
        for (y = 0; y < LOW_H; ++y) {
            t0 = timer_us();
            pack_2bpp_scaled_row(g_2bpp, y, LOW_W);
            result->cpu_us += elapsed_us(t0, timer_us());
            t0 = timer_us();
            push_scaled_row(0, 40, y, LOW_W);
            result->push_us += elapsed_us(t0, timer_us());
        }
    }
    finish_result(result, TEST_FRAMES, elapsed_us(start, timer_us()), BADGER_W * BADGER_H);
}

static void bench_1bpp_full(VideoBenchResult *result) {
    uint32_t frame;
    uint32_t start;
    result->name = "1bpp 320x240";
    result->source_bytes = sizeof(g_1bpp_320);
    result->changed_pixels = BADGER_W * BADGER_H * TEST_FRAMES;
    draw_status(result->name, 0, TEST_FRAMES);
    result->cpu_us = 0;
    result->push_us = 0;
    start = timer_us();
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        int y;
        uint32_t t0 = timer_us();
        make_1bpp(g_1bpp_320, BADGER_W, BADGER_H, frame);
        result->cpu_us += elapsed_us(t0, timer_us());
        for (y = 0; y < BADGER_H; ++y) {
            t0 = timer_us();
            pack_1bpp_full_row(g_1bpp_320, y);
            result->cpu_us += elapsed_us(t0, timer_us());
            t0 = timer_us();
            push_full_row(y);
            result->push_us += elapsed_us(t0, timer_us());
        }
    }
    finish_result(result, TEST_FRAMES, elapsed_us(start, timer_us()), BADGER_W * BADGER_H);
}

static void bench_tile_delta(VideoBenchResult *result) {
    uint32_t frame;
    uint32_t start;
    uint32_t changed_tiles = 0;
    result->name = "tile delta 1bpp";
    result->source_bytes = sizeof(g_1bpp_a);
    draw_status(result->name, 0, TEST_FRAMES);
    result->cpu_us = 0;
    result->push_us = 0;
    make_1bpp(g_1bpp_a, LOW_W, LOW_H, 0);
    start = timer_us();
    for (frame = 1; frame <= TEST_FRAMES; ++frame) {
        int ty;
        int tx;
        uint32_t t0 = timer_us();
        make_1bpp(g_1bpp_b, LOW_W, LOW_H, frame);
        result->cpu_us += elapsed_us(t0, timer_us());
        for (ty = 0; ty < LOW_H; ty += TILE_H) {
            for (tx = 0; tx < LOW_W; tx += TILE_W) {
                t0 = timer_us();
                if (tile_changed(g_1bpp_a, g_1bpp_b, tx, ty)) {
                    int row;
                    result->cpu_us += elapsed_us(t0, timer_us());
                    for (row = 0; row < TILE_H; ++row) {
                        t0 = timer_us();
                        pack_scaled_tile_row(g_1bpp_b, tx, ty, row);
                        result->cpu_us += elapsed_us(t0, timer_us());
                        t0 = timer_us();
                        push_scaled_tile_row(tx, ty, row);
                        result->push_us += elapsed_us(t0, timer_us());
                    }
                    changed_tiles += 1u;
                } else {
                    result->cpu_us += elapsed_us(t0, timer_us());
                }
            }
        }
        t0 = timer_us();
        for (ty = 0; ty < (int)sizeof(g_1bpp_a); ++ty) g_1bpp_a[ty] = g_1bpp_b[ty];
        result->cpu_us += elapsed_us(t0, timer_us());
    }
    result->changed_pixels = changed_tiles * TILE_W * TILE_H * 4u;
    finish_result(result, TEST_FRAMES, elapsed_us(start, timer_us()), BADGER_W * BADGER_H);
}

static void bench_rle_cpu(VideoBenchResult *result) {
    uint32_t frame;
    uint32_t start;
    uint32_t bytes = 0;
    result->name = "RLE CPU decode";
    draw_status(result->name, 0, TEST_FRAMES);
    result->cpu_us = 0;
    result->push_us = 0;
    start = timer_us();
    for (frame = 0; frame < TEST_FRAMES; ++frame) {
        uint32_t t0 = timer_us();
        uint32_t frame_bytes = make_rle_frame(g_rle, frame);
        bytes += frame_bytes;
        decode_rle_cpu(g_rle, frame_bytes);
        result->cpu_us += elapsed_us(t0, timer_us());
    }
    result->source_bytes = bytes / TEST_FRAMES;
    result->changed_pixels = 0;
    finish_result(result, TEST_FRAMES, elapsed_us(start, timer_us()), LOW_W * LOW_H);
}

static void format_result_line(char *line, int capacity, const VideoBenchResult *result) {
    int pos = 0;
    append_text(line, &pos, capacity, result->name);
    append_text(line, &pos, capacity, " ");
    append_u32(line, &pos, capacity, result->fps10 / 10u);
    append_char(line, &pos, capacity, '.');
    append_u32(line, &pos, capacity, result->fps10 % 10u);
    append_text(line, &pos, capacity, "fps ");
    append_u32(line, &pos, capacity, result->source_bytes);
    append_text(line, &pos, capacity, "B");
    finish_line(line, &pos, capacity);
}

static void append_avg_us(char *line, int *pos, int capacity, const char *label, uint32_t value, uint32_t frames) {
    append_text(line, pos, capacity, label);
    append_u32(line, pos, capacity, value / frames);
    append_text(line, pos, capacity, "us ");
}

static void draw_results(const VideoBenchResult *results, int count) {
    int i;
    picocalc_lcd_clear(0x000000u);
    draw_text_line(0, "BAD APPLE VIDEO BUDGET", 0x00ff80u);
    draw_text_line(1, "PicoCalc LCD: 24-bit SPI path", 0x808080u);
    draw_text_line(2, "CPU includes decode + scale/pack", 0x808080u);
    for (i = 0; i < count; ++i) {
        char line[64];
        format_result_line(line, sizeof(line), results + i);
        draw_text_line(4 + i * 2, line, results[i].fps10 >= 150u ? 0xc0ffc0u : 0xffc080u);
        {
            int pos = 0;
            uint32_t frames = TEST_FRAMES;
            append_avg_us(line, &pos, sizeof(line), "cpu ", results[i].cpu_us, frames);
            append_avg_us(line, &pos, sizeof(line), "lcd ", results[i].push_us, frames);
            append_text(line, &pos, sizeof(line), "chg ");
            append_u32(line, &pos, sizeof(line), results[i].changed_pixels / frames);
            finish_line(line, &pos, sizeof(line));
            draw_text_line(5 + i * 2, line, 0x808080u);
        }
    }
    draw_text_line(18, "15fps is the first pass/fail line", 0xffffffu);
    draw_text_line(20, "Use hardware numbers, emulator is only", 0x80c0ffu);
    draw_text_line(21, "a rendering/smoke sanity check.", 0x80c0ffu);
    g_sink ^= results[0].total_us;
}

#ifdef PICOCALC_BARE_SIM
const char *picocalc_lcd_sim_ppm_path(void) {
    return "build/sim/sim_video_bench.ppm";
}
#endif

void bare_main(void) {
    VideoBenchResult results[6];
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    bench_rgb_full(&results[0]);
    bench_1bpp_low(&results[1]);
    bench_2bpp_low(&results[2]);
    bench_1bpp_full(&results[3]);
    bench_tile_delta(&results[4]);
    bench_rle_cpu(&results[5]);
    draw_results(results, 6);
    while (1) {
    }
}