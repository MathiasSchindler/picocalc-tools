#include "picocalc_lcd_bare.h"

#define LCD_W 320
#define LCD_H 320

static unsigned int rgb(unsigned int r, unsigned int g, unsigned int b) {
    return ((r & 255U) << 16) | ((g & 255U) << 8) | (b & 255U);
}

static unsigned int ramp_color(int x) {
    unsigned int t = (unsigned int)(x & 255);
    if (x < 64) return rgb(255U, t * 4U, 0U);
    if (x < 128) return rgb(255U - (t - 64U) * 4U, 255U, 0U);
    if (x < 192) return rgb(0U, 255U - (t - 128U) * 4U, (t - 128U) * 4U);
    return rgb((t - 192U) * 4U, 0U, 255U);
}

#ifdef PICOCALC_BARE_SIM
const char *picocalc_lcd_sim_ppm_path(void) {
    return "build/sim/sim_graphics.ppm";
}
#else
static void demo_delay(void) {
    volatile unsigned int i;
    for (i = 0U; i < 180000U; ++i) {
    }
}
#endif

static void draw_color_bars(void) {
    static const unsigned int colors[8] = {
        0xff0000U, 0x00ff00U, 0x0000ffU, 0xffff00U,
        0x00ffffU, 0xff00ffU, 0xffffffU, 0x202020U
    };
    int i;
    for (i = 0; i < 8; ++i) {
        picocalc_lcd_fill_rect(i * 40, 22, i * 40 + 39, 57, colors[i]);
    }
}

static void draw_gradient(void) {
    int x;
    for (x = 0; x < LCD_W; x += 4) {
        unsigned int shade = (unsigned int)(x * 255 / (LCD_W - 1));
        picocalc_lcd_fill_rect(x, 68, x + 3, 95, rgb(shade, shade, shade));
        picocalc_lcd_fill_rect(x, 96, x + 3, 127, ramp_color(x * 255 / (LCD_W - 1)));
    }
}

static void draw_checkerboard(void) {
    int y;
    int x;
    for (y = 140; y < 224; y += 14) {
        for (x = 8; x < 316; x += 14) {
            unsigned int on = (((x / 14) ^ (y / 14)) & 1) != 0;
            picocalc_lcd_fill_rect(x, y, x + 13, y + 13, on ? 0x303030U : 0x00a060U);
        }
    }
}

static void draw_clipping_tests(void) {
    picocalc_lcd_fill_rect(-12, -12, 10, 10, 0xff8000U);
    picocalc_lcd_fill_rect(309, -12, 331, 10, 0x0080ffU);
    picocalc_lcd_fill_rect(-12, 309, 10, 331, 0xff0080U);
    picocalc_lcd_fill_rect(309, 309, 331, 331, 0x80ff00U);
}

static void draw_motion_trace(void) {
    int i;
    for (i = 0; i < 12; ++i) {
        int x = 12 + i * 24;
        unsigned int color = ramp_color(i * 21);
        picocalc_lcd_fill_rect(x, 250, x + 18, 268, color);
    }
}

static void animate_box(void) {
#ifndef PICOCALC_BARE_SIM
    int x = 12;
    int dx = 4;
    picocalc_lcd_fill_rect(x, 286, x + 24, 308, 0xffffffU);
    while (1) {
        int next_x = x + dx;
        if (next_x <= 12 || next_x >= 284) {
            dx = -dx;
            next_x = x + dx;
        }
        if (next_x > x) {
            picocalc_lcd_fill_rect(x + 25, 286, next_x + 24, 308, 0xffffffU);
            picocalc_lcd_fill_rect(x, 286, next_x - 1, 308, 0x000000U);
        } else {
            picocalc_lcd_fill_rect(next_x, 286, x - 1, 308, 0xffffffU);
            picocalc_lcd_fill_rect(next_x + 25, 286, x + 24, 308, 0x000000U);
        }
        x = next_x;
        demo_delay();
    }
#endif
}

static void draw_animation_lane(void) {
    picocalc_lcd_fill_rect(12, 286, 308, 308, 0x000000U);
    picocalc_lcd_fill_rect(12, 284, 308, 285, 0x303030U);
    picocalc_lcd_fill_rect(12, 309, 308, 310, 0x303030U);
}

void bare_main(void) {
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000U);
    draw_clipping_tests();
    picocalc_lcd_puts_scale(24, 4, "GRAPHICS DEMO", 0x00ff00U, 0x000000U, 1);
    draw_color_bars();
    picocalc_lcd_puts_scale(4, 60, "GRAY + RGB RAMPS", 0xffffffU, 0x000000U, 1);
    draw_gradient();
    picocalc_lcd_puts_scale(4, 130, "CHECKER + CLIP", 0xffffffU, 0x000000U, 1);
    draw_checkerboard();
    draw_clipping_tests();
    picocalc_lcd_puts_scale(4, 236, "MOTION TRACE", 0xffffffU, 0x000000U, 1);
    draw_motion_trace();
    picocalc_lcd_puts_scale(4, 276, "ANIMATION ON HARDWARE", 0xffffffU, 0x000000U, 1);
    draw_animation_lane();
    animate_box();
}