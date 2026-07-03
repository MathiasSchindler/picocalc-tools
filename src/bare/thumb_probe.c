#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

static const uint16_t g_halfwords[] = {0x1001u, 0x1002u, 0x1234u, 0x1004u};

__attribute__((naked, noinline)) static uint32_t thumb_probe_asm(const uint16_t *values) {
    (void)values;
    __asm__ volatile(
    "movs r1, #0\n"
        "ldrh r3, [r0, #4]\n"
    "add r0, r1, r3\n"
        "bx lr\n");
}

void bare_main(void) {
    uint32_t result;
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);
    result = thumb_probe_asm(g_halfwords);
    if (result == 0x1234u) {
        picocalc_lcd_fill_rect(20, 52, 299, 214, 0x102040u);
        picocalc_lcd_puts_scale(44, 84, "THUMB PROBE OK", 0xffffffu, 0x102040u, 1);
        picocalc_lcd_puts_scale(44, 112, "LDRH IMMEDIATE", 0x80ff80u, 0x102040u, 1);
    } else {
        picocalc_lcd_fill_rect(20, 52, 299, 214, 0x401000u);
        picocalc_lcd_puts_scale(44, 84, "THUMB PROBE FAIL", 0xffffffu, 0x401000u, 1);
    }
    while (1) {
    }
}