#ifndef PICOCALC_EMU_LCD_H
#define PICOCALC_EMU_LCD_H

#include "emu_state.h"

void emu_lcd_gpio_sync(EmuState *emu);
void emu_lcd_spi_byte(EmuState *emu, u8 byte);

#endif