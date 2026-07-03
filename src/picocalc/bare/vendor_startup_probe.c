#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#define ROM_CODE(a, b) ((uint32_t)(a) | ((uint32_t)(b) << 8))

#define SIO_DIV_UDIVIDEND REG32(0xd0000060u)
#define SIO_DIV_UDIVISOR REG32(0xd0000064u)
#define SIO_DIV_QUOTIENT REG32(0xd0000070u)
#define SIO_DIV_REMAINDER REG32(0xd0000074u)
#define SIO_DIV_CSR REG32(0xd0000078u)
#define SIO_SPINLOCK11 REG32(0xd000012cu)
#define TIMERAWL REG32(0x40054028u)
#define RTC_CTRL REG32(0x4005c00cu)
#define I2C_IC_ENABLE_STATUS REG32(I2C1_BASE + 0x9cu)
#define XIP_SSI_SR REG32(0x18000028u)

typedef uint32_t (*rom_lookup_fn)(uint32_t table, uint32_t code);
typedef uint32_t (*rom_word_fn)(uint32_t value);
typedef void *(*rom_mem_fn)(void *dst, uint32_t value_or_src, uint32_t len);

static uint8_t g_src[16];
static uint8_t g_dst[16];

static uint32_t read_bootrom_halfword(uint32_t addr) {
    uint32_t value;
    __asm__ volatile ("ldrh %0, [%1]" : "=r"(value) : "r"(addr) : "memory");
    return value;
}

static uint32_t rom_lookup(uint32_t table, uint32_t code) {
    rom_lookup_fn lookup = (rom_lookup_fn)(read_bootrom_halfword(0x18u) | 1u);
    return lookup(table, code);
}

static uint32_t local_indirect_target(uint32_t value) {
    return value + 7u;
}

__attribute__((naked, noinline)) static uint32_t blx_register_probe(uint32_t target, uint32_t value) {
    (void)target;
    (void)value;
    __asm__ volatile(
        "push {lr}\n"
        "mov r3, r0\n"
        "mov r0, r1\n"
        "blx r3\n"
        "pop {pc}\n");
}

static uint32_t primask_barrier_probe(void) {
    uint32_t before;
    uint32_t during;
    uint32_t after;
    __asm__ volatile(
        "mrs %0, PRIMASK\n"
        "cpsid i\n"
        "mrs %1, PRIMASK\n"
        "msr PRIMASK, %0\n"
        "mrs %2, PRIMASK\n"
        "dmb sy\n"
        "dsb sy\n"
        "isb sy\n"
        : "=r"(before), "=r"(during), "=r"(after)
        :
        : "memory");
    return (before << 2) | (during << 1) | after;
}

static uint32_t bootrom_helper_probe(void) {
    uint32_t func_table = read_bootrom_halfword(0x14u);
    uint32_t data_table = read_bootrom_halfword(0x16u);
    rom_word_fn clz = (rom_word_fn)rom_lookup(func_table, ROM_CODE('L', '3'));
    rom_word_fn popcount = (rom_word_fn)rom_lookup(func_table, ROM_CODE('P', '3'));
    rom_word_fn reverse = (rom_word_fn)rom_lookup(func_table, ROM_CODE('R', '3'));
    rom_word_fn ctz = (rom_word_fn)rom_lookup(func_table, ROM_CODE('T', '3'));
    rom_mem_fn mem_set = (rom_mem_fn)rom_lookup(func_table, ROM_CODE('M', 'S'));
    rom_mem_fn mem_copy = (rom_mem_fn)rom_lookup(func_table, ROM_CODE('M', 'C'));
    uint32_t sd = rom_lookup(data_table, ROM_CODE('S', 'D'));
    uint32_t sf = rom_lookup(data_table, ROM_CODE('S', 'F'));
    uint32_t flash_connect = rom_lookup(func_table, ROM_CODE('I', 'F'));
    uint32_t flash_exit_xip = rom_lookup(func_table, ROM_CODE('E', 'X'));
    uint32_t flash_flush = rom_lookup(func_table, ROM_CODE('F', 'C'));
    uint32_t flash_erase = rom_lookup(func_table, ROM_CODE('R', 'E'));
    uint32_t flash_program = rom_lookup(func_table, ROM_CODE('R', 'P'));
    uint32_t ok = 1u;
    int i;

    if (clz == 0u || popcount == 0u || reverse == 0u || ctz == 0u || mem_set == 0u || mem_copy == 0u) ok = 0u;
    if (sd == 0u || sf == 0u || flash_connect == 0u || flash_exit_xip == 0u || flash_flush == 0u || flash_erase == 0u || flash_program == 0u) ok = 0u;
    if (clz(0x00f00000u) != 8u) ok = 0u;
    if (popcount(0xf0f00001u) != 9u) ok = 0u;
    if (reverse(0x00000003u) != 0xc0000000u) ok = 0u;
    if (ctz(0x00001000u) != 12u) ok = 0u;

    mem_set(g_src, 0x5au, sizeof(g_src));
    mem_copy(g_dst, (uint32_t)g_src, sizeof(g_dst));
    for (i = 0; i < (int)sizeof(g_dst); ++i) {
        if (g_dst[i] != 0x5au) ok = 0u;
    }
    return ok;
}

