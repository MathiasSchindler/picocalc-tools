#include "picocalc_lcd_bare.h"
#include "picocalc_font.h"
#include "rp2040_regs.h"

#ifdef PICOCALC_SDK_FLASH
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#endif

#define LCD_SCK 10
#define LCD_TX  11
#define LCD_RX  12
#define LCD_CS  13
#define LCD_DC  14
#define LCD_RST 15

#define LCD_WIDTH  320
#define LCD_HEIGHT 320

#define CMD_COLADDR 0x2au
#define CMD_PAGEADDR 0x2bu
#define CMD_MEMWRITE 0x2cu
#define CMD_MADCTL 0x36u
#define CMD_SLPOUT 0x11u
#define CMD_DISPON 0x29u
#define CMD_INVON 0x21u

#ifdef PICOCALC_SDK_FLASH
#define LCD_SPI spi1
#endif

static void delay_ms(unsigned int ms) {
#ifdef PICOCALC_SDK_FLASH
    sleep_ms(ms);
#else
    while (ms-- != 0u) {
        reg_wait_cycles(60000u);
    }
#endif
}

static void spi_finish(void) {
#ifdef PICOCALC_SDK_FLASH
    while (spi_is_busy(LCD_SPI)) {
        tight_loop_contents();
    }
#else
    while ((SPI_SSPSR & SPI_SR_TFE) == 0u) {
    }
    while ((SPI_SSPSR & SPI_SR_BSY) != 0u) {
    }
    while ((SPI_SSPSR & SPI_SR_RNE) != 0u) {
        (void)SPI_SSPDR;
    }
    SPI_SSPICR = 3u;
#endif
}

static void spi_write_byte(uint8_t byte) {
#ifdef PICOCALC_SDK_FLASH
    (void)spi_write_blocking(LCD_SPI, &byte, 1);
#else
    while ((SPI_SSPSR & SPI_SR_TNF) == 0u) {
    }
    SPI_SSPDR = byte;
#endif
}

static void spi_write(const uint8_t *data, size_t count) {
#ifdef PICOCALC_SDK_FLASH
    (void)spi_write_blocking(LCD_SPI, data, count);
#else
    while (count-- != 0u) {
        spi_write_byte(*data++);
    }
#endif
}

static void lcd_command(uint8_t command) {
    gpio_put(LCD_DC, 0);
    gpio_put(LCD_CS, 0);
    spi_write_byte(command);
    spi_finish();
    gpio_put(LCD_CS, 1);
}

static void lcd_data_many(const uint8_t *data, size_t count) {
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    spi_write(data, count);
    spi_finish();
    gpio_put(LCD_CS, 1);
}

static void lcd_command_data(uint8_t command, const uint8_t *data, size_t count) {
    lcd_command(command);
    lcd_data_many(data, count);
}

static void lcd_set_window(int x1, int y1, int x2, int y2) {
    uint8_t coords[4];
    coords[0] = (uint8_t)(x1 >> 8);
    coords[1] = (uint8_t)x1;
    coords[2] = (uint8_t)(x2 >> 8);
    coords[3] = (uint8_t)x2;
    lcd_command_data(CMD_COLADDR, coords, sizeof(coords));
    coords[0] = (uint8_t)(y1 >> 8);
    coords[1] = (uint8_t)y1;
    coords[2] = (uint8_t)(y2 >> 8);
    coords[3] = (uint8_t)y2;
    lcd_command_data(CMD_PAGEADDR, coords, sizeof(coords));
    lcd_command(CMD_MEMWRITE);
}

static void lcd_fill_rect(int x1, int y1, int x2, int y2, unsigned int rgb) {
    uint8_t line[LCD_WIDTH * 3];
    uint8_t r = (uint8_t)(rgb >> 16);
    uint8_t g = (uint8_t)(rgb >> 8);
    uint8_t b = (uint8_t)rgb;
    int width;
    int y;
    int i;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
    if (x2 < x1 || y2 < y1) return;

    width = x2 - x1 + 1;
    for (i = 0; i < width; ++i) {
        line[i * 3 + 0] = r;
        line[i * 3 + 1] = g;
        line[i * 3 + 2] = b;
    }

    lcd_set_window(x1, y1, x2, y2);
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    for (y = y1; y <= y2; ++y) {
        spi_write(line, (size_t)width * 3u);
    }
    spi_finish();
    gpio_put(LCD_CS, 1);
}

