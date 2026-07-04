#include "host_nolibc.h"
#include "emu_bootrom.h"
#include "emu_lcd.h"
#include "emu_mem.h"
#include "emu_trace.h"
#include "emu_util.h"
#include "gif_writer.h"
#include "png_writer.h"

#define FLASH_BASE 0x10000000u
#define APP_BASE   0x10032000u
#define APP_FLASH_OFFSET (APP_BASE - FLASH_BASE)
#define BOOT2_BASE 0x10000000u
#define BOOT2_SIZE 0x00000100u
#define RAM_BASE   0x20000000u
#define RAM_SIZE   0x00042000u
#define LCD_W 320
#define LCD_H 320

#define RESETS_RESET           0x4000c000u
#define RESETS_DONE            0x4000c008u
#define RESETS_RESET_XOR       0x4000d000u
#define RESETS_RESET_SET       0x4000e000u
#define RESETS_RESET_CLR       0x4000f000u
#define RESET_IO_BANK0         (1u << 5)
#define RESET_I2C1             (1u << 4)
#define RESET_PADS_BANK0       (1u << 8)
#define RESET_SPI0             (1u << 16)
#define RESET_SPI1             (1u << 17)
#define RESET_MODELED_MASK     (RESET_IO_BANK0 | RESET_I2C1 | RESET_PADS_BANK0 | RESET_SPI0 | RESET_SPI1)
#define RESET_DONE_MASK        0x01ffffffu

#define IO_BANK0_BASE          0x40014000u
#define IO_BANK0_SIZE          0x00002000u
#define PADS_BANK0_BASE        0x4001c000u
#define PADS_BANK0_SIZE        0x00001000u

#define CLOCKS_BASE            0x40008000u
#define CLOCKS_SIZE            0x00001000u
#define CLK_REF_SELECTED       0x38u
#define CLK_SYS_SELECTED       0x44u
#define CLK_PERI_SELECTED      0x50u
#define CLK_USB_SELECTED       0x5cu
#define CLK_ADC_SELECTED       0x68u
#define CLK_RTC_SELECTED       0x74u
#define XOSC_BASE              0x40024000u
#define XOSC_SIZE              0x00001000u
#define XOSC_STATUS            0x04u
#define XOSC_STATUS_STABLE     ((1u << 31) | (1u << 12))
#define PLL_SYS_BASE           0x40028000u
#define PLL_USB_BASE           0x4002c000u
#define PLL_SIZE               0x00001000u
#define PLL_CS_LOCK            (1u << 31)

#define SPI0_BASE              0x4003c000u
#define SPI1_BASE              0x40040000u
#define SPI_SIZE               0x00001000u
#define SPI_SSPCR0_OFF         0x00u
#define SPI_SSPCR1_OFF         0x04u
#define SPI_SSPDR_OFF          0x08u
#define SPI_SSPSR_OFF          0x0cu
#define SPI_SSPCPSR_OFF        0x10u
#define SPI_SSPICR_OFF         0x20u
#define SPI_SSPDMACR_OFF       0x24u
#define SPI_SSPCR0             (SPI1_BASE + SPI_SSPCR0_OFF)
#define SPI_SSPCR1             (SPI1_BASE + SPI_SSPCR1_OFF)
#define SPI_SSPDR              (SPI1_BASE + SPI_SSPDR_OFF)
#define SPI_SSPSR              (SPI1_BASE + SPI_SSPSR_OFF)
#define SPI_SSPCPSR            (SPI1_BASE + SPI_SSPCPSR_OFF)
#define SPI_SSPICR             (SPI1_BASE + SPI_SSPICR_OFF)
#define SPI0_SSPDR             (SPI0_BASE + SPI_SSPDR_OFF)
#define SPI_SR_TFE             (1u << 0)
#define SPI_SR_TNF             (1u << 1)
#define SPI_SR_RNE             (1u << 2)
#define SPI_SR_BSY             (1u << 4)
#define SPI_FIFO_SIZE          8
#define SPI_BYTE_CYCLES        4u

#define I2C1_BASE              0x40048000u
#define I2C_IC_CON             (I2C1_BASE + 0x00u)
#define I2C_IC_TAR             (I2C1_BASE + 0x04u)
#define I2C_IC_DATA_CMD        (I2C1_BASE + 0x10u)
#define I2C_IC_FS_HCNT         (I2C1_BASE + 0x1cu)
#define I2C_IC_FS_LCNT         (I2C1_BASE + 0x20u)
#define I2C_IC_RAW_INTR        (I2C1_BASE + 0x34u)
#define I2C_IC_RX_TL           (I2C1_BASE + 0x38u)
#define I2C_IC_TX_TL           (I2C1_BASE + 0x3cu)
#define I2C_IC_CLR_TX_ABRT     (I2C1_BASE + 0x54u)
#define I2C_IC_CLR_STOP        (I2C1_BASE + 0x60u)
#define I2C_IC_ENABLE          (I2C1_BASE + 0x6cu)
#define I2C_IC_STATUS          (I2C1_BASE + 0x70u)
#define I2C_IC_TXFLR           (I2C1_BASE + 0x74u)
#define I2C_IC_RXFLR           (I2C1_BASE + 0x78u)
#define I2C_IC_SDA_HOLD        (I2C1_BASE + 0x7cu)
#define I2C_IC_TX_ABRT         (I2C1_BASE + 0x80u)
#define I2C_IC_DMA_CR          (I2C1_BASE + 0x88u)
#define I2C_IC_ENABLE_STATUS   (I2C1_BASE + 0x9cu)
#define I2C_IC_FS_SPKLEN       (I2C1_BASE + 0xa0u)
#define I2C_DATA_CMD_READ      (1u << 8)
#define I2C_DATA_CMD_STOP      (1u << 9)
#define I2C_RAW_STOP_DET       (1u << 9)
#define I2C_RAW_TX_ABRT        (1u << 6)
#define I2C_RAW_TX_EMPTY       (1u << 4)
#define I2C_STATUS_RFNE        (1u << 3)
#define I2C_STATUS_TFE         (1u << 2)
#define I2C_STATUS_TFNF        (1u << 1)
#define I2C_FIFO_SIZE          8

#define UART0_BASE             0x40034000u
#define UART_SIZE              0x00001000u
#define UART_DR                0x00u
#define UART_FR                0x18u
#define UART_FR_RXFE           (1u << 4)
#define UART_FR_TXFE           (1u << 7)

#define PIO0_BASE              0x50200000u
#define PIO1_BASE              0x50300000u
#define PIO_SIZE               0x00001000u
#define PIO_FSTAT_OFF          0x04u
#define PIO_FDEBUG_OFF         0x08u
#define PIO_TXF0_OFF           0x10u
#define PIO_TXF3_OFF           0x1cu
#define PIO_RXF0_OFF           0x20u
#define PIO_RXF3_OFF           0x2cu
#define PIO_FSTAT_RXEMPTY_MASK 0x00000f00u
#define PIO_FSTAT_TXEMPTY_MASK 0x0f000000u

#define CYW43_PIN_WL_REG_ON    23u
#define CYW43_PIN_WL_DATA      24u
#define CYW43_PIN_WL_CS        25u
#define CYW43_PIN_WL_CLOCK     29u

#define DMA_BASE               0x50000000u
#define DMA_CH_STRIDE          0x40u
#define DMA_READ_ADDR          0x00u
#define DMA_WRITE_ADDR         0x04u
#define DMA_TRANS_COUNT        0x08u
#define DMA_CTRL_TRIG          0x0cu
#define DMA_CTRL_EN            (1u << 0)
#define DMA_CTRL_DATA_SIZE_MASK (3u << 2)
#define DMA_CTRL_INCR_READ     (1u << 4)
#define DMA_CTRL_INCR_WRITE    (1u << 5)
#define DMA_CTRL_DREQ_SHIFT    15u
#define DMA_CTRL_DREQ_MASK     (0x3fu << DMA_CTRL_DREQ_SHIFT)
#define DMA_CTRL_BUSY          (1u << 24)
#define DREQ_SPI0_TX           16u
#define DREQ_SPI1_TX           18u

#define PPB_SYST_CSR           0xe000e010u
#define PPB_SYST_RVR           0xe000e014u
#define PPB_SYST_CVR           0xe000e018u
#define PPB_NVIC_ISER          0xe000e100u
#define PPB_NVIC_ICER          0xe000e180u
#define PPB_NVIC_ISPR          0xe000e200u
#define PPB_NVIC_ICPR          0xe000e280u
#define PPB_SCB_ICSR           0xe000ed04u
#define PPB_SCB_VTOR           0xe000ed08u
#define SYST_CSR_ENABLE        (1u << 0)
#define SYST_CSR_TICKINT       (1u << 1)
#define SYST_CSR_CLKSOURCE     (1u << 2)
#define SYST_CSR_COUNTFLAG     (1u << 16)

#define EXC_RESET              1u
#define EXC_NMI                2u
#define EXC_HARDFAULT          3u
#define EXC_SVC                11u
#define EXC_PENDSV             14u
#define EXC_SYSTICK            15u
#define EXC_IRQ0               16u
#define EXC_RETURN_THREAD_MSP  0xfffffff9u

#define SIO_GPIO_OUT_SET       0xd0000014u
#define SIO_GPIO_OUT_CLR       0xd0000018u
#define SIO_GPIO_OE_SET        0xd0000024u
#define SIO_GPIO_OE_CLR        0xd0000028u
#define SIO_CPUID              0xd0000000u
#define SIO_DIV_UDIVIDEND      0xd0000060u
#define SIO_DIV_UDIVISOR       0xd0000064u
#define SIO_DIV_SDIVIDEND      0xd0000068u
#define SIO_DIV_SDIVISOR       0xd000006cu
#define SIO_DIV_QUOTIENT       0xd0000070u
#define SIO_DIV_REMAINDER      0xd0000074u
#define SIO_DIV_CSR            0xd0000078u
#define SIO_DIV_CSR_READY      (1u << 0)
#define SIO_DIV_CSR_DIRTY      (1u << 1)
#define SIO_SPINLOCK_BASE      0xd0000100u
#define SIO_SPINLOCK_END       0xd0000180u

#define TIMER_BASE             0x40054000u
#define TIMERAWH               (TIMER_BASE + 0x24u)
#define TIMERAWL               (TIMER_BASE + 0x28u)
#define TIMER_CYCLES_PER_US    100u
#define TIMER_WAIT_ACCEL_US    1000u

#define RTC_BASE               0x4005c000u
#define RTC_SIZE               0x00001000u
#define RTC_CTRL               0x0cu
#define RTC_CTRL_ACTIVE        (1u << 1)

#define BUSCTRL_BASE           0x40030000u
#define BUSCTRL_SIZE           0x00001000u
#define BUSCTRL_PRIORITY_ACK   0x04u
#define BUSCTRL_PERFSEL0       0x0cu
#define BUSCTRL_PERFSEL1       0x14u
#define BUSCTRL_PERFSEL2       0x1cu
#define BUSCTRL_PERFSEL3       0x24u

#define ROSC_BASE              0x40060000u
#define ROSC_STATUS            (ROSC_BASE + 0x18u)
#define ROSC_RANDOMBIT         (ROSC_BASE + 0x1cu)
#define ROSC_STATUS_STABLE     (1u << 31)
#define ROSC_STATUS_DIV_RUNNING (1u << 16)
#define ROSC_STATUS_ENABLED    (1u << 12)

#define XIP_SSI_BASE           0x18000000u
#define XIP_SSI_SIZE           0x00000100u
#define XIP_SSI_SR             (XIP_SSI_BASE + 0x28u)
#define XIP_SSI_DR0            (XIP_SSI_BASE + 0x60u)

#define LCD_CS_PIN             13u
#define LCD_DC_PIN             14u
#define LCD_RST_PIN            15u
#define KBD_ADDR               0x1fu
#define KBD_REG_KEY            0x09u

#define LIVE_COLS 80
#define LIVE_ROWS 40
#define SYMBOL_MAP_MAX_SIZE (128u * 1024u)
#define SYMBOL_MAP_MAX_SYMBOLS 1024
#define SYMBOL_MAP_NAME_STORAGE (32u * 1024u)

#include "emu_state.h"

static EmuState g_emu;

typedef struct {
    u32 value;
    const char *name;
} SymbolMapEntry;

static char g_symbol_map_storage[SYMBOL_MAP_MAX_SIZE + 1u];
static char g_symbol_name_storage[SYMBOL_MAP_NAME_STORAGE];
static SymbolMapEntry g_symbol_map[SYMBOL_MAP_MAX_SYMBOLS];
static int g_symbol_map_count;
static usize g_symbol_name_used;

#define g_flash g_emu.flash
#define g_flash_size g_emu.flash_size
#define g_ram g_emu.ram
#define g_framebuffer g_emu.framebuffer
#define g_png_work g_emu.png_work
#define g_gif g_emu.gif
#define g_key_script g_emu.key_script
#define g_key_file g_emu.key_file
#define g_key_script_pos g_emu.key_script_pos
#define g_cycles g_emu.cycles
#define g_sim_ms g_emu.sim_ms
#define g_frame_ready g_emu.frame_ready
#define g_live_stdin g_emu.live_stdin
#define g_trace_fd g_emu.trace_fd
#define g_trace_mask g_emu.trace_mask
#define g_frame_hash g_emu.frame_hash
#define g_last_output_hash g_emu.last_output_hash
#define g_expected_hash g_emu.expected_hash
#define g_expect_hash g_emu.expect_hash
#define g_live_terminal g_emu.live_terminal
#define g_fail_on_budget g_emu.fail_on_budget
#define g_report_milestones g_emu.report_milestones
#define g_input_base g_emu.input_base
#define g_vector_base g_emu.vector_base
#define g_max_steps g_emu.max_steps
#define g_gif_active g_emu.gif_active
#define g_gif_fd g_emu.gif_fd
#define g_gif_fps g_emu.gif_fps
#define g_flash_state_path g_emu.flash_state_path
#define g_cyw43_wifi_fw_path g_emu.cyw43_wifi_fw_path
#define g_cyw43_bt_fw_path g_emu.cyw43_bt_fw_path
#define g_cyw43_nvram_path g_emu.cyw43_nvram_path
#define g_cyw43_inventory g_emu.cyw43_inventory
#define g_cyw43_model g_emu.cyw43_model
#define g_cyw43_pio_tx_words g_emu.cyw43_pio_tx_words
#define g_cyw43_pio_rx_words g_emu.cyw43_pio_rx_words
#define g_cyw43_dma_events g_emu.cyw43_dma_events
#define g_cyw43_gpio_events g_emu.cyw43_gpio_events
#define g_current_pc g_emu.current_pc
#define g_frame_index g_emu.frame_index
#define g_target_frames g_emu.target_frames
#define g_seen_lcd_command g_emu.seen_lcd_command
#define g_first_lcd_command g_emu.first_lcd_command
#define g_first_lcd_command_pc g_emu.first_lcd_command_pc
#define g_first_lcd_command_cycles g_emu.first_lcd_command_cycles
#define g_seen_lcd_pixel g_emu.seen_lcd_pixel
#define g_first_lcd_pixel_x g_emu.first_lcd_pixel_x
#define g_first_lcd_pixel_y g_emu.first_lcd_pixel_y
#define g_first_lcd_pixel_rgb g_emu.first_lcd_pixel_rgb
#define g_first_lcd_pixel_pc g_emu.first_lcd_pixel_pc
#define g_first_lcd_pixel_cycles g_emu.first_lcd_pixel_cycles
#define g_seen_lcd_nonblack g_emu.seen_lcd_nonblack
#define g_first_lcd_nonblack_x g_emu.first_lcd_nonblack_x
#define g_first_lcd_nonblack_y g_emu.first_lcd_nonblack_y
#define g_first_lcd_nonblack_rgb g_emu.first_lcd_nonblack_rgb
#define g_first_lcd_nonblack_pc g_emu.first_lcd_nonblack_pc
#define g_first_lcd_nonblack_cycles g_emu.first_lcd_nonblack_cycles
#define g_skip_tick_after_exception_return g_emu.skip_tick_after_exception_return
#define g_resets_reset g_emu.resets_reset
#define g_gpio_out g_emu.gpio_out
#define g_gpio_oe g_emu.gpio_oe
#define g_sio_dividend g_emu.sio_dividend
#define g_sio_divisor g_emu.sio_divisor
#define g_sio_quotient g_emu.sio_quotient
#define g_sio_remainder g_emu.sio_remainder
#define g_sio_div_signed g_emu.sio_div_signed
#define g_sio_div_dirty g_emu.sio_div_dirty
#define g_io_bank0 g_emu.io_bank0
#define g_pads_bank0 g_emu.pads_bank0
#define g_clocks g_emu.clocks
#define g_xosc g_emu.xosc
#define g_pll_sys g_emu.pll_sys
#define g_pll_usb g_emu.pll_usb
#define g_rtc g_emu.rtc
#define g_uart0 g_emu.uart0
#define g_pio0 g_emu.pio0
#define g_pio1 g_emu.pio1
#define g_xip_dr0_command g_emu.xip_dr0_command
#define g_xip_dr0_reads g_emu.xip_dr0_reads
#define g_lcd g_emu.lcd
#define g_spi g_emu.spi
#define g_i2c g_emu.i2c
#define g_dma g_emu.dma
#define g_core g_emu.core
#define g_saved_termios g_emu.saved_termios
#define g_saved_termios_valid g_emu.saved_termios_valid
#define g_live_buffer g_emu.live_buffer

