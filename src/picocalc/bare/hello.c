#include "picocalc_lcd_bare.h"

void bare_main(void) {
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    picocalc_lcd_puts(24, 40, "PicoCalc", 0x00ff00u, 0x000000u);
    picocalc_lcd_puts(24, 82, "bare hello", 0x00ff00u, 0x000000u);
#ifdef PICOCALC_SDK_FLASH
    picocalc_lcd_puts(24, 124, "Pico SDK", 0x00ff00u, 0x000000u);
    picocalc_lcd_puts(24, 166, "flash UF2", 0x00ff00u, 0x000000u);
#else
    picocalc_lcd_puts(24, 124, "no Pico SDK", 0x00ff00u, 0x000000u);
    picocalc_lcd_puts(24, 166, "SD boot bin", 0x00ff00u, 0x000000u);
#endif
#ifndef PICOCALC_BARE_SIM
    while (1) {
    }
#endif
}