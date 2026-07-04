#include "emu_lcd.h"
#include "emu_trace.h"

#define LCD_CS_PIN             13u
#define LCD_DC_PIN             14u

static void lcd_set_pixel(EmuState *emu, int x, int y, u8 r, u8 g, u8 b) {
    usize off;
    u32 rgb;
    if (x < 0 || y < 0 || x >= LCD_W || y >= LCD_H) return;
    rgb = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
    if (!emu->seen_lcd_pixel) {
        emu->seen_lcd_pixel = 1;
        emu->first_lcd_pixel_x = (u32)x;
        emu->first_lcd_pixel_y = (u32)y;
        emu->first_lcd_pixel_rgb = rgb;
        emu->first_lcd_pixel_pc = emu->current_pc;
        emu->first_lcd_pixel_cycles = emu->cycles;
    }
    if (rgb != 0u && !emu->seen_lcd_nonblack) {
        emu->seen_lcd_nonblack = 1;
        emu->first_lcd_nonblack_x = (u32)x;
        emu->first_lcd_nonblack_y = (u32)y;
        emu->first_lcd_nonblack_rgb = rgb;
        emu->first_lcd_nonblack_pc = emu->current_pc;
        emu->first_lcd_nonblack_cycles = emu->cycles;
    }
    off = ((usize)y * LCD_W + (usize)x) * 3u;
    emu->framebuffer[off] = r;
    emu->framebuffer[off + 1u] = g;
    emu->framebuffer[off + 2u] = b;
}

static int param_be16(EmuState *emu, int off) {
    return ((int)emu->lcd.params[off] << 8) | (int)emu->lcd.params[off + 1];
}

static int lcd_expected_params(u8 command) {
    if (command == 0x2au || command == 0x2bu) return 4;
    if (command == 0x36u || command == 0x3au) return 1;
    if (command == 0x33u) return 6;
    if (command == 0x37u) return 2;
    if (command == 0xb1u || command == 0xc0u) return 2;
    if (command == 0xb6u || command == 0xc5u) return 3;
    if (command == 0xf7u) return 4;
    if (command == 0xb4u || command == 0xb7u || command == 0xc1u || command == 0xc7u) return 1;
    if (command == 0xe0u || command == 0xe1u) return 15;
    return 0;
}

static void lcd_apply_params(EmuState *emu) {
    if (emu->lcd.command == 0x2au) {
        emu->lcd.x0 = param_be16(emu, 0);
        emu->lcd.x1 = param_be16(emu, 2);
    } else if (emu->lcd.command == 0x2bu) {
        emu->lcd.y0 = param_be16(emu, 0);
        emu->lcd.y1 = param_be16(emu, 2);
    } else if (emu->lcd.command == 0x36u) {
        emu->lcd.madctl = emu->lcd.params[0];
    } else if (emu->lcd.command == 0x3au) {
        emu->lcd.colmod = emu->lcd.params[0];
    }
    if (emu->trace_fd >= 0) {
        emu_trace_text(emu, "lcd params cmd="); emu_trace_hex32(emu, emu->lcd.command);
        emu_trace_text(emu, " count="); emu_trace_hex32(emu, (u32)emu->lcd.param_count);
        if (emu->lcd.command == 0x2au) {
            emu_trace_text(emu, " x0="); emu_trace_hex32(emu, (u32)emu->lcd.x0);
            emu_trace_text(emu, " x1="); emu_trace_hex32(emu, (u32)emu->lcd.x1);
        } else if (emu->lcd.command == 0x2bu) {
            emu_trace_text(emu, " y0="); emu_trace_hex32(emu, (u32)emu->lcd.y0);
            emu_trace_text(emu, " y1="); emu_trace_hex32(emu, (u32)emu->lcd.y1);
        } else if (emu->lcd.command == 0x36u) {
            emu_trace_text(emu, " madctl="); emu_trace_hex32(emu, (u32)emu->lcd.madctl);
        } else if (emu->lcd.command == 0x3au) {
            emu_trace_text(emu, " colmod="); emu_trace_hex32(emu, (u32)emu->lcd.colmod);
        }
        emu_trace_text(emu, "\n");
    }
}