#define parse_trace_kinds(s, out_mask) emu_trace_parse_kinds((s), (out_mask))
#define trace_text(s) emu_trace_text(&g_emu, (s))
#define trace_enabled(kind) emu_trace_enabled(&g_emu, (kind))
#define trace_hex32(value) emu_trace_hex32(&g_emu, (value))
#define trace_pair(name, value) emu_trace_pair(&g_emu, (name), (value))
#define trace_mmio(kind, addr, value) emu_trace_mmio(&g_emu, (kind), (addr), (value))
#define trace_unknown_mmio(kind, addr, value, pc) emu_trace_unknown_mmio(&g_emu, (kind), (addr), (value), (pc))
#define trace_xip_mmio(kind, addr, value) emu_trace_xip_mmio(&g_emu, (kind), (addr), (value))
#define trace_branch(kind, pc, target, lr, op) emu_trace_branch(&g_emu, (kind), (pc), (target), (lr), (op))
#define lcd_gpio_sync() emu_lcd_gpio_sync(&g_emu)
#define lcd_spi_byte(byte) emu_lcd_spi_byte(&g_emu, (byte))
#define boot2_stub_read8(addr, out_value) emu_boot2_stub_read8((addr), (out_value))
#define boot2_stub_read16(addr, out_value) emu_boot2_stub_read16((addr), (out_value))
#define boot2_stub_read32(addr, out_value) emu_boot2_stub_read32((addr), (out_value))
#define bootrom_read8(addr, out_value) emu_bootrom_read8((addr), (out_value))
#define bootrom_read16(addr, out_value) emu_bootrom_read16((addr), (out_value))
#define bootrom_read32(addr, out_value) emu_bootrom_read32((addr), (out_value))
#define bootrom_lookup_call(cpu, return_pc) emu_bootrom_lookup_call(&g_emu, (cpu), (return_pc))
#define bootrom_function_call(cpu, target, return_pc) emu_bootrom_function_call(&g_emu, (cpu), (target), (return_pc), &g_bootrom_mem_ops, 0)
#define flash_offset(addr, out_off) emu_mem_flash_offset(&g_emu, (addr), (out_off))
#define ram_offset(addr, out_off) emu_mem_ram_offset(&g_emu, (addr), (out_off))
#define executable_addr(addr) emu_mem_executable_addr(&g_emu, (addr))
#define mem_read32(addr) emu_mem_read32(&g_emu, &g_mem_mmio_ops, 0, (addr))
#define mem_read16(addr) emu_mem_read16(&g_emu, &g_mem_mmio_ops, 0, (addr))
#define mem_read8(addr) emu_mem_read8(&g_emu, &g_mem_mmio_ops, 0, (addr))
#define mem_write32(addr, value) emu_mem_write32(&g_emu, &g_mem_mmio_ops, 0, (addr), (value))
#define mem_write16(addr, value) emu_mem_write16(&g_emu, &g_mem_mmio_ops, 0, (addr), (value))
#define mem_write8(addr, value) emu_mem_write8(&g_emu, &g_mem_mmio_ops, 0, (addr), (value))
#define flash_fill_erased() emu_mem_flash_fill_erased(&g_emu)

static u32 mem_mmio_read32(void *ctx, u32 addr);
static void mem_mmio_write8(void *ctx, u32 addr, u32 value);
static void mem_mmio_write16(void *ctx, u32 addr, u32 value);
static void mem_mmio_write32(void *ctx, u32 addr, u32 value);
static u8 bootrom_mem_read8(void *ctx, u32 addr);
static u32 bootrom_mem_read32(void *ctx, u32 addr);
static void bootrom_mem_write8(void *ctx, u32 addr, u32 value);
static void bootrom_mem_write32(void *ctx, u32 addr, u32 value);

static const EmuMemMmioOps g_mem_mmio_ops = {
    mem_mmio_read32,
    mem_mmio_write8,
    mem_mmio_write16,
    mem_mmio_write32
};

static const EmuBootromMemOps g_bootrom_mem_ops = {
    bootrom_mem_read8,
    bootrom_mem_read32,
    bootrom_mem_write8,
    bootrom_mem_write32
};

static void append_char(char *buf, usize *pos, usize cap, char ch) {
    if (*pos + 1u < cap) buf[(*pos)++] = ch;
}

static void append_text(char *buf, usize *pos, usize cap, const char *text) {
    while (*text != 0) append_char(buf, pos, cap, *text++);
}

static void append_uint(char *buf, usize *pos, usize cap, u32 value) {
    char tmp[10];
    int count = 0;
    int i;
    if (value == 0u) {
        append_char(buf, pos, cap, '0');
        return;
    }
    while (value != 0u && count < (int)sizeof(tmp)) {
        tmp[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    for (i = count - 1; i >= 0; --i) append_char(buf, pos, cap, tmp[i]);
}

static int path_has_frame_pattern(const char *path) {
    usize i = 0;
    while (path[i] != 0) {
        if (path[i] == '%' && path[i + 1u] == 'd') return 1;
        i += 1u;
    }
    return 0;
}

static int path_is_gif(const char *path) {
    return str_ends(path, ".gif");
}

static int path_is_frame_capture(const char *path) {
    return path_has_frame_pattern(path) || path_is_gif(path);
}

static void append_dec(char *out_path, usize *out_pos, int value) {
    char tmp[16];
    int n = 0;
    int i;
    if (value == 0) {
        out_path[(*out_pos)++] = '0';
        return;
    }
    while (value > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + value % 10);
        value /= 10;
    }
    for (i = n - 1; i >= 0; --i) out_path[(*out_pos)++] = tmp[i];
}

static void format_frame_path(const char *pattern, int index, char *out_path, usize out_cap) {
    usize in_pos = 0;
    usize out_pos = 0;
    while (pattern[in_pos] != 0 && out_pos + 1u < out_cap) {
        if (pattern[in_pos] == '%' && pattern[in_pos + 1u] == 'd') {
            append_dec(out_path, &out_pos, index);
            in_pos += 2u;
        } else {
            out_path[out_pos++] = pattern[in_pos++];
        }
    }
    out_path[out_pos < out_cap ? out_pos : out_cap - 1u] = 0;
}

static u32 framebuffer_hash(void) {
    u32 hash = 2166136261u;
    usize i;
    for (i = 0; i < sizeof(g_framebuffer); ++i) hash = (hash ^ g_framebuffer[i]) * 16777619u;
    return hash;
}

static int parse_map_hex(const char **text_io, u32 *out_value) {
    const char *text = *text_io;
    u32 value = 0;
    int any = 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    while (*text != 0 && *text != ' ' && *text != '\t' && *text != '\r' && *text != '\n') {
        int digit = hex_value(*text);
        if (digit < 0) return 0;
        value = (value << 4) | (u32)digit;
        any = 1;
        text += 1;
    }
    if (!any) return 0;
    *text_io = text;
    *out_value = value;
    return 1;
}

static int line_is_symbols_header(const char *line, const char *end) {
    const char text[] = "SYMBOLS";
    usize index = 0;
    while (line + index < end && text[index] != 0) {
        if (line[index] != text[index]) return 0;
        index += 1u;
    }
    return text[index] == 0 && line + index == end;
}

static void add_symbol_map_entry(u32 value, const char *name, const char *end) {
    usize name_len = 0;
    char *stored;
    if (g_symbol_map_count >= SYMBOL_MAP_MAX_SYMBOLS) return;
    while (name + name_len < end && name[name_len] != 0) name_len += 1u;
    while (name_len > 0u && (name[name_len - 1u] == ' ' || name[name_len - 1u] == '\t' || name[name_len - 1u] == '\r')) name_len -= 1u;
    if (name_len == 0u || g_symbol_name_used + name_len + 1u > SYMBOL_MAP_NAME_STORAGE) return;
    stored = g_symbol_name_storage + g_symbol_name_used;
    for (usize index = 0; index < name_len; ++index) stored[index] = name[index];
    stored[name_len] = 0;
    g_symbol_name_used += name_len + 1u;
    g_symbol_map[g_symbol_map_count].value = value;
    g_symbol_map[g_symbol_map_count].name = stored;
    g_symbol_map_count += 1;
}

static void load_symbol_map(const char *path) {
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize offset = 0;
    int in_symbols = 0;
    char *line;
    if (fd < 0) {
        out("failed to open symbol map\n");
        sys_exit(1);
    }
    while (offset < SYMBOL_MAP_MAX_SIZE) {
        long got = sys_read((int)fd, g_symbol_map_storage + offset, SYMBOL_MAP_MAX_SIZE - offset);
        if (got < 0) {
            (void)sys_close((int)fd);
            out("failed to read symbol map\n");
            sys_exit(1);
        }
        if (got == 0) break;
        offset += (usize)got;
    }
    (void)sys_close((int)fd);
    if (offset >= SYMBOL_MAP_MAX_SIZE) {
        out("symbol map too large\n");
        sys_exit(1);
    }
    g_symbol_map_storage[offset] = 0;
    line = g_symbol_map_storage;
    while (*line != 0) {
        char *end = line;
        while (*end != 0 && *end != '\n') end += 1;
        if (in_symbols) {
            const char *cursor = line;
            u32 value;
            if (parse_map_hex(&cursor, &value)) {
                while (cursor < end && (*cursor == ' ' || *cursor == '\t')) cursor += 1;
                add_symbol_map_entry(value, cursor, end);
            }
        } else if (line_is_symbols_header(line, end)) {
            in_symbols = 1;
        }
        line = *end == '\n' ? end + 1 : end;
    }
}

static const SymbolMapEntry *lookup_symbol(u32 pc) {
    const SymbolMapEntry *best = 0;
    for (int index = 0; index < g_symbol_map_count; ++index) {
        const SymbolMapEntry *entry = g_symbol_map + index;
        if (entry->value == 0u || entry->value > pc) continue;
        if (best == 0 || entry->value > best->value) best = entry;
    }
    return best;
}

static void out_symbol_for_pc(u32 pc) {
    const SymbolMapEntry *entry = lookup_symbol(pc);
    u32 offset;
    if (entry == 0) return;
    offset = pc - entry->value;
    out(" (");
    out(entry->name);
    if (offset != 0u) {
        out("+");
        out_hex(offset);
    }
    out(")");
}

static void out_pc(u32 pc) {
    out_hex(pc);
    out_symbol_for_pc(pc);
}

typedef struct {
    u32 array_len;
    u32 array_hash;
    u32 wifi_fw_len;
    u32 clm_len;
} Cyw43BlobInfo;

static int cyw43_token_number(const char *token, usize len, u32 *out_value) {
    u32 value = 0;
    usize pos = 0;
    int base = 10;
    if (len >= 2u && token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        base = 16;
        pos = 2u;
    }
    if (pos >= len) return 0;
    while (pos < len) {
        int digit = hex_value(token[pos]);
        if (digit < 0 || digit >= base) return 0;
        value = base == 16 ? ((value << 4) | (u32)digit) : (value * 10u + (u32)digit);
        pos += 1u;
    }
    *out_value = value;
    return 1;
}

static const char *cyw43_find_text(const char *line, const char *needle) {
    usize pos = 0;
    while (line[pos] != 0) {
        usize npos = 0;
        while (needle[npos] != 0 && line[pos + npos] == needle[npos]) npos += 1u;
        if (needle[npos] == 0) return line + pos;
        pos += 1u;
    }
    return 0;
}

static int cyw43_parse_define_value(const char *line, const char *name, u32 *out_value) {
    const char *pos = cyw43_find_text(line, name);
    u32 value = 0;
    int any = 0;
    if (pos == 0) return 0;
    pos += str_len(name);
    while (*pos != 0 && (*pos < '0' || *pos > '9')) pos += 1;
    while (*pos >= '0' && *pos <= '9') {
        value = value * 10u + (u32)(*pos - '0');
        any = 1;
        pos += 1;
    }
    if (!any) return 0;
    *out_value = value;
    return 1;
}

static void cyw43_scan_line(const char *line, Cyw43BlobInfo *info) {
    u32 value;
    if (cyw43_parse_define_value(line, "CYW43_WIFI_FW_LEN", &value)) info->wifi_fw_len = value;
    if (cyw43_parse_define_value(line, "CYW43_CLM_LEN", &value)) info->clm_len = value;
}

static int cyw43_scan_header(const char *path, Cyw43BlobInfo *info) {
    char line[256];
    char token[32];
    usize line_len = 0;
    usize token_len = 0;
    int in_array = 0;
    int seen_initializer = 0;
    int in_string = 0;
    int escape_mode = 0;
    u32 escape_value = 0;
    int done_array = 0;
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    if (fd < 0) return -1;
    info->array_len = 0;
    info->array_hash = 2166136261u;
    info->wifi_fw_len = 0;
    info->clm_len = 0;
    while (1) {
        char ch;
        long got = sys_read((int)fd, &ch, 1u);
        if (got < 0) { (void)sys_close((int)fd); return -1; }
        if (got == 0) break;
        if (line_len + 1u < sizeof(line)) line[line_len++] = ch;
        if (ch == '\n') {
            line[line_len] = 0;
            cyw43_scan_line(line, info);
            line_len = 0;
        }
        if (!done_array) {
            if (!in_array) {
                if (ch == '{') in_array = 1;
                else if (ch == '=') seen_initializer = 1;
                else if (seen_initializer && !in_string && ch == '"') in_string = 1;
                else if (seen_initializer && !in_string && ch == ';' && info->array_len != 0u) done_array = 1;
                else if (in_string) {
                    if (escape_mode == 0) {
                        if (ch == '\\') escape_mode = 1;
                        else if (ch == '"') in_string = 0;
                        else {
                            info->array_hash = (info->array_hash ^ ((u32)(u8)ch)) * 16777619u;
                            info->array_len += 1u;
                        }
                    } else if (escape_mode == 1) {
                        if (ch == 'x' || ch == 'X') {
                            escape_mode = 2;
                            escape_value = 0;
                        } else {
                            info->array_hash = (info->array_hash ^ ((u32)(u8)ch)) * 16777619u;
                            info->array_len += 1u;
                            escape_mode = 0;
                        }
                    } else if (escape_mode == 2) {
                        int digit = hex_value(ch);
                        if (digit >= 0) {
                            escape_value = (u32)digit << 4;
                            escape_mode = 3;
                        } else {
                            escape_mode = 0;
                        }
                    } else {
                        int digit = hex_value(ch);
                        if (digit >= 0) escape_value |= (u32)digit;
                        info->array_hash = (info->array_hash ^ (escape_value & 0xffu)) * 16777619u;
                        info->array_len += 1u;
                        escape_mode = 0;
                    }
                }
            } else if (ch == '}') {
                if (token_len != 0u) {
                    u32 value;
                    if (cyw43_token_number(token, token_len, &value)) {
                        info->array_hash = (info->array_hash ^ (value & 0xffu)) * 16777619u;
                        info->array_len += 1u;
                    }
                    token_len = 0;
                }
                done_array = 1;
            } else if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F') || ch == 'x' || ch == 'X') {
                if (token_len + 1u < sizeof(token)) token[token_len++] = ch;
            } else if (token_len != 0u) {
                u32 value;
                if (cyw43_token_number(token, token_len, &value)) {
                    info->array_hash = (info->array_hash ^ (value & 0xffu)) * 16777619u;
                    info->array_len += 1u;
                }
                token_len = 0;
            }
        }
    }
    if (line_len != 0u) {
        line[line_len] = 0;
        cyw43_scan_line(line, info);
    }
    (void)sys_close((int)fd);
    return done_array ? 0 : -1;
}

