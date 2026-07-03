#include "picocalc_kbd_bare.h"
#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

static char hex_digit(unsigned int value) {
    value &= 0xfu;
    return value < 10u ? (char)('0' + value) : (char)('A' + value - 10u);
}

static void show_code(int key) {
    char line[10];
    unsigned int value = (unsigned int)key;
    line[0] = 'C';
    line[1] = 'O';
    line[2] = 'D';
    line[3] = 'E';
    line[4] = ' ';
    line[5] = '0';
    line[6] = 'X';
    line[7] = hex_digit(value >> 4);
    line[8] = hex_digit(value);
    line[9] = 0;
    picocalc_lcd_puts(24, 166, line, 0x00ff00u, 0x000000u);
}

void bare_main(void) {
    picocalc_lcd_init();
    picocalc_kbd_init();
    picocalc_lcd_clear(0x000000u);
    picocalc_lcd_puts(24, 40, "PicoCalc", 0x00ff00u, 0x000000u);
    picocalc_lcd_puts(24, 82, "bare keys", 0x00ff00u, 0x000000u);
    picocalc_lcd_puts(24, 124, "press key", 0x00ff00u, 0x000000u);
    picocalc_lcd_puts(24, 166, "CODE 0X00", 0x00ff00u, 0x000000u);

#ifdef PICOCALC_BARE_SIM
    show_code('u');
#else
    while (1) {
        int key = picocalc_kbd_read_key();
        if (key >= 0) show_code(key);
        reg_wait_cycles(100000u);
    }
#endif
}