void emu_lcd_gpio_sync(EmuState *emu) {
    emu->lcd.cs = (emu->gpio_out & (1u << LCD_CS_PIN)) != 0u;
    emu->lcd.dc = (emu->gpio_out & (1u << LCD_DC_PIN)) != 0u;
}

static void lcd_data(EmuState *emu, u8 byte) {
    int expected = lcd_expected_params(emu->lcd.command);
    int pixel_bytes = emu->lcd.colmod == 0x55u ? 2 : 3;
    if (expected > 0) {
        if (emu->lcd.param_count < (int)sizeof(emu->lcd.params)) emu->lcd.params[emu->lcd.param_count++] = byte;
        if (emu->lcd.param_count == expected) lcd_apply_params(emu);
        return;
    }
    if (emu->lcd.command == 0x2cu) {
        emu->lcd.pixel[emu->lcd.pixel_count++] = byte;
        if (emu->lcd.pixel_count == pixel_bytes) {
            u8 red = emu->lcd.pixel[0];
            u8 green = emu->lcd.pixel[1];
            u8 blue = emu->lcd.pixel[2];
            if (pixel_bytes == 2) {
                u16 rgb565 = (u16)(((u16)emu->lcd.pixel[0] << 8) | emu->lcd.pixel[1]);
                red = (u8)((((rgb565 >> 11) & 0x1fu) * 255u + 15u) / 31u);
                green = (u8)((((rgb565 >> 5) & 0x3fu) * 255u + 31u) / 63u);
                blue = (u8)(((rgb565 & 0x1fu) * 255u + 15u) / 31u);
            }
            lcd_set_pixel(emu, emu->lcd.x, emu->lcd.y, red, green, blue);
            emu->frame_hash = (emu->frame_hash * 16777619u) ^ ((u32)red << 16) ^ ((u32)green << 8) ^ blue;
            emu->lcd.pixel_count = 0;
            emu->lcd.x += 1;
            if (emu->lcd.x > emu->lcd.x1) {
                emu->lcd.x = emu->lcd.x0;
                emu->lcd.y += 1;
                if (emu->lcd.y > emu->lcd.y1) emu->lcd.y = emu->lcd.y0;
            }
        }
    }
}

static void lcd_command(EmuState *emu, u8 command) {
    emu->lcd.command = command;
    emu->lcd.param_count = 0;
    emu->lcd.pixel_count = 0;
    if (!emu->seen_lcd_command) {
        emu->seen_lcd_command = 1;
        emu->first_lcd_command = command;
        emu->first_lcd_command_pc = emu->current_pc;
        emu->first_lcd_command_cycles = emu->cycles;
    }
    if (emu->trace_fd >= 0) {
        emu_trace_text(emu, "lcd cmd="); emu_trace_hex32(emu, command); emu_trace_text(emu, " cycles="); emu_trace_hex32(emu, emu->cycles); emu_trace_text(emu, "\n");
    }
    if (command == 0x2cu) {
        emu->lcd.x = emu->lcd.x0;
        emu->lcd.y = emu->lcd.y0;
    } else if (command == 0x10u) {
        emu->lcd.sleep = 1;
    } else if (command == 0x11u) {
        emu->lcd.sleep = 0;
    } else if (command == 0x20u) {
        emu->lcd.invert = 0;
    } else if (command == 0x21u) {
        emu->lcd.invert = 1;
    } else if (command == 0x28u) {
        emu->lcd.display_on = 0;
    } else if (command == 0x29u) {
        emu->lcd.display_on = 1;
    } else if (command == 0x01u) {
        emu->lcd.sleep = 1;
        emu->lcd.display_on = 0;
        emu->lcd.invert = 0;
        emu->lcd.madctl = 0;
        emu->lcd.colmod = 0;
        emu->lcd.x0 = 0;
        emu->lcd.x1 = LCD_W - 1;
        emu->lcd.y0 = 0;
        emu->lcd.y1 = LCD_H - 1;
    }
}

void emu_lcd_spi_byte(EmuState *emu, u8 byte) {
    if (emu->lcd.cs != 0) return;
    if (emu->lcd.dc == 0) lcd_command(emu, byte);
    else lcd_data(emu, byte);
}