static void cyw43_report_blob(const char *label, const char *path) {
    Cyw43BlobInfo info;
    if (path == 0) return;
    if (cyw43_scan_header(path, &info) != 0) {
        out("cyw43 blob "); out(label); out(" path="); out(path); out(" error=read-or-parse\n");
        return;
    }
    out("cyw43 blob "); out(label); out(" path="); out(path);
    out(" bytes="); out_hex(info.array_len);
    out(" hash="); out_hex(info.array_hash);
    if (info.wifi_fw_len != 0u) { out(" wifi-fw-len="); out_hex(info.wifi_fw_len); }
    if (info.clm_len != 0u) { out(" clm-len="); out_hex(info.clm_len); }
    out("\n");
    if (trace_enabled(TRACE_CYW43)) {
        trace_text("cyw43-blob label="); trace_text(label);
        trace_text(" bytes="); trace_hex32(info.array_len);
        trace_text(" hash="); trace_hex32(info.array_hash);
        trace_text(" wifi_fw_len="); trace_hex32(info.wifi_fw_len);
        trace_text(" clm_len="); trace_hex32(info.clm_len);
        trace_text("\n");
    }
}

static void cyw43_inventory_report(void) {
    cyw43_report_blob("wifi", g_cyw43_wifi_fw_path);
    cyw43_report_blob("bt", g_cyw43_bt_fw_path);
    cyw43_report_blob("nvram", g_cyw43_nvram_path);
}

static int cyw43_trace_active(void) {
    return g_cyw43_model || trace_enabled(TRACE_CYW43);
}

static int cyw43_pio_fifo_addr(u32 addr) {
    u32 off;
    if (addr >= PIO0_BASE && addr < PIO0_BASE + PIO_SIZE) off = addr - PIO0_BASE;
    else if (addr >= PIO1_BASE && addr < PIO1_BASE + PIO_SIZE) off = addr - PIO1_BASE;
    else return 0;
    return (off >= PIO_TXF0_OFF && off <= PIO_TXF3_OFF) || (off >= PIO_RXF0_OFF && off <= PIO_RXF3_OFF);
}

static void cyw43_trace_pio(const char *kind, u32 addr, u32 value) {
    u32 pio = addr >= PIO1_BASE ? 1u : 0u;
    u32 off = addr - (pio != 0u ? PIO1_BASE : PIO0_BASE);
    if (!trace_enabled(TRACE_CYW43)) return;
    trace_text("cyw43-"); trace_text(kind);
    trace_text(" pio="); trace_hex32(pio);
    trace_text(" off="); trace_hex32(off);
    trace_text(" value="); trace_hex32(value);
    trace_text(" pc="); trace_hex32(g_current_pc);
    trace_text(" cycles="); trace_hex32(g_cycles);
    trace_text("\n");
}

static void cyw43_trace_gpio(const char *kind, u32 mask, u32 state) {
    static const u32 cyw43_mask = (1u << CYW43_PIN_WL_REG_ON) | (1u << CYW43_PIN_WL_DATA) | (1u << CYW43_PIN_WL_CS) | (1u << CYW43_PIN_WL_CLOCK);
    if ((mask & cyw43_mask) == 0u) return;
    g_cyw43_gpio_events += 1u;
    if (!trace_enabled(TRACE_CYW43)) return;
    trace_text("cyw43-"); trace_text(kind);
    trace_text(" mask="); trace_hex32(mask & cyw43_mask);
    trace_text(" state="); trace_hex32(state & cyw43_mask);
    trace_text(" pc="); trace_hex32(g_current_pc);
    trace_text(" cycles="); trace_hex32(g_cycles);
    trace_text("\n");
}

static void report_lcd_milestones(void) {
    if (!g_report_milestones) return;
    out("lcd milestones:");
    if (g_seen_lcd_command) {
        out(" first-cmd="); out_hex(g_first_lcd_command);
        out(" pc="); out_pc(g_first_lcd_command_pc);
        out(" cycles="); out_hex(g_first_lcd_command_cycles);
    } else {
        out(" first-cmd=none");
    }
    if (g_seen_lcd_pixel) {
        out(" first-pixel x="); out_hex(g_first_lcd_pixel_x);
        out(" y="); out_hex(g_first_lcd_pixel_y);
        out(" rgb="); out_hex(g_first_lcd_pixel_rgb);
        out(" pc="); out_pc(g_first_lcd_pixel_pc);
        out(" cycles="); out_hex(g_first_lcd_pixel_cycles);
    } else {
        out(" first-pixel=none");
    }
    if (g_seen_lcd_nonblack) {
        out(" first-nonblack x="); out_hex(g_first_lcd_nonblack_x);
        out(" y="); out_hex(g_first_lcd_nonblack_y);
        out(" rgb="); out_hex(g_first_lcd_nonblack_rgb);
        out(" pc="); out_pc(g_first_lcd_nonblack_pc);
        out(" cycles="); out_hex(g_first_lcd_nonblack_cycles);
    } else {
        out(" first-nonblack=none");
    }
    out("\n");
}

static void report_pc_window(u32 pc) {
    out(" pc-window=");
    out_hex(mem_read16(pc));
    out(",");
    out_hex(mem_read16(pc + 2u));
    out(",");
    out_hex(mem_read16(pc + 4u));
    out(",");
    out_hex(mem_read16(pc + 6u));
    out(",");
    out_hex(mem_read16(pc + 8u));
    out(",");
    out_hex(mem_read16(pc + 10u));
}

static void terminal_enter_live(void) {
    Termios raw;
    long flags;
    if (!g_live_terminal) return;
    if (sys_ioctl(0, TCGETS, &g_saved_termios) == 0) {
        raw = g_saved_termios;
        raw.c_lflag &= ~(TERM_ICANON | TERM_ECHO);
        raw.c_cc[TERM_VMIN] = 0;
        raw.c_cc[TERM_VTIME] = 0;
        if (sys_ioctl(0, TCSETS, &raw) == 0) g_saved_termios_valid = 1;
    }
    flags = sys_fcntl(0, F_GETFL, 0);
    if (flags >= 0) (void)sys_fcntl(0, F_SETFL, flags | O_NONBLOCK);
    (void)sys_write(1, "\033[?25l\033[2J\033[H", sizeof("\033[?25l\033[2J\033[H") - 1u);
}

static void terminal_leave_live(void) {
    if (!g_live_terminal) return;
    if (g_saved_termios_valid) (void)sys_ioctl(0, TCSETS, &g_saved_termios);
    (void)sys_write(1, "\033[0m\033[?25h\n", sizeof("\033[0m\033[?25h\n") - 1u);
}

static void terminal_render_live(void) {
    usize pos = 0;
    int row;
    int col;
    if (!g_live_terminal) return;
    append_text(g_live_buffer, &pos, sizeof(g_live_buffer), "\033[H");
    for (row = 0; row < LIVE_ROWS; ++row) {
        for (col = 0; col < LIVE_COLS; ++col) {
            u32 red = 0;
            u32 green = 0;
            u32 blue = 0;
            int y;
            int x;
            for (y = 0; y < LCD_H / LIVE_ROWS; ++y) {
                for (x = 0; x < LCD_W / LIVE_COLS; ++x) {
                    usize off = (((usize)row * (LCD_H / LIVE_ROWS) + (usize)y) * LCD_W +
                                 (usize)col * (LCD_W / LIVE_COLS) + (usize)x) * 3u;
                    red += g_framebuffer[off];
                    green += g_framebuffer[off + 1u];
                    blue += g_framebuffer[off + 2u];
                }
            }
            red /= (LCD_W / LIVE_COLS) * (LCD_H / LIVE_ROWS);
            green /= (LCD_W / LIVE_COLS) * (LCD_H / LIVE_ROWS);
            blue /= (LCD_W / LIVE_COLS) * (LCD_H / LIVE_ROWS);
            append_text(g_live_buffer, &pos, sizeof(g_live_buffer), "\033[48;2;");
            append_uint(g_live_buffer, &pos, sizeof(g_live_buffer), red);
            append_char(g_live_buffer, &pos, sizeof(g_live_buffer), ';');
            append_uint(g_live_buffer, &pos, sizeof(g_live_buffer), green);
            append_char(g_live_buffer, &pos, sizeof(g_live_buffer), ';');
            append_uint(g_live_buffer, &pos, sizeof(g_live_buffer), blue);
            append_text(g_live_buffer, &pos, sizeof(g_live_buffer), "m ");
        }
        append_text(g_live_buffer, &pos, sizeof(g_live_buffer), "\033[0m\n");
    }
    append_text(g_live_buffer, &pos, sizeof(g_live_buffer), "hash=");
    append_uint(g_live_buffer, &pos, sizeof(g_live_buffer), framebuffer_hash());
    append_text(g_live_buffer, &pos, sizeof(g_live_buffer), " steps update\n");
    (void)sys_write(1, g_live_buffer, pos);
}

static int load_key_file(const char *path) {
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize off = 0;
    if (fd < 0) return -1;
    while (off + 1u < sizeof(g_key_file)) {
        long got = sys_read((int)fd, g_key_file + off, sizeof(g_key_file) - off - 1u);
        if (got < 0) return -1;
        if (got == 0) break;
        off += (usize)got;
    }
    g_key_file[off] = 0;
    return 0;
}

static int next_script_key(void) {
    char ch;
    if (g_live_stdin) {
        long got = sys_read(0, &ch, 1u);
        if (got == 1) return (unsigned char)ch;
        return -1;
    }
    if (g_key_script == 0 || g_key_script[g_key_script_pos] == 0) return -1;
    ch = g_key_script[g_key_script_pos++];
    if (ch != '\\') return (unsigned char)ch;
    ch = g_key_script[g_key_script_pos++];
    if (ch == 0) return '\\';
    if (ch == 'n') return '\n';
    if (ch == 'r') return '\r';
    if (ch == 't') return '\t';
    if (ch == 'b') return 8;
    if (ch == '\\') return '\\';
    if (ch == 'x') {
        int hi = hex_value(g_key_script[g_key_script_pos]);
        int lo;
        if (hi < 0) return 'x';
        g_key_script_pos += 1u;
        lo = hex_value(g_key_script[g_key_script_pos]);
        if (lo < 0) return hi;
        g_key_script_pos += 1u;
        return (hi << 4) | lo;
    }
    return (unsigned char)ch;
}

static int register_bank_read(u32 addr, u32 base, u32 size, u32 *bank, u32 *out_value) {
    if (addr < base || addr >= base + size) return 0;
    if ((addr & 3u) != 0u) { *out_value = 0; return 1; }
    *out_value = bank[(addr - base) >> 2];
    return 1;
}

static int register_bank_write(u32 addr, u32 value, u32 base, u32 size, u32 *bank) {
    u32 alias;
    u32 off;
    if (addr < base || addr >= base + 0x4000u) return 0;
    alias = (addr - base) & 0x3000u;
    off = (addr - base) & 0x0fffu;
    if (off >= size) return 0;
    if ((off & 3u) == 0u) {
        u32 *slot = bank + (off >> 2);
        if (alias == 0x1000u) *slot ^= value;
        else if (alias == 0x2000u) *slot |= value;
        else if (alias == 0x3000u) *slot &= ~value;
        else *slot = value;
    }
    return 1;
}

static int clock_read32(u32 addr, u32 *out_value) {
    u32 off;
    if (addr >= CLOCKS_BASE && addr < CLOCKS_BASE + CLOCKS_SIZE) {
        off = addr - CLOCKS_BASE;
        if ((off & 3u) != 0u) { *out_value = 0; return 1; }
        if (off == CLK_REF_SELECTED || off == CLK_SYS_SELECTED || off == CLK_PERI_SELECTED ||
            off == CLK_USB_SELECTED || off == CLK_ADC_SELECTED || off == CLK_RTC_SELECTED) {
            u32 ctrl = g_clocks[(off - 8u) >> 2];
            *out_value = 1u << (ctrl & 0x1fu);
            return 1;
        }
        *out_value = g_clocks[off >> 2];
        return 1;
    }
    if (addr >= XOSC_BASE && addr < XOSC_BASE + XOSC_SIZE) {
        off = addr - XOSC_BASE;
        if ((off & 3u) != 0u) { *out_value = 0; return 1; }
        *out_value = g_xosc[off >> 2] | (off == XOSC_STATUS ? XOSC_STATUS_STABLE : 0u);
        return 1;
    }
    if (addr >= PLL_SYS_BASE && addr < PLL_SYS_BASE + PLL_SIZE) {
        off = addr - PLL_SYS_BASE;
        if ((off & 3u) != 0u) { *out_value = 0; return 1; }
        *out_value = g_pll_sys[off >> 2] | (off == 0u ? PLL_CS_LOCK : 0u);
        return 1;
    }
    if (addr >= PLL_USB_BASE && addr < PLL_USB_BASE + PLL_SIZE) {
        off = addr - PLL_USB_BASE;
        if ((off & 3u) != 0u) { *out_value = 0; return 1; }
        *out_value = g_pll_usb[off >> 2] | (off == 0u ? PLL_CS_LOCK : 0u);
        return 1;
    }
    return 0;
}

static int clock_write32(u32 addr, u32 value) {
    if (register_bank_write(addr, value, CLOCKS_BASE, CLOCKS_SIZE, g_clocks)) return 1;
    if (register_bank_write(addr, value, XOSC_BASE, XOSC_SIZE, g_xosc)) return 1;
    if (register_bank_write(addr, value, PLL_SYS_BASE, PLL_SIZE, g_pll_sys)) return 1;
    if (register_bank_write(addr, value, PLL_USB_BASE, PLL_SIZE, g_pll_usb)) return 1;
    return 0;
}

static u32 xip_dr0_read(void) {
    static const u8 unique_id[8] = { 0x4du, 0x53u, 0x50u, 0x43u, 0x41u, 0x4cu, 0x43u, 0x01u };
    u32 value = 0;
    if (g_xip_dr0_command == 0x4bu && g_xip_dr0_reads >= 5u && g_xip_dr0_reads < 13u) {
        value = unique_id[g_xip_dr0_reads - 5u];
    }
    g_xip_dr0_reads += 1u;
    trace_xip_mmio("xipr", XIP_SSI_DR0, value);
    return value;
}

static void xip_dr0_write(u32 value) {
    if ((value & 0xffu) == 0x4bu) {
        g_xip_dr0_command = 0x4bu;
        g_xip_dr0_reads = 0;
    }
    trace_xip_mmio("xipw", XIP_SSI_DR0, value);
}

static u32 timer_us_low(void) {
    return g_cycles / TIMER_CYCLES_PER_US;
}

static u32 timer_us_high(void) {
    return 0u;
}

static void spi_rx_push(u8 byte) {
    if (g_spi.rx_count < SPI_FIFO_SIZE) {
        g_spi.rx[(g_spi.rx_head + g_spi.rx_count) & (SPI_FIFO_SIZE - 1)] = byte;
        g_spi.rx_count += 1;
    } else {
        g_spi.overrun = 1;
    }
    g_spi.last_rx = byte;
}