static uint32_t sio_probe(void) {
    uint32_t lock;
    SIO_DIV_UDIVIDEND = 1000u;
    SIO_DIV_UDIVISOR = 7u;
    lock = SIO_SPINLOCK11;
    SIO_SPINLOCK11 = 0u;
    return ((SIO_DIV_CSR & 1u) != 0u) && SIO_DIV_QUOTIENT == 142u && SIO_DIV_REMAINDER == 6u && lock != 0u;
}

static uint32_t rtc_probe(void) {
    RTC_CTRL = 0u;
    if ((RTC_CTRL & (1u << 1)) != 0u) return 0u;
    RTC_CTRL = 1u;
    return (RTC_CTRL & (1u << 1)) != 0u;
}

static uint32_t spi0_probe(void) {
    reset_unreset(RESET_SPI0);
    return (SPI0_SSPSR & (SPI_SR_TFE | SPI_SR_TNF)) == (SPI_SR_TFE | SPI_SR_TNF);
}

static uint32_t mmio_status_probe(void) {
    uint32_t timer0 = TIMERAWL;
    uint32_t timer1 = TIMERAWL;
    uint32_t xip_status = XIP_SSI_SR;
    I2C_IC_ENABLE = 1u;
    return timer1 >= timer0 && (I2C_IC_ENABLE_STATUS & 1u) != 0u && (xip_status & 0x0fu) == 0x0eu;
}

void bare_main(void) {
    uint32_t ok = 1u;
    picocalc_lcd_init();
    picocalc_lcd_clear(0x000000u);

    if (blx_register_probe((uint32_t)local_indirect_target | 1u, 35u) != 42u) ok = 0u;
    if (primask_barrier_probe() != 2u) ok = 0u;
    if (!bootrom_helper_probe()) ok = 0u;
    if (!sio_probe()) ok = 0u;
    if (!rtc_probe()) ok = 0u;
    if (!spi0_probe()) ok = 0u;
    if (!mmio_status_probe()) ok = 0u;

    picocalc_lcd_fill_rect(18, 40, 301, 232, ok ? 0x003824u : 0x401000u);
    picocalc_lcd_puts_scale(38, 72, ok ? "VENDOR STARTUP" : "VENDOR START FAIL", 0xffffffu, ok ? 0x003824u : 0x401000u, 1);
    picocalc_lcd_puts_scale(38, 100, ok ? "BLX ROM SIO OK" : "CHECK TRACE", 0x80ff80u, ok ? 0x003824u : 0x401000u, 1);
    picocalc_lcd_puts_scale(38, 128, ok ? "PRIMASK TIMER XIP" : "BOOT/MMIO MISMATCH", 0x80ff80u, ok ? 0x003824u : 0x401000u, 1);
    while (1) {
    }
}