static void lcd_reset_panel(void) {
    gpio_put(LCD_RST, 1);
    delay_ms(10);
    gpio_put(LCD_RST, 0);
    delay_ms(10);
    gpio_put(LCD_RST, 1);
    delay_ms(200);
}

static void lcd_init_panel(void) {
    static const uint8_t gamma_pos[] = {0x00, 0x03, 0x09, 0x08, 0x16, 0x0a, 0x3f, 0x78, 0x4c, 0x09, 0x0a, 0x08, 0x16, 0x1a, 0x0f};
    static const uint8_t gamma_neg[] = {0x00, 0x16, 0x19, 0x03, 0x0f, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0e, 0x0d, 0x35, 0x37, 0x0f};
    static const uint8_t power1[] = {0x17, 0x15};
    static const uint8_t vcom[] = {0x00, 0x12, 0x80};
    static const uint8_t display_function[] = {0x02, 0x02, 0x3b};
    static const uint8_t adjust3[] = {0xa9, 0x51, 0x2c, 0x82};
    static const uint8_t madctl[] = {0x48};
    static const uint8_t pixfmt[] = {0x66};
    static const uint8_t zero[] = {0x00};
    static const uint8_t frame_rate[] = {0xa0};
    static const uint8_t inversion[] = {0x02};
    static const uint8_t entry_mode[] = {0xc6};
    static const uint8_t power2[] = {0x41};

    lcd_reset_panel();
    lcd_command_data(0xe0, gamma_pos, sizeof(gamma_pos));
    lcd_command_data(0xe1, gamma_neg, sizeof(gamma_neg));
    lcd_command_data(0xc0, power1, sizeof(power1));
    lcd_command_data(0xc1, power2, sizeof(power2));
    lcd_command_data(0xc5, vcom, sizeof(vcom));
    lcd_command_data(CMD_MADCTL, madctl, sizeof(madctl));
    lcd_command_data(0x3a, pixfmt, sizeof(pixfmt));
    lcd_command_data(0xb0, zero, sizeof(zero));
    lcd_command_data(0xb1, frame_rate, sizeof(frame_rate));
    lcd_command(CMD_INVON);
    lcd_command_data(0xb4, inversion, sizeof(inversion));
    lcd_command_data(0xb6, display_function, sizeof(display_function));
    lcd_command_data(0xb7, entry_mode, sizeof(entry_mode));
    lcd_command_data(0xe9, zero, sizeof(zero));
    lcd_command_data(0xf7, adjust3, sizeof(adjust3));
    lcd_command(CMD_SLPOUT);
    delay_ms(120);
    lcd_command(CMD_DISPON);
    delay_ms(120);
}

void picocalc_lcd_init(void) {
#ifdef PICOCALC_SDK_FLASH
    spi_init(LCD_SPI, 24000000u);
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_TX, GPIO_FUNC_SPI);
    gpio_set_function(LCD_RX, GPIO_FUNC_SPI);
    gpio_init(LCD_CS);
    gpio_init(LCD_DC);
    gpio_init(LCD_RST);
    gpio_set_dir(LCD_CS, GPIO_OUT);
    gpio_set_dir(LCD_DC, GPIO_OUT);
    gpio_set_dir(LCD_RST, GPIO_OUT);
    gpio_put(LCD_CS, 1);
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_RST, 1);
#else
    reset_unreset(RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_SPI1);

    gpio_pad_default(LCD_SCK);
    gpio_pad_default(LCD_TX);
    gpio_pad_default(LCD_RX);
    gpio_pad_default(LCD_CS);
    gpio_pad_default(LCD_DC);
    gpio_pad_default(LCD_RST);

    gpio_put(LCD_CS, 1);
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_RST, 1);
    gpio_out_enable(LCD_CS);
    gpio_out_enable(LCD_DC);
    gpio_out_enable(LCD_RST);

    gpio_set_function(LCD_CS, GPIO_FUNC_SIO);
    gpio_set_function(LCD_DC, GPIO_FUNC_SIO);
    gpio_set_function(LCD_RST, GPIO_FUNC_SIO);
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_TX, GPIO_FUNC_SPI);
    gpio_set_function(LCD_RX, GPIO_FUNC_SPI);
    gpio_out_disable(LCD_RX);

    SPI_SSPCR1 = 0;
    SPI_SSPCPSR = 6u;
    SPI_SSPCR0 = 7u;
    SPI_SSPCR1 = 2u;