static void spi_service(int force) {
    while (g_spi.tx_count > 0 && (force || g_cycles >= g_spi.busy_until)) {
        u8 byte = g_spi.tx[g_spi.tx_head & (SPI_FIFO_SIZE - 1)];
        g_spi.tx_head = (g_spi.tx_head + 1) & (SPI_FIFO_SIZE - 1);
        g_spi.tx_count -= 1;
        if (g_spi.busy_until < g_cycles) g_spi.busy_until = g_cycles;
        g_spi.busy_until += SPI_BYTE_CYCLES;
        lcd_spi_byte(byte);
        spi_rx_push(byte);
        if (!force) break;
    }
}

static void spi_write_data(u8 byte) {
    spi_service(0);
    if (g_spi.tx_count < SPI_FIFO_SIZE) {
        g_spi.tx[(g_spi.tx_head + g_spi.tx_count) & (SPI_FIFO_SIZE - 1)] = byte;
        g_spi.tx_count += 1;
    } else {
        g_spi.overrun = 1;
    }
}

static u32 spi_status(void) {
    u32 status = 0;
    spi_service(0);
    if (g_spi.tx_count == 0) status |= SPI_SR_TFE;
    if (g_spi.tx_count < SPI_FIFO_SIZE) status |= SPI_SR_TNF;
    if (g_spi.rx_count > 0) status |= SPI_SR_RNE;
    if (g_spi.tx_count > 0 || g_spi.busy_until > g_cycles) status |= SPI_SR_BSY;
    return status;
}

static u32 spi_read_data(void) {
    u8 byte;
    spi_service(0);
    if (g_spi.rx_count <= 0) return g_spi.last_rx;
    byte = g_spi.rx[g_spi.rx_head & (SPI_FIFO_SIZE - 1)];
    g_spi.rx_head = (g_spi.rx_head + 1) & (SPI_FIFO_SIZE - 1);
    g_spi.rx_count -= 1;
    return byte;
}

static int spi_reg_offset(u32 addr, u32 *out_reg, u32 *out_alias) {
    if (addr >= SPI0_BASE && addr < SPI0_BASE + 0x4000u) {
        *out_reg = (addr - SPI0_BASE) & 0x0fffu;
        *out_alias = (addr - SPI0_BASE) & 0x3000u;
        return 1;
    }
    if (addr >= SPI1_BASE && addr < SPI1_BASE + 0x4000u) {
        *out_reg = (addr - SPI1_BASE) & 0x0fffu;
        *out_alias = (addr - SPI1_BASE) & 0x3000u;
        return 1;
    }
    return 0;
}

static u32 atomic_alias_value(u32 old_value, u32 value, u32 alias) {
    if (alias == 0x1000u) return old_value ^ value;
    if (alias == 0x2000u) return old_value | value;
    if (alias == 0x3000u) return old_value & ~value;
    return value;
}

static void i2c_rx_push(u8 byte) {
    if (g_i2c.rx_count < I2C_FIFO_SIZE) {
        g_i2c.rx[(g_i2c.rx_head + g_i2c.rx_count) & (I2C_FIFO_SIZE - 1)] = byte;
        g_i2c.rx_count += 1;
    } else {
        g_i2c.tx_abrt |= 1u;
        g_i2c.raw_intr |= I2C_RAW_TX_ABRT;
    }
}

static void i2c_finish_command(u32 value) {
    if ((value & I2C_DATA_CMD_STOP) != 0u) g_i2c.raw_intr |= I2C_RAW_STOP_DET;
    if (g_i2c.tx_level > 0) g_i2c.tx_level -= 1;
    g_i2c.raw_intr |= I2C_RAW_TX_EMPTY;
    g_i2c.busy_until = g_cycles + 200u;
}

static void dma_start_channel(int ch) {
    DmaChannel *dma = &g_dma[ch];
    u32 read_addr = dma->read_addr;
    u32 write_addr = dma->write_addr;
    u32 count = dma->transfer_count;
    u32 ctrl = dma->ctrl_trig;
    u32 data_size = (ctrl & DMA_CTRL_DATA_SIZE_MASK) >> 2;
    u32 dreq = (ctrl & DMA_CTRL_DREQ_MASK) >> DMA_CTRL_DREQ_SHIFT;
    u32 width = data_size == 0u ? 1u : (data_size == 1u ? 2u : 4u);
    u32 limit = count;
    if ((ctrl & DMA_CTRL_EN) == 0u) return;
    if (cyw43_trace_active() && (cyw43_pio_fifo_addr(read_addr) || cyw43_pio_fifo_addr(write_addr))) {
        g_cyw43_dma_events += 1u;
        if (trace_enabled(TRACE_CYW43)) {
            trace_text("cyw43-dma ch="); trace_hex32((u32)ch);
            trace_text(" read="); trace_hex32(read_addr);
            trace_text(" write="); trace_hex32(write_addr);
            trace_text(" count="); trace_hex32(count);
            trace_text(" dreq="); trace_hex32(dreq);
            trace_text(" width="); trace_hex32(width);
            trace_text(" pc="); trace_hex32(g_current_pc);
            trace_text("\n");
        }
    }
    if (limit > 0x100000u) limit = 0x100000u;
    dma->ctrl_trig |= DMA_CTRL_BUSY;
    while (limit > 0u) {
        u32 value;
        if ((write_addr == SPI_SSPDR && dreq == DREQ_SPI1_TX) || (write_addr == SPI0_SSPDR && dreq == DREQ_SPI0_TX)) {
            while (g_spi.tx_count >= SPI_FIFO_SIZE) {
                g_cycles += 1u;
                spi_service(0);
            }
        }
        if (width == 1u) value = mem_read8(read_addr);
        else if (width == 2u) value = mem_read16(read_addr);
        else value = mem_read32(read_addr);
        if (width == 1u) mem_write8(write_addr, value);
        else if (width == 2u) mem_write16(write_addr, value);
        else mem_write32(write_addr, value);
        if ((ctrl & DMA_CTRL_INCR_READ) != 0u) read_addr += width;
        if ((ctrl & DMA_CTRL_INCR_WRITE) != 0u) write_addr += width;
        limit -= 1u;
        g_cycles += 1u;
        if ((write_addr == SPI_SSPDR && dreq == DREQ_SPI1_TX) || (write_addr == SPI0_SSPDR && dreq == DREQ_SPI0_TX)) spi_service(0);
    }
    dma->read_addr = read_addr;
    dma->write_addr = write_addr;
    dma->transfer_count = count - (count > 0x100000u ? 0x100000u : count);
    dma->ctrl_trig &= ~(DMA_CTRL_BUSY | DMA_CTRL_EN);
    trace_text("dma ch="); trace_hex32((u32)ch); trace_text(" count="); trace_hex32(count); trace_text("\n");
}

static int dma_read32(u32 addr, u32 *out_value) {
    u32 off;
    int ch;
    u32 reg;
    if (addr < DMA_BASE || addr >= DMA_BASE + 12u * DMA_CH_STRIDE) return 0;
    off = addr - DMA_BASE;
    ch = (int)(off / DMA_CH_STRIDE);
    reg = off & (DMA_CH_STRIDE - 1u);
    if (reg == DMA_READ_ADDR) *out_value = g_dma[ch].read_addr;
    else if (reg == DMA_WRITE_ADDR) *out_value = g_dma[ch].write_addr;
    else if (reg == DMA_TRANS_COUNT) *out_value = g_dma[ch].transfer_count;
    else if (reg == DMA_CTRL_TRIG) *out_value = g_dma[ch].ctrl_trig;
    else *out_value = 0;
    return 1;
}

static int dma_write32(u32 addr, u32 value) {
    u32 off;
    int ch;
    u32 reg;
    if (addr < DMA_BASE || addr >= DMA_BASE + 12u * DMA_CH_STRIDE) return 0;
    off = addr - DMA_BASE;
    ch = (int)(off / DMA_CH_STRIDE);
    reg = off & (DMA_CH_STRIDE - 1u);
    if (reg == DMA_READ_ADDR) g_dma[ch].read_addr = value;
    else if (reg == DMA_WRITE_ADDR) g_dma[ch].write_addr = value;
    else if (reg == DMA_TRANS_COUNT) g_dma[ch].transfer_count = value;
    else if (reg == DMA_CTRL_TRIG) { g_dma[ch].ctrl_trig = value; dma_start_channel(ch); }
    else return 1;
    trace_mmio("dmaw", addr, value);
    return 1;
}

static int pio_read32(u32 addr, u32 *out_value) {
    u32 base;
    u32 *bank;
    u32 off;
    if (addr >= PIO0_BASE && addr < PIO0_BASE + PIO_SIZE) {
        base = PIO0_BASE;
        bank = g_pio0;
    } else if (addr >= PIO1_BASE && addr < PIO1_BASE + PIO_SIZE) {
        base = PIO1_BASE;
        bank = g_pio1;
    } else {
        return 0;
    }
    off = addr - base;
    if (off == PIO_FSTAT_OFF) {
        *out_value = g_cyw43_model ? PIO_FSTAT_TXEMPTY_MASK : (PIO_FSTAT_RXEMPTY_MASK | PIO_FSTAT_TXEMPTY_MASK);
        cyw43_trace_pio("pio-status", addr, *out_value);
        return 1;
    }
    if (g_cyw43_model && off >= PIO_RXF0_OFF && off <= PIO_RXF3_OFF) {
        *out_value = 0xffffffffu;
        g_cyw43_pio_rx_words += 1u;
        cyw43_trace_pio("pio-rx", addr, *out_value);
        return 1;
    }
    *out_value = bank[off >> 2];
    cyw43_trace_pio("pior", addr, *out_value);
    return 1;
}

static int pio_write32(u32 addr, u32 value) {
    u32 base;
    u32 *bank;
    u32 off;
    if (addr >= PIO0_BASE && addr < PIO0_BASE + PIO_SIZE) {
        base = PIO0_BASE;
        bank = g_pio0;
    } else if (addr >= PIO1_BASE && addr < PIO1_BASE + PIO_SIZE) {
        base = PIO1_BASE;
        bank = g_pio1;
    } else {
        return 0;
    }
    off = addr - base;
    bank[off >> 2] = value;
    if (off >= PIO_TXF0_OFF && off <= PIO_TXF3_OFF) g_cyw43_pio_tx_words += 1u;
    cyw43_trace_pio(off >= PIO_TXF0_OFF && off <= PIO_TXF3_OFF ? "pio-tx" : "piow", addr, value);
    return 1;
}

static u32 mmio_read32(u32 addr) {
    u32 value;
    u32 spi_reg;
    u32 spi_alias;
    u32 i2c_addr = I2C1_BASE + ((addr - I2C1_BASE) & 0x0fffu);
    if (dma_read32(addr, &value)) return value;
    if (pio_read32(addr, &value)) return value;
    if (clock_read32(addr, &value)) return value;
    if (register_bank_read(addr, IO_BANK0_BASE, IO_BANK0_SIZE, g_io_bank0, &value)) return value;
    if (register_bank_read(addr, PADS_BANK0_BASE, PADS_BANK0_SIZE, g_pads_bank0, &value)) return value;
    if (register_bank_read(addr, RTC_BASE, RTC_SIZE, g_rtc, &value)) {
        if (addr == RTC_BASE + RTC_CTRL && (value & 1u)) value |= RTC_CTRL_ACTIVE;
        return value;
    }
    if (addr >= BUSCTRL_BASE && addr < BUSCTRL_BASE + BUSCTRL_SIZE) {
        u32 off = addr - BUSCTRL_BASE;
        if (off == BUSCTRL_PRIORITY_ACK) return 1u;
        if (off == BUSCTRL_PERFSEL0 || off == BUSCTRL_PERFSEL1 || off == BUSCTRL_PERFSEL2 || off == BUSCTRL_PERFSEL3) return 0x1fu;
        return 0u;
    }
    if (addr == ROSC_STATUS) return ROSC_STATUS_STABLE | ROSC_STATUS_DIV_RUNNING | ROSC_STATUS_ENABLED;
    if (addr == ROSC_RANDOMBIT) return (g_cycles >> 3) & 1u;
    if (addr == PPB_SYST_CSR) return g_core.syst_csr;
    if (addr == PPB_SYST_RVR) return g_core.syst_rvr;
    if (addr == PPB_SYST_CVR) return g_core.syst_cvr;
    if (addr == PPB_NVIC_ISER) return g_core.nvic_enable;
    if (addr == PPB_NVIC_ISPR) return g_core.nvic_pending;
    if (addr == PPB_SCB_ICSR) return g_core.icsr | (g_core.nvic_pending != 0u ? (1u << 22) : 0u);
    if (addr == PPB_SCB_VTOR) return g_core.vtor;
    if (addr == TIMERAWH) return timer_us_high();
    if (addr == TIMERAWL) return timer_us_low();
    if (addr == XIP_SSI_SR) { trace_xip_mmio("xipr", addr, 0x0eu); return 0x0eu; }
    if (addr == XIP_SSI_DR0) return xip_dr0_read();
    if (addr == SIO_CPUID) return 0u;
    if (addr == SIO_DIV_UDIVIDEND || addr == SIO_DIV_SDIVIDEND) return g_sio_dividend;
    if (addr == SIO_DIV_UDIVISOR || addr == SIO_DIV_SDIVISOR) return g_sio_divisor;
    if (addr == SIO_DIV_QUOTIENT) return g_sio_quotient;
    if (addr == SIO_DIV_REMAINDER) return g_sio_remainder;
    if (addr == SIO_DIV_CSR) return SIO_DIV_CSR_READY | (g_sio_div_dirty ? SIO_DIV_CSR_DIRTY : 0u);
    if (addr >= SIO_SPINLOCK_BASE && addr < SIO_SPINLOCK_END && (addr & 3u) == 0u) return 1u;
    if (addr == RESETS_RESET) return g_resets_reset;
    if (addr == RESETS_DONE) return RESET_DONE_MASK & ~g_resets_reset;
    if (spi_reg_offset(addr, &spi_reg, &spi_alias)) {
        if (spi_reg == SPI_SSPCR0_OFF) return g_spi.cr0;
        if (spi_reg == SPI_SSPCR1_OFF) return g_spi.cr1;
        if (spi_reg == SPI_SSPDR_OFF) return spi_read_data();
        if (spi_reg == SPI_SSPSR_OFF) return spi_status();
        if (spi_reg == SPI_SSPCPSR_OFF) return g_spi.cpsr;
        if (spi_reg == SPI_SSPDMACR_OFF) return g_spi.dmacr;
    }
    if (register_bank_read(addr, UART0_BASE, UART_SIZE, g_uart0, &value)) {
        if ((addr & 0x0fffu) == UART_FR) return UART_FR_RXFE | UART_FR_TXFE;
        if ((addr & 0x0fffu) == UART_DR) return 0u;
        return value;
    }
    if (addr >= I2C1_BASE && addr < I2C1_BASE + 0x4000u) addr = i2c_addr;
    if (addr == I2C_IC_CON) return g_i2c.con;
    if (addr == I2C_IC_TAR) return g_i2c.tar;
    if (addr == I2C_IC_DATA_CMD) {
        u8 value;
        if (g_i2c.rx_count <= 0) return 0;
        value = g_i2c.rx[g_i2c.rx_head & 7];
        g_i2c.rx_head = (g_i2c.rx_head + 1) & 7;
        g_i2c.rx_count -= 1;
        return value;
    }
    if (addr == I2C_IC_FS_HCNT) return g_i2c.fs_hcnt;
    if (addr == I2C_IC_FS_LCNT) return g_i2c.fs_lcnt;
    if (addr == I2C_IC_RX_TL) return g_i2c.rx_tl;
    if (addr == I2C_IC_TX_TL) return g_i2c.tx_tl;
    if (addr == I2C_IC_RAW_INTR) return g_i2c.raw_intr | I2C_RAW_TX_EMPTY;
    if (addr == I2C_IC_CLR_TX_ABRT) { g_i2c.raw_intr &= ~I2C_RAW_TX_ABRT; return 0; }
    if (addr == I2C_IC_CLR_STOP) { g_i2c.raw_intr &= ~I2C_RAW_STOP_DET; return 0; }
    if (addr == I2C_IC_ENABLE) return g_i2c.enable;
    if (addr == I2C_IC_TXFLR) return (u32)g_i2c.tx_level;
    if (addr == I2C_IC_RXFLR) return (u32)g_i2c.rx_count;
    if (addr == I2C_IC_ENABLE_STATUS) return g_i2c.enable & 1u;
    if (addr == I2C_IC_STATUS) return (g_i2c.tx_level == 0 ? I2C_STATUS_TFE : 0u) |
        (g_i2c.tx_level < I2C_FIFO_SIZE ? I2C_STATUS_TFNF : 0u) |
        (g_i2c.rx_count > 0 ? I2C_STATUS_RFNE : 0u);
    if (addr == I2C_IC_SDA_HOLD) return g_i2c.sda_hold;
    if (addr == I2C_IC_TX_ABRT) return g_i2c.tx_abrt;
    if (addr == I2C_IC_DMA_CR) return g_i2c.dma_cr;
    if (addr == I2C_IC_FS_SPKLEN) return g_i2c.fs_spklen;
    trace_unknown_mmio("mmior", addr, 0u, 0u);
    return 0u;
}

