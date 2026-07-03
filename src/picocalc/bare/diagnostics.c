#include "picocalc_kbd_bare.h"
#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#define APP_BASE 0x10032000u
#define TIMERAWL REG32(0x40054028u)
#define RTC_CTRL REG32(0x4005c00cu)
#define I2C_IC_ENABLE_STATUS REG32(I2C1_BASE + 0x78u)
#define XIP_SSI_SR REG32(0x18000028u)
#define SIO_DIV_UDIVIDEND REG32(0xd0000060u)
#define SIO_DIV_UDIVISOR REG32(0xd0000064u)
#define SIO_DIV_QUOTIENT REG32(0xd0000070u)
#define SIO_DIV_REMAINDER REG32(0xd0000074u)

#define ROM_CODE(a, b) ((uint32_t)(a) | ((uint32_t)(b) << 8))

typedef uint32_t (*rom_lookup_fn)(uint32_t table, uint32_t code);
typedef uint32_t (*rom_word_fn)(uint32_t value);

static uint32_t read_halfword(uint32_t addr) {
    uint32_t value;
    __asm__ volatile ("ldrh %0, [%1]" : "=r"(value) : "r"(addr) : "memory");
    return value;
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

static void append_hex_nibble(char *buffer, int *position, int capacity, uint32_t value) {
    value &= 15u;
    append_char(buffer, position, capacity, (char)(value < 10u ? '0' + value : 'a' + value - 10u));
}

static void append_hex32(char *buffer, int *position, int capacity, uint32_t value) {
    int shift;
    append_text(buffer, position, capacity, "0x");
    for (shift = 28; shift >= 0; shift -= 4) append_hex_nibble(buffer, position, capacity, value >> shift);
}

static void finish_line(char *buffer, int *position, int capacity) {
    buffer[*position < capacity ? *position : capacity - 1] = 0;
}

static void draw_line(int row, const char *text, uint32_t color) {
    picocalc_lcd_puts_scale(0, row * 14, text, color, 0x000000u, 1);
}

static void format_bool_line(char *buffer, int capacity, const char *name, int ok, const char *detail) {
    int position = 0;
    append_text(buffer, &position, capacity, name);
    append_text(buffer, &position, capacity, ok ? ": OK " : ": BAD ");
    append_text(buffer, &position, capacity, detail);
    finish_line(buffer, &position, capacity);
}

static void format_hex_line(char *buffer, int capacity, const char *name, uint32_t value, int ok) {
    int position = 0;
    append_text(buffer, &position, capacity, name);
    append_text(buffer, &position, capacity, ok ? ": OK " : ": BAD ");
    append_hex32(buffer, &position, capacity, value);
    finish_line(buffer, &position, capacity);
}

static void format_timer_line(char *buffer, int capacity, uint32_t delta) {
    int position = 0;
    append_text(buffer, &position, capacity, "timer: ");
    append_text(buffer, &position, capacity, delta != 0u ? "OK +" : "BAD +");
    append_u32(buffer, &position, capacity, delta);
    append_text(buffer, &position, capacity, " us");
    finish_line(buffer, &position, capacity);
}

static int bootrom_probe(void) {
    uint32_t func_table = read_halfword(0x14u);
    rom_lookup_fn lookup = (rom_lookup_fn)(read_halfword(0x18u) | 1u);
    rom_word_fn popcount;
    if (func_table == 0u || (uint32_t)lookup == 1u) return 0;
    popcount = (rom_word_fn)(lookup(func_table, ROM_CODE('P', '3')) | 1u);
    if ((uint32_t)popcount == 1u) return 0;
    return popcount(0x80000007u) == 4u;
}

static int divider_probe(void) {
    SIO_DIV_UDIVIDEND = 1000u;
    SIO_DIV_UDIVISOR = 7u;
    return SIO_DIV_QUOTIENT == 142u && SIO_DIV_REMAINDER == 6u;
}

static int rtc_probe(void) {
    RTC_CTRL = 0u;
    if ((RTC_CTRL & (1u << 1)) != 0u) return 0;
    RTC_CTRL = 1u;
    return (RTC_CTRL & (1u << 1)) != 0u;
}

static int wait_i2c_enabled(void) {
    uint32_t timeout = 200000u;
    while ((I2C_IC_ENABLE_STATUS & 1u) == 0u) {
        if (timeout-- == 0u) return 0;
    }
    return 1;
}

static int keyboard_probe(int *out_key) {
    int key;
    int enabled;
    picocalc_kbd_init();
    enabled = wait_i2c_enabled();
    key = picocalc_kbd_read_key();
    *out_key = key;
    if ((I2C_IC_TX_ABRT & 0xffffu) != 0u) return 0;
    if ((I2C_IC_ENABLE_STATUS & 1u) == 0u) {
        I2C_IC_ENABLE = 1u;
        enabled = wait_i2c_enabled();
    }
    return enabled;
}

void bare_main(void) {
    char line[48];
    uint32_t timer_start;
    uint32_t timer_delta;
    uint32_t vector_sp;
    uint32_t vector_reset;
    uint32_t reset_done;
    uint32_t spi_status;
    uint32_t xip_status;
    int key = -1;
    int keyboard_ok;
    int position;

    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);

    timer_start = TIMERAWL;
    reg_wait_cycles(10000u);
    timer_delta = TIMERAWL - timer_start;

    vector_sp = REG32(APP_BASE + 0u);
    vector_reset = REG32(APP_BASE + 4u);
    reset_done = RESETS_DONE;
    spi_status = SPI_SSPSR;
    xip_status = XIP_SSI_SR;
    keyboard_ok = keyboard_probe(&key);

    draw_line(0, "PicoCalc RP2040 diag", 0xffffffu);
    draw_line(1, "LCD: OK if visible", 0xc0ffc0u);

    format_timer_line(line, sizeof(line), timer_delta);
    draw_line(3, line, timer_delta != 0u ? 0xc0ffc0u : 0xff8080u);

    format_bool_line(line, sizeof(line), "vector", vector_sp == 0x20042000u && (vector_reset & 1u) != 0u, "SD app header");
    draw_line(4, line, vector_sp == 0x20042000u && (vector_reset & 1u) != 0u ? 0xc0ffc0u : 0xff8080u);

    format_hex_line(line, sizeof(line), "reset done", reset_done, (reset_done & (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_SPI1 | RESET_I2C1)) == (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_SPI1 | RESET_I2C1));
    draw_line(5, line, 0xc0ffc0u);

    format_hex_line(line, sizeof(line), "spi1 stat", spi_status, (spi_status & (SPI_SR_TFE | SPI_SR_TNF)) == (SPI_SR_TFE | SPI_SR_TNF));
    draw_line(6, line, 0xc0ffc0u);

    format_bool_line(line, sizeof(line), "i2c1", (I2C_IC_ENABLE_STATUS & 1u) != 0u, "enabled");
    draw_line(7, line, (I2C_IC_ENABLE_STATUS & 1u) != 0u ? 0xc0ffc0u : 0xff8080u);

    position = 0;
    append_text(line, &position, sizeof(line), keyboard_ok ? "kbd: OK " : "kbd: BAD ");
    if (key >= 0) {
        append_text(line, &position, sizeof(line), "key ");
        append_hex32(line, &position, sizeof(line), (uint32_t)key);
    } else {
        append_text(line, &position, sizeof(line), "no key");
    }
    finish_line(line, &position, sizeof(line));
    draw_line(8, line, keyboard_ok ? 0xc0ffc0u : 0xff8080u);

    format_bool_line(line, sizeof(line), "bootrom", bootrom_probe(), "lookup P3");
    draw_line(9, line, 0xc0ffc0u);

    format_bool_line(line, sizeof(line), "sio div", divider_probe(), "1000/7");
    draw_line(10, line, 0xc0ffc0u);

    format_bool_line(line, sizeof(line), "rtc", rtc_probe(), "active bit");
    draw_line(11, line, 0xc0ffc0u);

    format_hex_line(line, sizeof(line), "xip ssi", xip_status, (xip_status & 0x0fu) != 0u);
    draw_line(12, line, 0xc0ffc0u);

    draw_line(14, "limits: no SD/audio/USB test", 0x808080u);
    draw_line(15, "keyboard shows key if held", 0x808080u);
    draw_line(17, "copy this whole screen", 0xffffffu);

    while (1) {
    }
}
