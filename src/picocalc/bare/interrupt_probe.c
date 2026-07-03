#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

static volatile uint32_t g_svc_count;
static volatile uint32_t g_systick_count;
static volatile uint32_t g_irq0_count;

void bare_svc(void) {
    g_svc_count += 1u;
}

void bare_systick(void) {
    g_systick_count += 1u;
    if (g_systick_count >= 2u) PPB_SYST_CSR = 0u;
}

void bare_irq0(void) {
    g_irq0_count += 1u;
    PPB_NVIC_ICPR = 1u;
}

static void wait_for(volatile uint32_t *counter, uint32_t target) {
    volatile uint32_t spin;
    for (spin = 0u; spin < 200000u && *counter < target; ++spin) {
    }
}

void bare_main(void) {
    int ok;
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);

    __asm__ volatile ("svc #7" ::: "memory");

    PPB_SYST_RVR = 4u;
    PPB_SYST_CVR = 0u;
    PPB_SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE;
    wait_for(&g_systick_count, 2u);

    PPB_NVIC_ISER = 1u;
    PPB_NVIC_ISPR = 1u;
    wait_for(&g_irq0_count, 1u);

    ok = g_svc_count == 1u && g_systick_count >= 2u && g_irq0_count == 1u;
    picocalc_lcd_fill_rect(16, 32, 303, 236, ok ? 0x004020u : 0x401000u);
    picocalc_lcd_puts_scale(36, 68, ok ? "INTERRUPT TEST" : "INTERRUPT FAIL", 0xffffffu, ok ? 0x004020u : 0x401000u, 1);
    picocalc_lcd_puts_scale(36, 96, ok ? "SVC SYSTICK IRQ0 OK" : "CHECK EXCEPTION TRACE", 0xffffffu, ok ? 0x004020u : 0x401000u, 1);
    while (1) {
    }
}