static void sio_div_update(void) {
    g_sio_div_dirty = 0;
    if (g_sio_divisor == 0u) {
        g_sio_quotient = 0xffffffffu;
        g_sio_remainder = g_sio_dividend;
    } else if (g_sio_div_signed) {
        s32 dividend = (s32)g_sio_dividend;
        s32 divisor = (s32)g_sio_divisor;
        if (dividend == (s32)0x80000000u && divisor == -1) {
            g_sio_quotient = 0x80000000u;
            g_sio_remainder = 0u;
        } else {
            g_sio_quotient = (u32)(dividend / divisor);
            g_sio_remainder = (u32)(dividend % divisor);
        }
    } else {
        g_sio_quotient = g_sio_dividend / g_sio_divisor;
        g_sio_remainder = g_sio_dividend % g_sio_divisor;
    }
}

static void mmio_write(u32 addr, u32 value, u32 width) {
    u32 spi_reg;
    u32 spi_alias;
    u32 i2c_alias = (addr - I2C1_BASE) & 0x3000u;
    u32 i2c_addr = I2C1_BASE + ((addr - I2C1_BASE) & 0x0fffu);
    if (dma_write32(addr, value)) return;
    if (pio_write32(addr, value)) return;
    if (clock_write32(addr, value)) { trace_mmio("clkw", addr, value); return; }
    if (register_bank_write(addr, value, IO_BANK0_BASE, IO_BANK0_SIZE, g_io_bank0)) return;
    if (register_bank_write(addr, value, PADS_BANK0_BASE, PADS_BANK0_SIZE, g_pads_bank0)) return;
    if (register_bank_write(addr, value, RTC_BASE, RTC_SIZE, g_rtc)) return;
    if (addr == PPB_SYST_CSR) { g_core.syst_csr = value & (SYST_CSR_ENABLE | SYST_CSR_TICKINT | SYST_CSR_CLKSOURCE); return; }
    if (addr == PPB_SYST_RVR) { g_core.syst_rvr = value & 0x00ffffffu; return; }
    if (addr == PPB_SYST_CVR) { g_core.syst_cvr = 0; g_core.syst_csr &= ~SYST_CSR_COUNTFLAG; return; }
    if (addr == PPB_NVIC_ISER) { g_core.nvic_enable |= value; return; }
    if (addr == PPB_NVIC_ICER) { g_core.nvic_enable &= ~value; return; }
    if (addr == PPB_NVIC_ISPR) { g_core.nvic_pending |= value; return; }
    if (addr == PPB_NVIC_ICPR) { g_core.nvic_pending &= ~value; return; }
    if (addr == PPB_SCB_ICSR) { g_core.icsr = value; return; }
    if (addr == PPB_SCB_VTOR) { g_core.vtor = value & 0xffffff80u; return; }
    if (addr == RESETS_RESET) { g_resets_reset = value & RESET_DONE_MASK; trace_mmio("rstw", addr, value); return; }
    if (addr == RESETS_RESET_XOR) { g_resets_reset ^= value & RESET_DONE_MASK; trace_mmio("rstw", addr, value); return; }
    if (addr == RESETS_RESET_SET) { g_resets_reset |= value & RESET_DONE_MASK; trace_mmio("rstw", addr, value); return; }
    if (addr == RESETS_RESET_CLR) { g_resets_reset &= ~(value & RESET_DONE_MASK); trace_mmio("rstw", addr, value); return; }
    if (addr == SIO_GPIO_OUT_SET) {
        g_gpio_out |= value;
        cyw43_trace_gpio("gpio-set", value, g_gpio_out);
        lcd_gpio_sync();
        return;
    }
    if (addr == SIO_GPIO_OUT_CLR) {
        g_gpio_out &= ~value;
        cyw43_trace_gpio("gpio-clr", value, g_gpio_out);
        lcd_gpio_sync();
        return;
    }
    if (addr == SIO_GPIO_OE_SET) { g_gpio_oe |= value; cyw43_trace_gpio("gpio-oe-set", value, g_gpio_oe); return; }
    if (addr == SIO_GPIO_OE_CLR) { g_gpio_oe &= ~value; cyw43_trace_gpio("gpio-oe-clr", value, g_gpio_oe); return; }
    if (addr == SIO_DIV_UDIVIDEND) { g_sio_div_signed = 0; g_sio_dividend = value; sio_div_update(); return; }
    if (addr == SIO_DIV_UDIVISOR) { g_sio_div_signed = 0; g_sio_divisor = value; sio_div_update(); return; }
    if (addr == SIO_DIV_SDIVIDEND) { g_sio_div_signed = 1; g_sio_dividend = value; sio_div_update(); return; }
    if (addr == SIO_DIV_SDIVISOR) { g_sio_div_signed = 1; g_sio_divisor = value; sio_div_update(); return; }
    if (addr == SIO_DIV_QUOTIENT) { g_sio_quotient = value; g_sio_div_dirty = 1; return; }
    if (addr == SIO_DIV_REMAINDER) { g_sio_remainder = value; g_sio_div_dirty = 1; return; }
    if (addr >= SIO_SPINLOCK_BASE && addr < SIO_SPINLOCK_END && (addr & 3u) == 0u) return;
    if (spi_reg_offset(addr, &spi_reg, &spi_alias)) {
        if (spi_reg == SPI_SSPCR0_OFF) { g_spi.cr0 = atomic_alias_value(g_spi.cr0, value, spi_alias); return; }
        if (spi_reg == SPI_SSPCR1_OFF) { g_spi.cr1 = atomic_alias_value(g_spi.cr1, value, spi_alias); return; }
        if (spi_reg == SPI_SSPCPSR_OFF) { g_spi.cpsr = atomic_alias_value(g_spi.cpsr, value, spi_alias); return; }
        if (spi_reg == SPI_SSPDMACR_OFF) { g_spi.dmacr = atomic_alias_value(g_spi.dmacr, value, spi_alias); return; }
        if (spi_reg == SPI_SSPICR_OFF) { g_spi.overrun = 0; return; }
        if (spi_reg == SPI_SSPDR_OFF) {
            if (width == 2u) {
                spi_write_data((u8)(value >> 8));
                spi_write_data((u8)value);
            } else {
                spi_write_data((u8)value);
            }
            return;
        }
    }
    if (register_bank_write(addr, value, UART0_BASE, UART_SIZE, g_uart0)) return;
    if (addr >= I2C1_BASE && addr < I2C1_BASE + 0x4000u) addr = i2c_addr;
    if (addr == I2C_IC_CON) { g_i2c.con = atomic_alias_value(g_i2c.con, value, i2c_alias); return; }
    if (addr == I2C_IC_TAR) { g_i2c.tar = atomic_alias_value(g_i2c.tar, value, i2c_alias); return; }
    if (addr == I2C_IC_DATA_CMD) {
        if (g_i2c.tx_level >= I2C_FIFO_SIZE) {
            g_i2c.tx_overflow = 1;
            g_i2c.tx_abrt |= 1u;
            g_i2c.raw_intr |= I2C_RAW_TX_ABRT;
            return;
        }
        g_i2c.tx_level += 1;
        if ((value & I2C_DATA_CMD_READ) != 0u) {
            u8 byte = 0;
            if ((g_i2c.tar & 0x7fu) == KBD_ADDR && g_i2c.selected_reg == KBD_REG_KEY) {
                if (g_i2c.key_report_index == 0) {
                    g_i2c.key_report_key = next_script_key();
                    g_i2c.key_report_index = 1;
                    byte = g_i2c.key_report_key >= 0 ? 1u : 0u;
                } else {
                    g_i2c.key_report_index = 0;
                    byte = g_i2c.key_report_key >= 0 ? (u8)g_i2c.key_report_key : 0u;
                }
            }
            i2c_rx_push(byte);
        } else if ((g_i2c.tar & 0x7fu) == KBD_ADDR) {
            g_i2c.selected_reg = (u8)value;
            g_i2c.key_report_index = 0;
        }
        trace_mmio("i2cw", addr, value);
        i2c_finish_command(value);
        return;
    }
    if (addr == I2C_IC_FS_HCNT) { g_i2c.fs_hcnt = value; return; }
    if (addr == I2C_IC_FS_LCNT) { g_i2c.fs_lcnt = value; return; }
    if (addr == I2C_IC_RX_TL) { g_i2c.rx_tl = value; return; }
    if (addr == I2C_IC_TX_TL) { g_i2c.tx_tl = value; return; }
    if (addr == I2C_IC_ENABLE) { g_i2c.enable = atomic_alias_value(g_i2c.enable, value, i2c_alias); return; }
    if (addr == I2C_IC_SDA_HOLD) { g_i2c.sda_hold = atomic_alias_value(g_i2c.sda_hold, value, i2c_alias); return; }
    if (addr == I2C_IC_DMA_CR) { g_i2c.dma_cr = atomic_alias_value(g_i2c.dma_cr, value, i2c_alias); return; }
    if (addr == I2C_IC_FS_SPKLEN) { g_i2c.fs_spklen = value; return; }
    if (addr == XIP_SSI_DR0) { xip_dr0_write(value); return; }
    if (addr >= XIP_SSI_BASE && addr < XIP_SSI_BASE + XIP_SSI_SIZE) { trace_xip_mmio("xipw", addr, value); return; }
    trace_unknown_mmio("mmiow", addr, value, 0u);
}

static void mmio_write32(u32 addr, u32 value) {
    mmio_write(addr, value, 4u);
}

static void mmio_write16(u32 addr, u32 value) {
    mmio_write(addr, value, 2u);
}

static void mmio_write8(u32 addr, u32 value) {
    mmio_write(addr, value, 1u);
}

static u32 mem_mmio_read32(void *ctx, u32 addr) {
    (void)ctx;
    return mmio_read32(addr);
}

static void mem_mmio_write32(void *ctx, u32 addr, u32 value) {
    (void)ctx;
    mmio_write32(addr, value);
}

static void mem_mmio_write16(void *ctx, u32 addr, u32 value) {
    (void)ctx;
    mmio_write16(addr, value);
}

static void mem_mmio_write8(void *ctx, u32 addr, u32 value) {
    (void)ctx;
    mmio_write8(addr, value);
}

static u8 bootrom_mem_read8(void *ctx, u32 addr) {
    (void)ctx;
    return mem_read8(addr);
}

static u32 bootrom_mem_read32(void *ctx, u32 addr) {
    (void)ctx;
    return mem_read32(addr);
}

static void bootrom_mem_write8(void *ctx, u32 addr, u32 value) {
    (void)ctx;
    mem_write8(addr, value);
}

static void bootrom_mem_write32(void *ctx, u32 addr, u32 value) {
    (void)ctx;
    mem_write32(addr, value);
}

static void set_nz(Cpu *cpu, u32 value) {
    cpu->n = value >> 31;
    cpu->z = value == 0u;
}

static void set_add_flags(Cpu *cpu, u32 a, u32 b, u32 result) {
    set_nz(cpu, result);
    cpu->c = result < a;
    cpu->v = (((a ^ result) & (b ^ result)) >> 31) & 1u;
}

static void set_sub_flags(Cpu *cpu, u32 a, u32 b, u32 result) {
    set_nz(cpu, result);
    cpu->c = a >= b;
    cpu->v = (((a ^ b) & (a ^ result)) >> 31) & 1u;
}

static void set_adc_flags(Cpu *cpu, u32 a, u32 b, u32 carry_in, u32 result) {
    unsigned long long wide = (unsigned long long)a + (unsigned long long)b + (unsigned long long)carry_in;
    set_nz(cpu, result);
    cpu->c = (u32)(wide >> 32) & 1u;
    cpu->v = (((a ^ result) & (b ^ result)) >> 31) & 1u;
}

static void set_sbc_flags(Cpu *cpu, u32 a, u32 b, u32 carry_in, u32 result) {
    unsigned long long subtrahend = (unsigned long long)b + (carry_in ? 0ull : 1ull);
    set_nz(cpu, result);
    cpu->c = (unsigned long long)a >= subtrahend;
    cpu->v = (((a ^ b) & (a ^ result)) >> 31) & 1u;
}

static u32 lsl_with_carry(Cpu *cpu, u32 value, u32 amount) {
    if (amount == 0u) return value;
    if (amount < 32u) {
        cpu->c = (value >> (32u - amount)) & 1u;
        return value << amount;
    }
    if (amount == 32u) cpu->c = value & 1u;
    else cpu->c = 0u;
    return 0u;
}

static u32 lsr_with_carry(Cpu *cpu, u32 value, u32 amount) {
    if (amount == 0u) return value;
    if (amount < 32u) {
        cpu->c = (value >> (amount - 1u)) & 1u;
        return value >> amount;
    }
    if (amount == 32u) cpu->c = (value >> 31) & 1u;
    else cpu->c = 0u;
    return 0u;
}

static u32 asr_with_carry(Cpu *cpu, u32 value, u32 amount) {
    if (amount == 0u) return value;
    if (amount < 32u) {
        cpu->c = (value >> (amount - 1u)) & 1u;
        return (u32)((s32)value >> amount);
    }
    cpu->c = (value >> 31) & 1u;
    return (value & 0x80000000u) != 0u ? 0xffffffffu : 0u;
}

static u32 ror_with_carry(Cpu *cpu, u32 value, u32 amount) {
    u32 rotate;
    if (amount == 0u) return value;
    rotate = amount & 31u;
    if (rotate != 0u) value = (value >> rotate) | (value << (32u - rotate));
    cpu->c = (value >> 31) & 1u;
    return value;
}

static int cond_true(Cpu *cpu, int cond) {
    switch (cond) {
        case 0: return cpu->z != 0u;
        case 1: return cpu->z == 0u;
        case 2: return cpu->c != 0u;
        case 3: return cpu->c == 0u;
        case 4: return cpu->n != 0u;
        case 5: return cpu->n == 0u;
        case 6: return cpu->v != 0u;
        case 7: return cpu->v == 0u;
        case 8: return cpu->c != 0u && cpu->z == 0u;
        case 9: return cpu->c == 0u || cpu->z != 0u;
        case 10: return cpu->n == cpu->v;
        case 11: return cpu->n != cpu->v;
        case 12: return cpu->z == 0u && cpu->n == cpu->v;
        case 13: return cpu->z != 0u || cpu->n != cpu->v;
        case 14: return 1;
    }
    return 0;
}

