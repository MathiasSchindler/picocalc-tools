#include "picocalc_lcd_bare.h"
#include "picocalc_font.h"
#include "linux_sys.h"

#define SIM_COLS 80
#define SIM_ROWS 40
#define LCD_WIDTH 320
#define LCD_HEIGHT 320

#define AT_FDCWD (-100)
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512

static char g_screen[SIM_ROWS][SIM_COLS];
static unsigned char g_framebuffer[LCD_WIDTH * LCD_HEIGHT * 3];

static void sim_set_pixel(int x, int y, unsigned int rgb) {
    size_t offset;
    if (x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    offset = ((size_t)y * LCD_WIDTH + (size_t)x) * 3U;
    g_framebuffer[offset + 0U] = (unsigned char)(rgb >> 16);
    g_framebuffer[offset + 1U] = (unsigned char)(rgb >> 8);
    g_framebuffer[offset + 2U] = (unsigned char)rgb;
}

static void sim_fill_rect(int x1, int y1, int x2, int y2, unsigned int rgb) {
    int x;
    int y;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LCD_WIDTH) x2 = LCD_WIDTH - 1;
    if (y2 >= LCD_HEIGHT) y2 = LCD_HEIGHT - 1;
    if (x2 < x1 || y2 < y1) return;
    for (y = y1; y <= y2; ++y) {
        for (x = x1; x <= x2; ++x) {
            sim_set_pixel(x, y, rgb);
        }
    }
}

static unsigned int sim_blend_channel(unsigned int fg, unsigned int bg, unsigned int alpha) {
    return (bg * (15U - alpha) + fg * alpha + 7U) / 15U;
}

static unsigned int sim_blend_color(unsigned int fg, unsigned int bg, unsigned int alpha) {
    unsigned int r;
    unsigned int g;
    unsigned int b;
    if (alpha == 0U) return bg;
    if (alpha >= 15U) return fg;
    r = sim_blend_channel((fg >> 16) & 255U, (bg >> 16) & 255U, alpha);
    g = sim_blend_channel((fg >> 8) & 255U, (bg >> 8) & 255U, alpha);
    b = sim_blend_channel(fg & 255U, bg & 255U, alpha);
    return (r << 16) | (g << 8) | b;
}

static void sim_draw_char(int x, int y, char ch, unsigned int fg, unsigned int bg, int scale) {
    const unsigned char *glyph = picocalc_font_glyph(ch);
    int row;
    int col;
    if (scale < 1) scale = 1;
    sim_fill_rect(x, y, x + PICOCALC_FONT_CELL_W * scale - 1, y + PICOCALC_FONT_CELL_H * scale - 1, bg);
    for (row = 0; row < PICOCALC_FONT_CELL_H; ++row) {
        for (col = 0; col < PICOCALC_FONT_CELL_W; ++col) {
            unsigned int alpha = picocalc_font_alpha(glyph, row, col);
            if (alpha != 0U) {
                int px = x + col * scale;
                int py = y + row * scale;
                sim_fill_rect(px, py, px + scale - 1, py + scale - 1, sim_blend_color(fg, bg, alpha));
            }
        }
    }
}

static void sim_clear_buffer(void) {
    int row;
    int col;
    for (row = 0; row < SIM_ROWS; ++row) {
        for (col = 0; col < SIM_COLS; ++col) {
            g_screen[row][col] = ' ';
        }
    }
}

void picocalc_lcd_init(void) {
    sim_clear_buffer();
}

void picocalc_lcd_clear(unsigned int rgb) {
    sim_clear_buffer();
    sim_fill_rect(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, rgb);
}

void picocalc_lcd_fill_rect(int x1, int y1, int x2, int y2, unsigned int rgb) {
    sim_fill_rect(x1, y1, x2, y2, rgb);
}

void picocalc_lcd_puts(int x, int y, const char *text, unsigned int fg, unsigned int bg) {
    picocalc_lcd_puts_scale(x, y, text, fg, bg, 3);
}

void picocalc_lcd_puts_scale(int x, int y, const char *text, unsigned int fg, unsigned int bg, int scale) {
    int row;
    int col;
    (void)fg;
    (void)bg;
    if (scale < 1) scale = 1;
    row = y / (PICOCALC_FONT_CELL_H * scale);
    col = x / (PICOCALC_FONT_CELL_W * scale);
    while (*text != '\0') {
        if (row >= 0 && row < SIM_ROWS && col >= 0 && col < SIM_COLS) {
            g_screen[row][col] = *text;
        }
        sim_draw_char(x, y, *text, fg, bg, scale);
        col += 1;
        x += PICOCALC_FONT_CELL_W * scale;
        text += 1;
    }
}

void picocalc_lcd_sim_flush(void) {
    int row;
    for (row = 0; row < SIM_ROWS; ++row) {
        int end = SIM_COLS;
        while (end > 0 && g_screen[row][end - 1] == ' ') {
            end -= 1;
        }
        if (end > 0) {
            char newline = '\n';
            (void)sim_linux_write(1, g_screen[row], (size_t)end);
            (void)sim_linux_write(1, &newline, 1U);
        }
    }
}

void picocalc_lcd_sim_write_ppm(const char *path) {
    static const char header[] = "P6\n320 320\n255\n";
    long fd = sim_linux_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)sim_linux_write((int)fd, header, sizeof(header) - 1U);
    (void)sim_linux_write((int)fd, g_framebuffer, sizeof(g_framebuffer));
    (void)sim_linux_close((int)fd);
}