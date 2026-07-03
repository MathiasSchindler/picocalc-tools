#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#define TIMERAWL REG32(0x40054028u)

static volatile uint32_t g_bench_sink;
static uint8_t g_copy_src[1024];
static uint8_t g_copy_dst[1024];

static uint32_t timer_us(void) {
    return TIMERAWL;
}

static uint32_t elapsed_us(uint32_t start, uint32_t end) {
    uint32_t elapsed = end - start;
    return elapsed == 0u ? 1u : elapsed;
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

static void draw_line(int row, const char *text, uint32_t color) {
    picocalc_lcd_puts_scale(0, row * 14, text, color, 0x000000u, 1);
}

static void format_rate_line(char *buffer, int capacity, const char *name, uint32_t count, uint32_t elapsed) {
    int position = 0;
    uint32_t kops_per_sec = count * 1000u / elapsed;
    append_text(buffer, &position, capacity, name);
    append_text(buffer, &position, capacity, ": ");
    append_u32(buffer, &position, capacity, elapsed);
    append_text(buffer, &position, capacity, " us ");
    append_u32(buffer, &position, capacity, kops_per_sec);
    append_text(buffer, &position, capacity, " k/s");
    finish_line(buffer, &position, capacity);
}

static void format_value_line(char *buffer, int capacity, const char *name, uint32_t elapsed, uint32_t value, const char *unit) {
    int position = 0;
    append_text(buffer, &position, capacity, name);
    append_text(buffer, &position, capacity, ": ");
    append_u32(buffer, &position, capacity, elapsed);
    append_text(buffer, &position, capacity, " us ");
    append_u32(buffer, &position, capacity, value);
    append_char(buffer, &position, capacity, ' ');
    append_text(buffer, &position, capacity, unit);
    finish_line(buffer, &position, capacity);
}

static uint32_t bench_nop(uint32_t count) {
    uint32_t start = timer_us();
    uint32_t loop_index;
    for (loop_index = 0; loop_index < count; ++loop_index) {
        __asm__ volatile ("nop" ::: "memory");
    }
    return elapsed_us(start, timer_us());
}

static uint32_t bench_alu(uint32_t count) {
    uint32_t value = 0x12345678u;
    uint32_t loop_index;
    uint32_t start = timer_us();
    for (loop_index = 0; loop_index < count; ++loop_index) {
        value += loop_index ^ 0x9e3779b9u;
        value = (value << 5) | (value >> 27);
    }
    g_bench_sink ^= value;
    return elapsed_us(start, timer_us());
}

static uint32_t bench_mul(uint32_t count) {
    uint32_t value = 1u;
    uint32_t loop_index;
    uint32_t start = timer_us();
    for (loop_index = 0; loop_index < count; ++loop_index) {
        value = value * 1664525u + 1013904223u;
    }
    g_bench_sink ^= value;
    return elapsed_us(start, timer_us());
}

static uint32_t bench_div(uint32_t count) {
    uint32_t value = 0u;
    uint32_t loop_index;
    uint32_t start = timer_us();
    for (loop_index = 1u; loop_index <= count; ++loop_index) {
        value += 1000000007u / loop_index;
    }
    g_bench_sink ^= value;
    return elapsed_us(start, timer_us());
}

static uint32_t bench_memcopy(uint32_t repeats) {
    uint32_t repeat_index;
    uint32_t byte_index;
    uint32_t start;
    for (byte_index = 0; byte_index < sizeof(g_copy_src); ++byte_index) {
        g_copy_src[byte_index] = (uint8_t)(byte_index * 37u + 11u);
    }
    start = timer_us();
    for (repeat_index = 0; repeat_index < repeats; ++repeat_index) {
        for (byte_index = 0; byte_index < sizeof(g_copy_src); ++byte_index) {
            g_copy_dst[byte_index] = g_copy_src[byte_index];
        }
    }
    g_bench_sink ^= g_copy_dst[repeats & 1023u];
    return elapsed_us(start, timer_us());
}

static uint32_t bench_timer_read(uint32_t count) {
    uint32_t loop_index;
    uint32_t value = 0u;
    uint32_t start = timer_us();
    for (loop_index = 0; loop_index < count; ++loop_index) {
        value ^= timer_us();
    }
    g_bench_sink ^= value;
    return elapsed_us(start, timer_us());
}

static uint32_t bench_lcd_fill(uint32_t repeats) {
    uint32_t repeat_index;
    uint32_t start = timer_us();
    for (repeat_index = 0; repeat_index < repeats; ++repeat_index) {
        picocalc_lcd_fill_rect(0, 294, 319, 313, repeat_index & 1u ? 0x202020u : 0x004040u);
    }
    return elapsed_us(start, timer_us());
}

void bare_main(void) {
    char nop_line[48];
    char alu_line[48];
    char mul_line[48];
    char div_line[48];
    char copy_line[48];
    char timer_line[48];
    char lcd_line[48];
    char sink_line[48];
    uint32_t elapsed;
    const uint32_t loop_count = 100000u;
    const uint32_t div_count = 12000u;
    const uint32_t copy_repeats = 256u;
    const uint32_t timer_reads = 50000u;
    const uint32_t lcd_repeats = 3u;

    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);

    elapsed = bench_nop(loop_count);
    format_rate_line(nop_line, sizeof(nop_line), "nop", loop_count, elapsed);

    elapsed = bench_alu(loop_count);
    format_rate_line(alu_line, sizeof(alu_line), "alu32", loop_count, elapsed);

    elapsed = bench_mul(loop_count);
    format_rate_line(mul_line, sizeof(mul_line), "mul32", loop_count, elapsed);

    elapsed = bench_div(div_count);
    format_rate_line(div_line, sizeof(div_line), "udiv32", div_count, elapsed);

    elapsed = bench_memcopy(copy_repeats);
    format_value_line(copy_line, sizeof(copy_line), "copy256K", elapsed, (copy_repeats * 1024u * 1000u) / elapsed, "KB/s");

    elapsed = bench_timer_read(timer_reads);
    format_rate_line(timer_line, sizeof(timer_line), "timer rd", timer_reads, elapsed);

    elapsed = bench_lcd_fill(lcd_repeats);
    format_value_line(lcd_line, sizeof(lcd_line), "lcd 19200px", elapsed, (lcd_repeats * 320u * 20u * 1000u) / elapsed, "kpix/s");
    format_value_line(sink_line, sizeof(sink_line), "sink", 0u, g_bench_sink, "");

    picocalc_lcd_clear(0x000000u);
    draw_line(0, "PicoCalc RP2040 bench", 0xffffffu);
    draw_line(1, "timer: TIMERAWL us", 0x80c0ffu);
    draw_line(3, "nop/alu/mul/div are loops", 0x808080u);
    draw_line(4, "copy uses 1024B buffer", 0x808080u);
    draw_line(5, lcd_line, 0xc0ffc0u);
    draw_line(7, nop_line, 0xc0ffc0u);
    draw_line(8, alu_line, 0xc0ffc0u);
    draw_line(9, mul_line, 0xc0ffc0u);
    draw_line(10, div_line, 0xc0ffc0u);
    draw_line(11, copy_line, 0xc0ffc0u);
    draw_line(12, timer_line, 0xc0ffc0u);
    draw_line(14, sink_line, 0x808080u);
    draw_line(16, "copy this whole screen", 0xffffffu);

    while (1) {
    }
}