static int try_accelerate_delay_loop(Cpu *cpu) {
    u32 pc = cpu->r[15];
    u16 op0 = mem_read16(pc);
    u16 op1 = mem_read16(pc + 2u);
    u16 op2 = mem_read16(pc + 4u);
    u16 op3 = mem_read16(pc + 6u);
    int reg;
    u32 count;
    if (op0 != 0x46c0u && op0 != 0xbf00u) return 0;
    if ((op1 & 0xf800u) != 0x3800u || (op1 & 0xffu) != 1u) return 0;
    reg = (op1 >> 8) & 7;
    if ((op2 & 0xf800u) != 0x2800u || ((op2 >> 8) & 7) != reg || (op2 & 0xffu) != 0u) return 0;
    if ((op3 & 0xff00u) != 0xd100u) return 0;
    if (pc + 10u + sx((u32)(op3 & 0xffu) << 1, 9) != pc) return 0;
    count = cpu->r[reg];
    if (count == 0u) return 0;
    cpu->r[reg] = 0;
    cpu->n = 0;
    cpu->z = 1;
    cpu->c = 1;
    cpu->v = 0;
    cpu->r[15] = pc + 8u;
    cpu->steps += 4u;
    g_cycles += count * 4u;
    g_sim_ms += count / 60000u;
    return 1;
}

static int try_accelerate_memory_delay_loop(Cpu *cpu) {
    u32 pc = cpu->r[15];
    u16 op0 = mem_read16(pc);
    u16 op1 = mem_read16(pc + 2u);
    u16 op2 = mem_read16(pc + 4u);
    u16 op3 = mem_read16(pc + 6u);
    u32 cond_pc;
    u16 cond0;
    u16 cond1;
    u16 cond2;
    int reg;
    int limit_reg;
    u32 addr;
    u32 value;
    u32 limit;
    u32 remaining;
    if ((op0 & 0xf800u) != 0x9800u) return 0;
    reg = (op0 >> 8) & 7;
    if (op1 != (u16)(0x3001u | ((u32)reg << 8))) return 0;
    if (op2 != (u16)(0x9000u | ((u32)reg << 8) | (op0 & 0xffu))) return 0;
    if ((op3 & 0xf800u) != 0xe000u) return 0;
    cond_pc = pc + 10u + sx((u32)(op3 & 0x07ffu) << 1, 12);
    cond0 = mem_read16(cond_pc);
    cond1 = mem_read16(cond_pc + 2u);
    cond2 = mem_read16(cond_pc + 4u);
    if (cond0 != op0) return 0;
    if ((cond1 & 0xffc0u) != 0x4280u || (cond1 & 7) != reg) return 0;
    limit_reg = (cond1 >> 3) & 15;
    if ((cond2 & 0xff00u) != 0xd900u) return 0;
    if (cond_pc + 8u + sx((u32)(cond2 & 0xffu) << 1, 9) != pc) return 0;
    addr = cpu->r[13] + ((u32)(op0 & 0xffu) << 2);
    value = mem_read32(addr);
    limit = cpu->r[limit_reg];
    if (value > limit) return 0;
    remaining = limit - value + 1u;
    mem_write32(addr, limit + 1u);
    cpu->n = 0;
    cpu->z = 0;
    cpu->c = 1;
    cpu->v = 0;
    cpu->r[15] = cond_pc + 6u;
    cpu->steps += 8u;
    g_cycles += remaining * 4u;
    g_sim_ms += remaining / 60000u;
    g_frame_ready = 1;
    return 1;
}

static int nearby_timer_load_seen(Cpu *cpu, u32 start_pc, u32 end_pc);

static int try_accelerate_timer_wait_loop(Cpu *cpu) {
    u32 pc = cpu->r[15];
    u16 op0 = mem_read16(pc);
    u16 op1 = mem_read16(pc + 2u);
    u32 loop_pc;
    u32 scan_pc;
    int saw_high = 0;
    int saw_low = 0;
    if (op0 != 0xbf20u) return 0;
    if ((op1 & 0xf800u) != 0xe000u) return 0;
    loop_pc = pc + 6u + sx((u32)(op1 & 0x07ffu) << 1, 12);
    if (loop_pc >= pc || pc - loop_pc > 96u) return 0;
    for (scan_pc = loop_pc; scan_pc < pc; scan_pc += 2u) {
        u16 op = mem_read16(scan_pc);
        if ((op & 0xf800u) == 0x6800u) {
            u32 base_reg = (op >> 3) & 7u;
            u32 offset = ((op >> 6) & 0x1fu) << 2;
            if (cpu->r[base_reg] == TIMER_BASE) {
                if (offset == 0x24u) saw_high = 1;
                if (offset == 0x28u) saw_low = 1;
            }
        }
    }
    if (!saw_high || !saw_low) return 0;
    g_cycles += TIMER_WAIT_ACCEL_US * TIMER_CYCLES_PER_US;
    g_sim_ms += TIMER_WAIT_ACCEL_US / 1000u;
    cpu->r[15] = pc + 2u;
    cpu->steps += 1u;
    return 1;
}

static int try_accelerate_timer_compare_loop(Cpu *cpu) {
    u32 pc = cpu->r[15];
    u16 op0 = mem_read16(pc);
    u16 op1 = mem_read16(pc + 2u);
    u16 op2 = mem_read16(pc + 4u);
    u32 low_reg;
    u32 limit_reg;
    u32 base_reg;
    u32 branch_target;
    u32 now;
    u32 remaining;
    if ((op0 & 0xf800u) != 0x6800u) return 0;
    low_reg = op0 & 7u;
    base_reg = (op0 >> 3) & 7u;
    if ((((op0 >> 6) & 0x1fu) << 2) != 0x28u) return 0;
    if (cpu->r[base_reg] != TIMER_BASE) return 0;
    if ((op1 & 0xffc0u) != 0x4280u || (op1 & 7u) != low_reg) return 0;
    limit_reg = (op1 >> 3) & 15u;
    if ((op2 & 0xff00u) != 0xd300u) return 0;
    branch_target = pc + 8u + sx((u32)(op2 & 0xffu) << 1, 9);
    if (branch_target >= pc || pc - branch_target > 16u) return 0;
    if (nearby_timer_load_seen(cpu, branch_target, pc) == 0) return 0;
    now = timer_us_low();
    if (now >= cpu->r[limit_reg]) return 0;
    remaining = cpu->r[limit_reg] - now;
    g_cycles += remaining * TIMER_CYCLES_PER_US;
    g_sim_ms += remaining / 1000u;
    cpu->r[low_reg] = cpu->r[limit_reg];
    cpu->n = 0;
    cpu->z = 1;
    cpu->c = 1;
    cpu->v = 0;
    cpu->r[15] = pc + 6u;
    cpu->steps += 3u;
    return 1;
}

static int nearby_timer_load_seen(Cpu *cpu, u32 start_pc, u32 end_pc) {
    u32 timer_base_regs = 0u;
    u32 scan_pc;
    int saw_timer_load = 0;
    for (scan_pc = start_pc; scan_pc < end_pc; scan_pc += 2u) {
        u16 op = mem_read16(scan_pc);
        if ((op & 0xf800u) == 0x4800u) {
            u32 literal_addr = (scan_pc + 4u) & ~3u;
            u32 target_reg = (op >> 8) & 7u;
            literal_addr += (u32)(op & 0xffu) << 2;
            if (mem_read32(literal_addr) == TIMER_BASE) timer_base_regs |= 1u << target_reg;
        }
        if ((op & 0xf800u) == 0x6800u) {
            u32 base_reg = (op >> 3) & 7u;
            u32 offset = ((op >> 6) & 0x1fu) << 2;
            if ((cpu->r[base_reg] == TIMER_BASE || (timer_base_regs & (1u << base_reg)) != 0u) &&
                    (offset == 0x24u || offset == 0x28u)) {
                saw_timer_load = 1;
            }
        }
    }
    return saw_timer_load;
}

static int try_accelerate_timer_poll_branch(Cpu *cpu) {
    u32 pc = cpu->r[15];
    u16 op = mem_read16(pc);
    u32 branch_target;
    u32 scan_start;
    int cond;
    int taken;
    if ((op & 0xf000u) != 0xd000u || (op & 0x0f00u) == 0x0f00u) return 0;
    branch_target = pc + 4u + sx((u32)(op & 0xffu) << 1, 9);
    cond = (op >> 8) & 15;
    taken = cond_true(cpu, cond);
    if (!taken && branch_target < pc) return 0;
    if (taken && branch_target > pc) return 0;
    scan_start = pc > 64u ? pc - 64u : pc;
    if (!nearby_timer_load_seen(cpu, scan_start, pc)) return 0;
    g_cycles += TIMER_WAIT_ACCEL_US * TIMER_CYCLES_PER_US;
    g_sim_ms += TIMER_WAIT_ACCEL_US / 1000u;
    cpu->r[15] = taken ? branch_target : pc + 2u;
    cpu->steps += 1u;
    return 1;
}

static u32 cpu_xpsr(const Cpu *cpu) {
    return (cpu->n << 31) | (cpu->z << 30) | (cpu->c << 29) | (cpu->v << 28) | (1u << 24) | (cpu->ipsr & 0x1ffu);
}

static void cpu_set_xpsr(Cpu *cpu, u32 xpsr) {
    cpu->n = (xpsr >> 31) & 1u;
    cpu->z = (xpsr >> 30) & 1u;
    cpu->c = (xpsr >> 29) & 1u;
    cpu->v = (xpsr >> 28) & 1u;
    cpu->ipsr = xpsr & 0x1ffu;
}

static int exception_enter(Cpu *cpu, u32 exc) {
    u32 handler = mem_read32(g_core.vtor + exc * 4u);
    u32 sp;
    if (handler == 0u || handler == 0xffffffffu || (handler & 1u) == 0u) return 0;
    sp = cpu->r[13] - 32u;
    mem_write32(sp + 0u, cpu->r[0]);
    mem_write32(sp + 4u, cpu->r[1]);
    mem_write32(sp + 8u, cpu->r[2]);
    mem_write32(sp + 12u, cpu->r[3]);
    mem_write32(sp + 16u, cpu->r[12]);
    mem_write32(sp + 20u, cpu->r[14]);
    mem_write32(sp + 24u, cpu->r[15] | 1u);
    mem_write32(sp + 28u, cpu_xpsr(cpu));
    cpu->r[13] = sp;
    cpu->r[14] = EXC_RETURN_THREAD_MSP;
    cpu->r[15] = handler & ~1u;
    cpu->ipsr = exc;
    if (exc == EXC_SYSTICK) g_core.icsr &= ~(1u << 26);
    if (g_trace_fd >= 0) { trace_text("exception enter="); trace_hex32(exc); trace_text(" handler="); trace_hex32(handler); trace_text("\n"); }
    return 1;
}

static int exception_return(Cpu *cpu, u32 value) {
    u32 sp;
    if ((value & 0xfffffff0u) != 0xfffffff0u) return 0;
    sp = cpu->r[13];
    cpu->r[0] = mem_read32(sp + 0u);
    cpu->r[1] = mem_read32(sp + 4u);
    cpu->r[2] = mem_read32(sp + 8u);
    cpu->r[3] = mem_read32(sp + 12u);
    cpu->r[12] = mem_read32(sp + 16u);
    cpu->r[14] = mem_read32(sp + 20u);
    cpu->r[15] = mem_read32(sp + 24u) & ~1u;
    cpu_set_xpsr(cpu, mem_read32(sp + 28u));
    cpu->ipsr = 0;
    cpu->r[13] = sp + 32u;
    g_skip_tick_after_exception_return = 1;
    if (g_trace_fd >= 0) { trace_text("exception return pc="); trace_hex32(cpu->r[15]); trace_text("\n"); }
    return 1;
}

static void peripherals_tick(Cpu *cpu) {
    spi_service(0);
    if (g_skip_tick_after_exception_return) { g_skip_tick_after_exception_return = 0; return; }
    if (cpu->ipsr != 0u) return;
    if ((g_core.syst_csr & SYST_CSR_ENABLE) != 0u) {
        if (g_core.syst_cvr == 0u) g_core.syst_cvr = g_core.syst_rvr;
        else g_core.syst_cvr -= 1u;
        if (g_core.syst_cvr == 0u) {
            g_core.syst_csr |= SYST_CSR_COUNTFLAG;
            if ((g_core.syst_csr & SYST_CSR_TICKINT) != 0u) g_core.icsr |= 1u << 26;
        }
    }
}

static int service_interrupts(Cpu *cpu) {
    u32 pending;
    int irq;
    if (cpu->ipsr != 0u || cpu->primask != 0u) return 0;
    if ((g_core.icsr & (1u << 26)) != 0u) return exception_enter(cpu, EXC_SYSTICK);
    pending = g_core.nvic_pending & g_core.nvic_enable;
    if (pending == 0u) return 0;
    for (irq = 0; irq < 32; ++irq) {
        if ((pending & (1u << irq)) != 0u) {
            g_core.nvic_pending &= ~(1u << irq);
            return exception_enter(cpu, EXC_IRQ0 + (u32)irq);
        }
    }
    return 0;
}