#endif

    lcd_init_panel();
}

void picocalc_lcd_clear(unsigned int rgb) {
    lcd_fill_rect(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, rgb);
}

void picocalc_lcd_fill_rect(int x1, int y1, int x2, int y2, unsigned int rgb) {
    lcd_fill_rect(x1, y1, x2, y2, rgb);
}

void picocalc_lcd_blit_rgb(int x, int y, int width, int height, const unsigned char *rgb) {
    if (width <= 0 || height <= 0) return;
    if (x < 0 || y < 0 || x + width > LCD_WIDTH || y + height > LCD_HEIGHT) return;
    lcd_set_window(x, y, x + width - 1, y + height - 1);
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    spi_write(rgb, (size_t)width * (size_t)height * 3u);
    spi_finish();
    gpio_put(LCD_CS, 1);
}

static unsigned int blend_channel(unsigned int fg, unsigned int bg, unsigned int alpha) {
    return (bg * (15u - alpha) + fg * alpha + 7u) / 15u;
}

static unsigned int blend_color(unsigned int fg, unsigned int bg, unsigned int alpha) {
    unsigned int r;
    unsigned int g;
    unsigned int b;
    if (alpha == 0u) return bg;
    if (alpha >= 15u) return fg;
    r = blend_channel((fg >> 16) & 255u, (bg >> 16) & 255u, alpha);
    g = blend_channel((fg >> 8) & 255u, (bg >> 8) & 255u, alpha);
    b = blend_channel(fg & 255u, bg & 255u, alpha);
    return (r << 16) | (g << 8) | b;
}

static void lcd_draw_char(int x, int y, char ch, unsigned int fg, unsigned int bg, int scale) {
    const unsigned char *glyph = picocalc_font_glyph(ch);
    uint8_t line[LCD_WIDTH * 3];
    int cell_w;
    int cell_h;
    int x1;
    int y1;
    int x2;
    int y2;
    int py;
    if (scale < 1) scale = 1;
    cell_w = PICOCALC_FONT_CELL_W * scale;
    cell_h = PICOCALC_FONT_CELL_H * scale;
    x1 = x;
    y1 = y;
    x2 = x + cell_w - 1;
    y2 = y + cell_h - 1;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
    if (x2 < x1 || y2 < y1) return;

    lcd_set_window(x1, y1, x2, y2);
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    for (py = y1; py <= y2; ++py) {
        int row = (py - y) / scale;
        int px;
        int out = 0;
        for (px = x1; px <= x2; ++px) {
            int col = (px - x) / scale;
            unsigned int alpha = picocalc_font_alpha(glyph, row, col);
            unsigned int rgb = blend_color(fg, bg, alpha);
            line[out++] = (uint8_t)(rgb >> 16);
            line[out++] = (uint8_t)(rgb >> 8);
            line[out++] = (uint8_t)rgb;
        }
        spi_write(line, (size_t)out);
    }
    spi_finish();
    gpio_put(LCD_CS, 1);
}

void picocalc_lcd_puts(int x, int y, const char *text, unsigned int fg, unsigned int bg) {
    picocalc_lcd_puts_scale(x, y, text, fg, bg, 3);
}

void picocalc_lcd_puts_scale(int x, int y, const char *text, unsigned int fg, unsigned int bg, int scale) {
    if (scale < 1) scale = 1;
    while (*text != 0) {
        lcd_draw_char(x, y, *text++, fg, bg, scale);
        x += PICOCALC_FONT_CELL_W * scale;
    }
}