#ifndef PICOCALC_LCD_BARE_H
#define PICOCALC_LCD_BARE_H

void picocalc_lcd_init(void);
void picocalc_lcd_clear(unsigned int rgb);
void picocalc_lcd_fill_rect(int x1, int y1, int x2, int y2, unsigned int rgb);
void picocalc_lcd_blit_rgb(int x, int y, int width, int height, const unsigned char *rgb);
void picocalc_lcd_puts(int x, int y, const char *text, unsigned int fg, unsigned int bg);
void picocalc_lcd_puts_scale(int x, int y, const char *text, unsigned int fg, unsigned int bg, int scale);

#endif