static int step(Cpu *cpu) {
    u32 pc = cpu->r[15];
    u16 op = mem_read16(pc);
    g_current_pc = pc;
    cpu->r[15] = pc + 2u;

    if ((op & 0xff00u) == 0xbe00u) return exception_enter(cpu, EXC_HARDFAULT) ? 0 : -1;
    if ((op & 0xff00u) == 0xdf00u) return exception_enter(cpu, EXC_SVC) ? 0 : -1;
    if ((op & 0xffefu) == 0xb662u) { cpu->primask = (op & 0x0010u) != 0u; return 0; }
    if ((op & 0xff00u) == 0xbf00u) return 0;

    if ((op & 0xfff0u) == 0xf380u) {
        u16 op2 = mem_read16(pc + 2u);
        int rn = op & 15;
        int sysm = op2 & 0xff;
        if ((op2 & 0xff00u) == 0x8800u && sysm == 8) {
            cpu->r[13] = cpu->r[rn];
            cpu->r[15] = pc + 4u;
            return 0;
        }
        if ((op2 & 0xff00u) == 0x8800u && sysm == 0x10) {
            cpu->primask = cpu->r[rn] & 1u;
            cpu->r[15] = pc + 4u;
            return 0;
        }
    }

    if (op == 0xf3bfu) {
        u16 op2 = mem_read16(pc + 2u);
        if (op2 == 0x8f4fu || op2 == 0x8f5fu || op2 == 0x8f6fu) {
            cpu->r[15] = pc + 4u;
            return 0;
        }
    }

    if (op == 0xf3efu) {
        u16 op2 = mem_read16(pc + 2u);
        int rd = (op2 >> 8) & 15;
        int sysm = op2 & 0xff;
        if ((op2 & 0xf000u) == 0x8000u && sysm == 0x10) {
            cpu->r[rd] = cpu->primask;
            cpu->r[15] = pc + 4u;
            return 0;
        }
        if ((op2 & 0xf000u) == 0x8000u && sysm == 0x05) {
            cpu->r[rd] = cpu->ipsr;
            cpu->r[15] = pc + 4u;
            return 0;
        }
    }

    if ((op & 0xf800u) == 0xf000u) {
        u16 op2 = mem_read16(pc + 2u);
        if ((op2 & 0xf800u) == 0xf800u) {
            u32 imm = ((u32)(op & 0x07ffu) << 12) | ((u32)(op2 & 0x07ffu) << 1);
            imm = sx(imm, 23);
            cpu->r[14] = (pc + 5u) & ~1u;
            cpu->r[15] = (pc + 4u + imm) & ~1u;
            trace_branch("bl", pc, cpu->r[15], cpu->r[14], op);
            return 0;
        }
    }

    if ((op & 0xf800u) == 0xe000u) {
        cpu->r[15] = pc + 4u + sx((u32)(op & 0x07ffu) << 1, 12);
        return 0;
    }
    if ((op & 0xf000u) == 0xd000u && (op & 0x0f00u) != 0x0f00u) {
        int cond = (op >> 8) & 15;
        if (cond_true(cpu, cond)) cpu->r[15] = pc + 4u + sx((u32)(op & 0xffu) << 1, 9);
        return 0;
    }
    if ((op & 0xf800u) == 0x4800u) {
        int rt = (op >> 8) & 7;
        u32 addr = ((pc + 4u) & ~3u) + ((u32)(op & 0xffu) << 2);
        cpu->r[rt] = mem_read32(addr);
        return 0;
    }
    if ((op & 0xf800u) == 0x8800u) {
        int imm = (op >> 6) & 31;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = mem_read16(cpu->r[rn] + ((u32)imm << 1));
        return 0;
    }
    if ((op & 0xe000u) == 0x2000u) {
        int rd = (op >> 8) & 7;
        u32 imm = op & 0xffu;
        u32 result;
        if ((op & 0x1800u) == 0x0000u) {
            cpu->r[rd] = imm;
            set_nz(cpu, imm);
        } else if ((op & 0x1800u) == 0x0800u) {
            result = cpu->r[rd] - imm;
            set_sub_flags(cpu, cpu->r[rd], imm, result);
        } else if ((op & 0x1800u) == 0x1000u) {
            result = cpu->r[rd] + imm;
            cpu->r[rd] = result;
            set_add_flags(cpu, cpu->r[rd] - imm, imm, result);
        } else {
            result = cpu->r[rd] - imm;
            cpu->r[rd] = result;
            set_sub_flags(cpu, cpu->r[rd] + imm, imm, result);
        }
        return 0;
    }
    if ((op & 0xfc00u) == 0x4000u) {
        int alu = (op >> 6) & 15;
        int rm = (op >> 3) & 7;
        int rdn = op & 7;
        u32 a = cpu->r[rdn];
        u32 b = cpu->r[rm];
        u32 result = 0;
        u32 carry_in = cpu->c;
        if (alu == 0) result = a & b;
        else if (alu == 1) result = a ^ b;
        else if (alu == 2) result = lsl_with_carry(cpu, a, b & 0xffu);
        else if (alu == 3) result = lsr_with_carry(cpu, a, b & 0xffu);
        else if (alu == 4) result = asr_with_carry(cpu, a, b & 0xffu);
        else if (alu == 5) result = a + b + carry_in;
        else if (alu == 6) result = a - b - (carry_in ? 0u : 1u);
        else if (alu == 7) result = ror_with_carry(cpu, a, b & 0xffu);
        else if (alu == 8) result = a & b;
        else if (alu == 9) result = 0u - b;
        else if (alu == 10) result = a - b;
        else if (alu == 11) result = a + b;
        else if (alu == 12) result = a | b;
        else if (alu == 13) result = a * b;
        else if (alu == 14) result = a & ~b;
        else if (alu == 15) result = ~b;
        else return -1;
        if (alu == 5) { cpu->r[rdn] = result; set_adc_flags(cpu, a, b, carry_in, result); }
        else if (alu == 6) { cpu->r[rdn] = result; set_sbc_flags(cpu, a, b, carry_in, result); }
        else if (alu == 8) set_nz(cpu, result);
        else if (alu == 10) set_sub_flags(cpu, a, b, result);
        else if (alu == 9) { cpu->r[rdn] = result; set_sub_flags(cpu, 0u, b, result); }
        else if (alu == 11) set_add_flags(cpu, a, b, result);
        else { cpu->r[rdn] = result; set_nz(cpu, result); }
        return 0;
    }
    if ((op & 0xfc00u) == 0x4400u) {
        int opkind = (op >> 8) & 3;
        int dst = (op & 7) | ((op >> 4) & 8);
        int src = ((op >> 3) & 15);
        if (opkind == 0) cpu->r[dst] += cpu->r[src];
        else if (opkind == 1) set_sub_flags(cpu, cpu->r[dst], cpu->r[src], cpu->r[dst] - cpu->r[src]);
        else if (opkind == 2) cpu->r[dst] = cpu->r[src];
        else if (opkind == 3) {
            int link = (op & 0x0080u) != 0u;
            u32 target = cpu->r[src] & ~1u;
            trace_branch(link ? "blx" : "bx", pc, target, cpu->r[14], op);
            if (link) {
                cpu->r[14] = (pc + 2u) | 1u;
                if (target == BOOTROM_LOOKUP_ADDR) return bootrom_lookup_call(cpu, pc + 2u) ? 0 : -1;
                if (bootrom_function_call(cpu, target, pc + 2u)) return 0;
                cpu->r[15] = target;
            } else if (bootrom_function_call(cpu, target, cpu->r[14] & ~1u)) return 0;
            else if (!exception_return(cpu, cpu->r[src])) cpu->r[15] = target;
        }
        return 0;
    }
    if ((op & 0xfe00u) == 0xb400u) {
        u32 list = op & 0xffu;
        int i;
        if ((op & 0x0100u) != 0u) list |= 1u << 14;
        for (i = 14; i >= 0; --i) if ((list & (1u << i)) != 0u) { cpu->r[13] -= 4u; mem_write32(cpu->r[13], cpu->r[i]); }
        return 0;
    }
    if ((op & 0xfe00u) == 0xbc00u) {
        u32 list = op & 0xffu;
        int i;
        if ((op & 0x0100u) != 0u) list |= 1u << 15;
        for (i = 0; i < 16; ++i) if ((list & (1u << i)) != 0u) { cpu->r[i] = mem_read32(cpu->r[13]); cpu->r[13] += 4u; }
        if ((list & (1u << 15)) != 0u) {
            trace_branch("pop-pc", pc, cpu->r[15] & ~1u, cpu->r[14], op);
            if (!exception_return(cpu, cpu->r[15])) cpu->r[15] &= ~1u;
        }
        return 0;
    }
    if ((op & 0xf800u) == 0x6800u) {
        int imm = (op >> 6) & 31;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = mem_read32(cpu->r[rn] + ((u32)imm << 2));
        return 0;
    }
    if ((op & 0xf800u) == 0x6000u) {
        int imm = (op >> 6) & 31;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        mem_write32(cpu->r[rn] + ((u32)imm << 2), cpu->r[rt]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5800u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = mem_read32(cpu->r[rn] + cpu->r[rm]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5000u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        mem_write32(cpu->r[rn] + cpu->r[rm], cpu->r[rt]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5200u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        mem_write16(cpu->r[rn] + cpu->r[rm], cpu->r[rt]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5400u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        mem_write8(cpu->r[rn] + cpu->r[rm], cpu->r[rt]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5600u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = sx(mem_read8(cpu->r[rn] + cpu->r[rm]), 8);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5a00u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = mem_read16(cpu->r[rn] + cpu->r[rm]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5c00u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = mem_read8(cpu->r[rn] + cpu->r[rm]);
        return 0;
    }
    if ((op & 0xfe00u) == 0x5e00u) {
        int rm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = sx(mem_read16(cpu->r[rn] + cpu->r[rm]), 16);
        return 0;
    }
    if ((op & 0xf800u) == 0x7800u) {
        int imm = (op >> 6) & 31;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        cpu->r[rt] = mem_read8(cpu->r[rn] + (u32)imm);
        return 0;
    }
    if ((op & 0xf800u) == 0x7000u) {
        int imm = (op >> 6) & 31;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        mem_write8(cpu->r[rn] + (u32)imm, cpu->r[rt]);
        return 0;
    }
    if ((op & 0xf800u) == 0x9000u) {
        int rt = (op >> 8) & 7;
        mem_write32(cpu->r[13] + ((u32)(op & 0xffu) << 2), cpu->r[rt]);
        return 0;
    }
    if ((op & 0xf800u) == 0x9800u) {
        int rt = (op >> 8) & 7;
        cpu->r[rt] = mem_read32(cpu->r[13] + ((u32)(op & 0xffu) << 2));
        return 0;
    }
    if ((op & 0xf800u) == 0xa000u) {
        int rd = (op >> 8) & 7;
        cpu->r[rd] = ((pc + 4u) & ~3u) + ((u32)(op & 0xffu) << 2);
        return 0;
    }
    if ((op & 0xf800u) == 0xa800u) {
        int rd = (op >> 8) & 7;
        cpu->r[rd] = cpu->r[13] + ((u32)(op & 0xffu) << 2);
        return 0;
    }
    if ((op & 0xf000u) == 0xc000u) {
        int load = (op & 0x0800u) != 0u;
        int rn = (op >> 8) & 7;
        u32 addr = cpu->r[rn];
        u32 list = op & 0xffu;
        int i;
        for (i = 0; i < 8; ++i) {
            if ((list & (1u << i)) != 0u) {
                if (load) cpu->r[i] = mem_read32(addr);
                else mem_write32(addr, cpu->r[i]);
                addr += 4u;
            }
        }
        cpu->r[rn] = addr;
        return 0;
    }
    if ((op & 0xf800u) == 0x8000u) {
        int imm = (op >> 6) & 31;
        int rn = (op >> 3) & 7;
        int rt = op & 7;
        mem_write16(cpu->r[rn] + ((u32)imm << 1), cpu->r[rt]);
        return 0;
    }
    if ((op & 0xffc0u) == 0xb200u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        cpu->r[rd] = sx(cpu->r[rm] & 0xffffu, 16);
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xffc0u) == 0xb240u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        cpu->r[rd] = sx(cpu->r[rm] & 0xffu, 8);
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xffc0u) == 0xb280u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        cpu->r[rd] = cpu->r[rm] & 0xffffu;
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xffc0u) == 0xb2c0u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        cpu->r[rd] = cpu->r[rm] & 0xffu;
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xffc0u) == 0xba00u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        u32 value = cpu->r[rm];
        cpu->r[rd] = ((value & 0x000000ffu) << 24) | ((value & 0x0000ff00u) << 8) |
                     ((value & 0x00ff0000u) >> 8) | ((value & 0xff000000u) >> 24);
        return 0;
    }
    if ((op & 0xffc0u) == 0xba40u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        u32 value = cpu->r[rm];
        cpu->r[rd] = ((value & 0x00ff00ffu) << 8) | ((value & 0xff00ff00u) >> 8);
        return 0;
    }
    if ((op & 0xffc0u) == 0xbac0u) {
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        u32 value = cpu->r[rm];
        cpu->r[rd] = sx(((value & 0x000000ffu) << 8) | ((value & 0x0000ff00u) >> 8), 16);
        return 0;
    }
    if ((op & 0xff00u) == 0xb000u) {
        u32 imm = (u32)(op & 0x7fu) << 2;
        if ((op & 0x0080u) != 0u) cpu->r[13] -= imm;
        else cpu->r[13] += imm;
        return 0;
    }
    if ((op & 0xf800u) == 0x0000u) {
        int imm = (op >> 6) & 31;
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        cpu->r[rd] = lsl_with_carry(cpu, cpu->r[rm], (u32)imm);
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xf800u) == 0x0800u) {
        int imm = (op >> 6) & 31;
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        if (imm == 0) imm = 32;
        cpu->r[rd] = lsr_with_carry(cpu, cpu->r[rm], (u32)imm);
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xf800u) == 0x1000u) {
        int imm = (op >> 6) & 31;
        int rm = (op >> 3) & 7;
        int rd = op & 7;
        if (imm == 0) imm = 32;
        cpu->r[rd] = asr_with_carry(cpu, cpu->r[rm], (u32)imm);
        set_nz(cpu, cpu->r[rd]);
        return 0;
    }
    if ((op & 0xf800u) == 0x1800u) {
        int immflag = (op >> 10) & 1;
        int sub = (op >> 9) & 1;
        int rn_imm = (op >> 6) & 7;
        int rn = (op >> 3) & 7;
        int rd = op & 7;
        u32 b = immflag ? (u32)rn_imm : cpu->r[rn_imm];
        u32 a = cpu->r[rn];
        u32 result = sub ? a - b : a + b;
        cpu->r[rd] = result;
        if (sub) set_sub_flags(cpu, a, b, result); else set_add_flags(cpu, a, b, result);
        return 0;
    }
    if ((op & 0xff00u) == 0x4600u) return 0;

    out("unsupported pc="); out_hex(pc); out(" op="); out_hex(op); out("\n");
    return -1;
}

static void load_flash_state(const char *path) {
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize off = 0;
    if (fd < 0) return;
    while (off < sizeof(g_flash)) {
        long got = sys_read((int)fd, g_flash + off, sizeof(g_flash) - off);
        if (got <= 0) break;
        off += (usize)got;
    }
    if (off > g_flash_size) g_flash_size = off;
    (void)sys_close((int)fd);
}

static void save_flash_state(void) {
    long fd;
    usize off = 0;
    if (g_flash_state_path == 0) return;
    fd = sys_openat(AT_FDCWD, g_flash_state_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    while (off < sizeof(g_flash)) {
        long wrote = sys_write((int)fd, g_flash + off, sizeof(g_flash) - off);
        if (wrote <= 0) break;
        off += (usize)wrote;
    }
    (void)sys_close((int)fd);
}

static int load_file(const char *path) {
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize off = (usize)(g_input_base - FLASH_BASE);
    usize loaded = 0;
    if (fd < 0) return -1;
    while (off < sizeof(g_flash)) {
        long got = sys_read((int)fd, g_flash + off, sizeof(g_flash) - off);
        if (got < 0) { (void)sys_close((int)fd); return -1; }
        if (got == 0) break;
        off += (usize)got;
        loaded += (usize)got;
    }
    if (off > g_flash_size) g_flash_size = off;
    (void)sys_close((int)fd);
    return loaded > 8u ? 0 : -1;
}

static void write_ppm(const char *path) {
    static const char header[] = "P6\n320 320\n255\n";
    long fd = sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)sys_write((int)fd, header, sizeof(header) - 1u);
    (void)sys_write((int)fd, g_framebuffer, sizeof(g_framebuffer));
}

static int png_fd_write(void *ctx, const void *data, png_usize count) {
    int fd = *(int *)ctx;
    return sys_write(fd, data, count) == (long)count ? 0 : -1;
}

static int gif_fd_write(void *ctx, const void *data, gif_usize count) {
    int fd = *(int *)ctx;
    return sys_write(fd, data, count) == (long)count ? 0 : -1;
}

static void write_png(const char *path) {
    int fd = (int)sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)png_write_rgb8(png_fd_write, &fd, g_framebuffer, LCD_W, LCD_H, g_png_work, sizeof(g_png_work));
}

static void write_image(const char *path) {
    if (str_ends(path, ".ppm")) write_ppm(path);
    else write_png(path);
}

static int gif_start(const char *path) {
    if (g_gif_active) return 0;
    g_gif_fd = (int)sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_gif_fd < 0) return -1;
    if (gif_begin_rgb8(&g_gif, gif_fd_write, &g_gif_fd, LCD_W, LCD_H, (unsigned int)g_gif_fps) != 0) {
        (void)sys_close(g_gif_fd);
        g_gif_fd = -1;
        return -1;
    }
    g_gif_active = 1;
    return 0;
}

static int gif_finish(void) {
    int result = 0;
    if (!g_gif_active) return 0;
    if (gif_end(&g_gif) != 0) result = -1;
    (void)sys_close(g_gif_fd);
    g_gif_active = 0;
    g_gif_fd = -1;
    return result;
}

static void write_frame_output(const char *image_path) {
    char frame_path[512];
    u32 hash = framebuffer_hash();
    g_last_output_hash = hash;
    if (path_is_gif(image_path)) {
        if (gif_start(image_path) == 0) (void)gif_write_frame_rgb8(&g_gif, g_framebuffer);
        terminal_render_live();
        out("wrote gif frame "); out_hex((u32)g_frame_index); out(" "); out(image_path); out(" hash="); out_hex(hash); out("\n");
        if (g_trace_fd >= 0) { trace_text("gif-frame index="); trace_hex32((u32)g_frame_index); trace_text(" hash="); trace_hex32(hash); trace_text("\n"); }
        g_frame_index += 1;
    } else if (path_has_frame_pattern(image_path)) {
        format_frame_path(image_path, g_frame_index, frame_path, sizeof(frame_path));
        write_image(frame_path);
        terminal_render_live();
        out("wrote frame "); out_hex((u32)g_frame_index); out(" "); out(frame_path); out(" hash="); out_hex(hash); out("\n");
        if (g_trace_fd >= 0) { trace_text("frame index="); trace_hex32((u32)g_frame_index); trace_text(" hash="); trace_hex32(hash); trace_text("\n"); }
        g_frame_index += 1;
    } else {
        write_image(image_path);
        terminal_render_live();
    }
}

static void cpu_reset(Cpu *cpu) {
    int i;
    for (i = 0; i < 16; ++i) cpu->r[i] = 0;
    cpu->n = cpu->z = cpu->c = cpu->v = cpu->primask = cpu->ipsr = cpu->steps = 0;
}

static void emulator_reset(Cpu *cpu, const char *key_script, int target_frames) {
    int i;
    cpu_reset(cpu);
    g_lcd.dc = 1;
    g_lcd.cs = 1;
    g_lcd.sleep = 1;
    g_lcd.display_on = 0;
    g_lcd.invert = 0;
    g_lcd.madctl = 0;
    g_lcd.colmod = 0;
    g_lcd.command = 0;
    g_lcd.param_count = 0;
    g_lcd.pixel_count = 0;
    g_lcd.x0 = 0;
    g_lcd.x1 = LCD_W - 1;
    g_lcd.y0 = 0;
    g_lcd.y1 = LCD_H - 1;
    g_key_script = key_script;
    g_key_script_pos = 0;
    g_cycles = 0;
    g_sim_ms = 0;
    g_frame_ready = 0;
    g_frame_hash = 2166136261u;
    g_last_output_hash = 2166136261u;
    g_frame_index = 0;
    g_target_frames = target_frames <= 0 ? 1 : target_frames;
    g_seen_lcd_command = 0;
    g_first_lcd_command = 0;
    g_first_lcd_command_pc = 0;
    g_first_lcd_command_cycles = 0;
    g_seen_lcd_pixel = 0;
    g_first_lcd_pixel_x = 0;
    g_first_lcd_pixel_y = 0;
    g_first_lcd_pixel_rgb = 0;
    g_first_lcd_pixel_pc = 0;
    g_first_lcd_pixel_cycles = 0;
    g_seen_lcd_nonblack = 0;
    g_first_lcd_nonblack_x = 0;
    g_first_lcd_nonblack_y = 0;
    g_first_lcd_nonblack_rgb = 0;
    g_first_lcd_nonblack_pc = 0;
    g_first_lcd_nonblack_cycles = 0;
    g_skip_tick_after_exception_return = 0;
    g_resets_reset = 0;
    g_gpio_out = (1u << LCD_CS_PIN) | (1u << LCD_DC_PIN) | (1u << LCD_RST_PIN);
    g_gpio_oe = 0;
    g_sio_dividend = 0;
    g_sio_divisor = 0;
    g_sio_quotient = 0xffffffffu;
    g_sio_remainder = 0;
    g_sio_div_signed = 0;
    g_sio_div_dirty = 1;
    for (i = 0; i < (int)(IO_BANK0_SIZE / 4u); ++i) g_io_bank0[i] = 0;
    for (i = 0; i < (int)(PADS_BANK0_SIZE / 4u); ++i) g_pads_bank0[i] = 0;
    for (i = 0; i < (int)(CLOCKS_SIZE / 4u); ++i) g_clocks[i] = 0;
    for (i = 0; i < (int)(XOSC_SIZE / 4u); ++i) g_xosc[i] = 0;
    for (i = 0; i < (int)(PLL_SIZE / 4u); ++i) {
        g_pll_sys[i] = 0;
        g_pll_usb[i] = 0;
    }
    for (i = 0; i < (int)(RTC_SIZE / 4u); ++i) g_rtc[i] = 0;
    for (i = 0; i < (int)(PIO_SIZE / 4u); ++i) {
        g_pio0[i] = 0;
        g_pio1[i] = 0;
    }
    g_xip_dr0_command = 0;
    g_xip_dr0_reads = 0;
    g_spi.cr0 = 0;
    g_spi.cr1 = 0;
    g_spi.cpsr = 0;
    g_spi.last_rx = 0;
    g_spi.tx_head = 0;
    g_spi.tx_count = 0;
    g_spi.rx_head = 0;
    g_spi.rx_count = 0;
    g_spi.overrun = 0;
    g_spi.busy_until = 0;
    g_i2c.con = 0;
    g_i2c.tar = 0;
    g_i2c.enable = 0;
    g_i2c.fs_hcnt = 0;
    g_i2c.fs_lcnt = 0;
    g_i2c.sda_hold = 0;
    g_i2c.fs_spklen = 0;
    g_i2c.tx_abrt = 0;
    g_i2c.raw_intr = I2C_RAW_TX_EMPTY;
    g_i2c.selected_reg = 0;
    g_i2c.rx_head = 0;
    g_i2c.rx_count = 0;
    g_i2c.tx_level = 0;
    g_i2c.tx_overflow = 0;
    g_i2c.key_report_index = 0;
    g_i2c.key_report_key = -1;
    g_i2c.busy_until = 0;
    for (i = 0; i < 12; ++i) {
        g_dma[i].read_addr = 0;
        g_dma[i].write_addr = 0;
        g_dma[i].transfer_count = 0;
        g_dma[i].ctrl_trig = 0;
    }
    g_core.vtor = g_vector_base;
    g_core.icsr = 0;
    g_core.nvic_enable = 0;
    g_core.nvic_pending = 0;
    g_core.syst_csr = 0;
    g_core.syst_rvr = 0;
    g_core.syst_cvr = 0;
    g_saved_termios_valid = 0;
    g_gif_active = 0;
    g_gif_fd = -1;
    lcd_gpio_sync();
}

static int run_bin(const char *in_path, const char *image_path, const char *key_script, int target_frames) {
    Cpu cpu;
    u32 sp;
    u32 reset;
    emulator_reset(&cpu, key_script, target_frames);
    if (str_eq(key_script == 0 ? "" : key_script, "-")) {
        long flags = sys_fcntl(0, F_GETFL, 0);
        if (flags >= 0) (void)sys_fcntl(0, F_SETFL, flags | O_NONBLOCK);
        g_live_stdin = 1;
        g_key_script = 0;
    } else {
        g_live_stdin = 0;
    }
    terminal_enter_live();
    flash_fill_erased();
    if (g_flash_state_path != 0) load_flash_state(g_flash_state_path);
    if (load_file(in_path) != 0) {
        out("failed to load input bin\n");
        terminal_leave_live();
        return 1;
    }
    sp = read_le32(g_flash + (usize)(g_vector_base - FLASH_BASE));
    reset = read_le32(g_flash + (usize)(g_vector_base - FLASH_BASE) + 4u);
    if (sp < RAM_BASE || sp > RAM_BASE + RAM_SIZE || (reset & 1u) == 0u) {
        out("unexpected vector table sp="); out_hex(sp); out(" reset="); out_hex(reset); out("\n");
        save_flash_state();
        terminal_leave_live();
        return 1;
    }
    cpu.r[13] = sp;
    cpu.r[15] = reset & ~1u;
    while (cpu.steps < g_max_steps) {
        u32 pc = cpu.r[15];
        u16 op = mem_read16(pc);
        if (!executable_addr(pc)) {
            out("invalid execute pc="); out_pc(pc); out("\n");
            report_lcd_milestones();
            save_flash_state();
            terminal_leave_live();
            return 1;
        }
        if (op == 0xe7feu) break;
        if (try_accelerate_delay_loop(&cpu)) continue;
        if (try_accelerate_memory_delay_loop(&cpu)) {
            spi_service(1);
            if (path_is_frame_capture(image_path)) {
                write_frame_output(image_path);
                if (g_frame_index >= g_target_frames) break;
                g_frame_ready = 0;
                continue;
            }
            break;
        }
        if (try_accelerate_timer_wait_loop(&cpu)) continue;
        if (try_accelerate_timer_compare_loop(&cpu)) continue;
        if (try_accelerate_timer_poll_branch(&cpu)) continue;
        if (step(&cpu) != 0) {
            out("emulation stopped at step "); out_hex(cpu.steps);
            out(" pc="); out_pc(pc);
            out(" op="); out_hex(op);
            out(" r0="); out_hex(cpu.r[0]); out(" r1="); out_hex(cpu.r[1]);
            out(" r2="); out_hex(cpu.r[2]); out(" r3="); out_hex(cpu.r[3]);
            out(" sp="); out_hex(cpu.r[13]); out(" lr="); out_hex(cpu.r[14]);
            out("\n");
            report_lcd_milestones();
            save_flash_state();
            terminal_leave_live();
            return 1;
        }
        cpu.steps += 1u;
        g_cycles += 1u;
        peripherals_tick(&cpu);
        (void)service_interrupts(&cpu);
    }
    spi_service(1);
    if (g_frame_ready) {
        out("frame ready at simulated ms="); out_hex(g_sim_ms); out(" pc="); out_pc(cpu.r[15]); out("\n");
    } else if (cpu.steps >= g_max_steps) {
        out("frame budget reached pc="); out_pc(cpu.r[15]);
        out(" r0="); out_hex(cpu.r[0]); out(" r1="); out_hex(cpu.r[1]);
        out(" r2="); out_hex(cpu.r[2]); out(" r3="); out_hex(cpu.r[3]);
        out(" r4="); out_hex(cpu.r[4]); out(" r5="); out_hex(cpu.r[5]);
        out(" r6="); out_hex(cpu.r[6]); out(" r7="); out_hex(cpu.r[7]);
        out(" sp="); out_hex(cpu.r[13]); out(" lr="); out_hex(cpu.r[14]);
        out(" spi_tx="); out_hex((u32)g_spi.tx_count);
        out(" spi_rx="); out_hex((u32)g_spi.rx_count);
        out(" spi_busy_until="); out_hex(g_spi.busy_until);
        out(" cycles="); out_hex(g_cycles);
        report_pc_window(cpu.r[15]);
        out("\n");
    }
    if (!path_is_frame_capture(image_path) || g_frame_index == 0 || g_frame_index < g_target_frames) write_frame_output(image_path);
    if (gif_finish() != 0) out("gif write failure\n");
    report_lcd_milestones();
    if (g_fail_on_budget && cpu.steps >= g_max_steps) {
        save_flash_state();
        terminal_leave_live();
        out("step budget failure\n");
        return 1;
    }
    if (g_expect_hash && g_last_output_hash != g_expected_hash) {
        save_flash_state();
        terminal_leave_live();
        out("hash mismatch expected="); out_hex(g_expected_hash); out(" actual="); out_hex(g_last_output_hash); out("\n");
        return 1;
    }
    save_flash_state();
    terminal_leave_live();
    out("emulated "); out_hex(cpu.steps); out(" steps, wrote "); out(image_path); out(" hash="); out_hex(g_last_output_hash); out("\n");
    return 0;
}

__attribute__((used)) int emu_main(int argc, char **argv) {
    const char *key_script = 0;
    const char *trace_path = 0;
    int frames = 1;
    int i;
    g_trace_fd = -1;
    g_trace_mask = 0;
    g_expect_hash = 0;
    g_live_terminal = 0;
    g_fail_on_budget = 0;
    g_report_milestones = 0;
    g_input_base = APP_BASE;
    g_vector_base = APP_BASE;
    g_flash_state_path = 0;
    g_cyw43_wifi_fw_path = 0;
    g_cyw43_bt_fw_path = 0;
    g_cyw43_nvram_path = 0;
    g_cyw43_inventory = 0;
    g_cyw43_model = 0;
    g_max_steps = 20000000u;
    g_gif_fps = 15;
    for (i = 1; i < argc; ++i) {
        if (str_eq(argv[i], "--cyw43-inventory")) g_cyw43_inventory = 1;
        else if (str_eq(argv[i], "--cyw43-model")) g_cyw43_model = 1;
        else if (str_starts(argv[i], "--cyw43-wifi-fw=")) g_cyw43_wifi_fw_path = argv[i] + 16;
        else if (str_starts(argv[i], "--cyw43-bt-fw=")) g_cyw43_bt_fw_path = argv[i] + 14;
        else if (str_starts(argv[i], "--cyw43-nvram=")) g_cyw43_nvram_path = argv[i] + 14;
    }
    if (argc < 3 && !g_cyw43_inventory) {
        out("usage: bin_emu input.bin output.png|output.gif [key-script|-|@file] [--frames=N] [--gif-fps=N] [--trace[=path]] [--trace-kinds=base,calls,unknown-mmio,xip,cyw43|all] [--expect-hash=HEX] [--max-steps=N] [--fail-on-budget] [--report-milestones] [--live-terminal] [--flash-start] [--flash-state=PATH] [--symbols=MAP] [--cyw43-model] [--cyw43-inventory --cyw43-wifi-fw=PATH --cyw43-bt-fw=PATH --cyw43-nvram=PATH]\n");
        return 1;
    }
    if (g_cyw43_inventory && (argc < 3 || argv[1][0] == '-')) {
        cyw43_inventory_report();
        return 0;
    }
    for (i = 3; i < argc; ++i) {
        if (str_starts(argv[i], "--frames=")) {
            if (!parse_dec(argv[i] + 9, &frames)) {
                out("invalid --frames value\n");
                return 1;
            }
        } else if (str_starts(argv[i], "--gif-fps=")) {
            if (!parse_dec(argv[i] + 10, &g_gif_fps) || g_gif_fps <= 0 || g_gif_fps > 100) {
                out("invalid --gif-fps value\n");
                return 1;
            }
        } else if (str_eq(argv[i], "--trace")) {
            trace_path = 0;
            g_trace_fd = 2;
            if (g_trace_mask == 0u) g_trace_mask = TRACE_BASE;
        } else if (str_starts(argv[i], "--trace=")) {
            trace_path = argv[i] + 8;
        } else if (str_starts(argv[i], "--trace-kinds=")) {
            if (!parse_trace_kinds(argv[i] + 14, &g_trace_mask)) {
                out("invalid --trace-kinds value\n");
                return 1;
            }
        } else if (str_starts(argv[i], "--expect-hash=")) {
            if (!parse_hex32(argv[i] + 14, &g_expected_hash)) {
                out("invalid --expect-hash value\n");
                return 1;
            }
            g_expect_hash = 1;
        } else if (str_starts(argv[i], "--max-steps=")) {
            int steps;
            if (!parse_dec(argv[i] + 12, &steps) || steps <= 0) {
                out("invalid --max-steps value\n");
                return 1;
            }
            g_max_steps = (u32)steps;
        } else if (str_eq(argv[i], "--fail-on-budget")) {
            g_fail_on_budget = 1;
        } else if (str_eq(argv[i], "--report-milestones")) {
            g_report_milestones = 1;
        } else if (str_eq(argv[i], "--live-terminal")) {
            g_live_terminal = 1;
        } else if (str_eq(argv[i], "--flash-start")) {
            g_input_base = FLASH_BASE;
            g_vector_base = FLASH_BASE + BOOT2_SIZE;
        } else if (str_starts(argv[i], "--flash-state=")) {
            g_flash_state_path = argv[i] + 14;
        } else if (str_starts(argv[i], "--symbols=")) {
            load_symbol_map(argv[i] + 10);
        } else if (str_eq(argv[i], "--cyw43-inventory") || str_eq(argv[i], "--cyw43-model") ||
                   str_starts(argv[i], "--cyw43-wifi-fw=") || str_starts(argv[i], "--cyw43-bt-fw=") ||
                   str_starts(argv[i], "--cyw43-nvram=")) {
        } else {
            key_script = argv[i];
        }
    }
    if (g_live_terminal && key_script == 0) key_script = "-";
    if (trace_path != 0) {
        g_trace_fd = (int)sys_openat(AT_FDCWD, trace_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (g_trace_fd < 0) {
            out("failed to open trace path\n");
            return 1;
        }
        if (g_trace_mask == 0u) g_trace_mask = TRACE_BASE;
    }
    if (g_cyw43_inventory) {
        if (g_trace_fd >= 0 && g_trace_mask == 0u) g_trace_mask = TRACE_CYW43;
        cyw43_inventory_report();
        if (argc < 3) return 0;
    }
    if (key_script != 0 && key_script[0] == '@') {
        if (load_key_file(key_script + 1) != 0) {
            out("failed to load key replay file\n");
            return 1;
        }
        key_script = g_key_file;
    }
    return run_bin(argv[1], argv[2], key_script, frames);
}

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "and $-16, %rsp\n"
        "call emu_main\n"
        "mov %eax, %edi\n"
        "call sys_exit\n");
}