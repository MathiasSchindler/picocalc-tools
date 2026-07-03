#include "png_writer.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long usize;

#define FLASH_BASE 0x10000000u
#define APP_BASE   0x10032000u
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
#define RESET_SPI1             (1u << 17)
#define RESET_MODELED_MASK     (RESET_IO_BANK0 | RESET_I2C1 | RESET_PADS_BANK0 | RESET_SPI1)
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

#define SPI1_BASE              0x40040000u
#define SPI_SSPCR0             (SPI1_BASE + 0x00u)
#define SPI_SSPCR1             (SPI1_BASE + 0x04u)
#define SPI_SSPDR              (SPI1_BASE + 0x08u)
#define SPI_SSPSR              (SPI1_BASE + 0x0cu)
#define SPI_SSPCPSR            (SPI1_BASE + 0x10u)
#define SPI_SSPICR             (SPI1_BASE + 0x20u)
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
#define I2C_IC_CLR_TX_ABRT     (I2C1_BASE + 0x54u)
#define I2C_IC_CLR_STOP        (I2C1_BASE + 0x60u)
#define I2C_IC_ENABLE          (I2C1_BASE + 0x6cu)
#define I2C_IC_STATUS          (I2C1_BASE + 0x70u)
#define I2C_IC_ENABLE_STATUS   (I2C1_BASE + 0x78u)
#define I2C_IC_SDA_HOLD        (I2C1_BASE + 0x7cu)
#define I2C_IC_TX_ABRT         (I2C1_BASE + 0x80u)
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
#define SIO_SPINLOCK_BASE      0xd0000100u
#define SIO_SPINLOCK_END       0xd0000180u

#define BOOTROM_FUNC_TABLE     0x00000200u
#define BOOTROM_DATA_TABLE     0x00000300u
#define BOOTROM_LOOKUP_ADDR    0x00000100u
#define BOOTROM_SD_TABLE       0x00000402u
#define BOOTROM_SF_TABLE       0x00000482u
#define BOOTROM_FN_L3          0x00001100u
#define BOOTROM_FN_P3          0x00001110u
#define BOOTROM_FN_R3          0x00001120u
#define BOOTROM_FN_T3          0x00001130u
#define BOOTROM_FN_MS          0x00001140u
#define BOOTROM_FN_MC          0x00001150u
#define BOOTROM_FN_S4          0x00001160u
#define BOOTROM_FN_C4          0x00001170u
#define ROM_CODE_SD            0x00004453u
#define ROM_CODE_SF            0x00004653u
#define ROM_CODE_L3            0x0000334cu
#define ROM_CODE_P3            0x00003350u
#define ROM_CODE_R3            0x00003352u
#define ROM_CODE_T3            0x00003354u
#define ROM_CODE_MS            0x0000534du
#define ROM_CODE_MC            0x0000434du
#define ROM_CODE_S4            0x00003453u
#define ROM_CODE_C4            0x00003443u
#define BOOTROM_FN_IF          0x00001180u
#define BOOTROM_FN_EX          0x00001190u
#define BOOTROM_FN_FC          0x000011a0u
#define ROM_CODE_IF            0x00004649u
#define ROM_CODE_EX            0x00005845u
#define ROM_CODE_FC            0x00004346u

#define TIMER_BASE             0x40054000u
#define TIMERAWH               (TIMER_BASE + 0x24u)
#define TIMERAWL               (TIMER_BASE + 0x28u)

#define XIP_SSI_SR             0x18000028u

#define LCD_CS_PIN             13u
#define LCD_DC_PIN             14u
#define LCD_RST_PIN            15u
#define KBD_ADDR               0x1fu
#define KBD_REG_KEY            0x09u

#define TRACE_BASE             (1u << 0)
#define TRACE_CALLS            (1u << 1)
#define TRACE_UNKNOWN_MMIO     (1u << 2)


#define AT_FDCWD (-100)
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512
#define O_NONBLOCK 2048
#define F_GETFL 3
#define F_SETFL 4
#define TCGETS 0x5401u
#define TCSETS 0x5402u
#define TERM_ICANON 0000002u
#define TERM_ECHO 0000010u
#define TERM_VTIME 5
#define TERM_VMIN 6
#define LIVE_COLS 80
#define LIVE_ROWS 40

static u8 g_flash[2u * 1024u * 1024u];
static usize g_flash_size;
static u8 g_ram[RAM_SIZE];
static u8 g_framebuffer[LCD_W * LCD_H * 3u];
static u8 g_png_work[PNG_RGB8_WORK_SIZE(LCD_W, LCD_H)];
static const char *g_key_script;
static char g_key_file[4096];
static usize g_key_script_pos;
static u32 g_cycles;
static u32 g_sim_ms;
static int g_frame_ready;
static int g_live_stdin;
static int g_trace_fd;
static u32 g_trace_mask;
static u32 g_frame_hash;
static u32 g_last_output_hash;
static u32 g_expected_hash;
static int g_expect_hash;
static int g_live_terminal;
static int g_fail_on_budget;
static int g_report_milestones;
static u32 g_max_steps;
static u32 g_current_pc;
static int g_frame_index;
static int g_target_frames;
static int g_seen_lcd_command;
static u32 g_first_lcd_command;
static u32 g_first_lcd_command_pc;
static u32 g_first_lcd_command_cycles;
static int g_seen_lcd_pixel;
static u32 g_first_lcd_pixel_x;
static u32 g_first_lcd_pixel_y;
static u32 g_first_lcd_pixel_rgb;
static u32 g_first_lcd_pixel_pc;
static u32 g_first_lcd_pixel_cycles;
static int g_seen_lcd_nonblack;
static u32 g_first_lcd_nonblack_x;
static u32 g_first_lcd_nonblack_y;
static u32 g_first_lcd_nonblack_rgb;
static u32 g_first_lcd_nonblack_pc;
static u32 g_first_lcd_nonblack_cycles;
static u32 g_resets_reset;
static u32 g_gpio_out;
static u32 g_gpio_oe;
static u32 g_sio_dividend;
static u32 g_sio_divisor;
static u32 g_sio_quotient;
static u32 g_sio_remainder;
static int g_sio_div_signed;
static u32 g_io_bank0[IO_BANK0_SIZE / 4u];
static u32 g_pads_bank0[PADS_BANK0_SIZE / 4u];
static u32 g_clocks[CLOCKS_SIZE / 4u];
static u32 g_xosc[XOSC_SIZE / 4u];
static u32 g_pll_sys[PLL_SIZE / 4u];
static u32 g_pll_usb[PLL_SIZE / 4u];

typedef struct {
    u32 r[16];
    u32 n;
    u32 z;
    u32 c;
    u32 v;
    u32 primask;
    u32 ipsr;
    u32 steps;
} Cpu;

typedef struct {
    int dc;
    int cs;
    int sleep;
    int display_on;
    int invert;
    u8 madctl;
    u8 colmod;
    u8 command;
    u8 params[16];
    int param_count;
    int x0;
    int x1;
    int y0;
    int y1;
    int x;
    int y;
    u8 pixel[3];
    int pixel_count;
} Lcd;

typedef struct {
    u32 cr0;
    u32 cr1;
    u32 cpsr;
    u32 last_rx;
    u8 tx[SPI_FIFO_SIZE];
    u8 rx[SPI_FIFO_SIZE];
    int tx_head;
    int tx_count;
    int rx_head;
    int rx_count;
    int overrun;
    u32 busy_until;
} Spi;

typedef struct {
    u32 con;
    u32 tar;
    u32 enable;
    u32 fs_hcnt;
    u32 fs_lcnt;
    u32 sda_hold;
    u32 fs_spklen;
    u32 tx_abrt;
    u32 raw_intr;
    u8 selected_reg;
    u8 rx[8];
    int rx_head;
    int rx_count;
    int tx_level;
    int tx_overflow;
    int key_report_index;
    int key_report_key;
    u32 busy_until;
} I2c;

typedef struct {
    u32 read_addr;
    u32 write_addr;
    u32 transfer_count;
    u32 ctrl_trig;
} DmaChannel;

typedef struct {
    u32 vtor;
    u32 icsr;
    u32 nvic_enable;
    u32 nvic_pending;
    u32 syst_csr;
    u32 syst_rvr;
    u32 syst_cvr;
} Core;

typedef struct {
    u32 c_iflag;
    u32 c_oflag;
    u32 c_cflag;
    u32 c_lflag;
    u8 c_line;
    u8 c_cc[32];
    u32 c_ispeed;
    u32 c_ospeed;
} Termios;

static Lcd g_lcd;
static Spi g_spi;
static I2c g_i2c;
static DmaChannel g_dma[12];
static Core g_core;
static Termios g_saved_termios;
static int g_saved_termios_valid;
static char g_live_buffer[LIVE_ROWS * LIVE_COLS * 32u + 256u];

static void lcd_spi_byte(u8 byte);
static int hex_value(char ch);
static u32 mem_read32(u32 addr);
static u16 mem_read16(u32 addr);
static u8 mem_read8(u32 addr);
static void mem_write32(u32 addr, u32 value);
static void mem_write16(u32 addr, u32 value);
static void mem_write8(u32 addr, u32 value);

static long sys_read(int fd, void *data, usize count) {
    long result;
    register long n __asm__("rax") = 0;
    register long a0 __asm__("rdi") = fd;
    register void *a1 __asm__("rsi") = data;
    register usize a2 __asm__("rdx") = count;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static long sys_write(int fd, const void *data, usize count) {
    long result;
    register long n __asm__("rax") = 1;
    register long a0 __asm__("rdi") = fd;
    register const void *a1 __asm__("rsi") = data;
    register usize a2 __asm__("rdx") = count;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static long sys_openat(int dirfd, const char *path, int flags, int mode) {
    long result;
    register long n __asm__("rax") = 257;
    register long a0 __asm__("rdi") = dirfd;
    register const char *a1 __asm__("rsi") = path;
    register long a2 __asm__("rdx") = flags;
    register long a3 __asm__("r10") = mode;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2), "r"(a3) : "rcx", "r11", "memory");
    return result;
}

static long sys_fcntl(int fd, int cmd, long arg) {
    long result;
    register long n __asm__("rax") = 72;
    register long a0 __asm__("rdi") = fd;
    register long a1 __asm__("rsi") = cmd;
    register long a2 __asm__("rdx") = arg;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

static long sys_ioctl(int fd, unsigned long request, void *arg) {
    long result;
    register long n __asm__("rax") = 16;
    register long a0 __asm__("rdi") = fd;
    register unsigned long a1 __asm__("rsi") = request;
    register void *a2 __asm__("rdx") = arg;
    __asm__ volatile("syscall" : "=a"(result) : "a"(n), "r"(a0), "r"(a1), "r"(a2) : "rcx", "r11", "memory");
    return result;
}

void sys_exit(int status) {
    register long n __asm__("rax") = 60;
    register long a0 __asm__("rdi") = status;
    __asm__ volatile("syscall" : : "a"(n), "r"(a0) : "rcx", "r11", "memory");
    while (1) {}
}

static void out(const char *s) {
    usize n = 0;
    while (s[n] != 0) n += 1u;
    (void)sys_write(1, s, n);
}

static void out_hex(u32 value) {
    char buf[10];
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i) {
        u32 nibble = (value >> (28 - i * 4)) & 15u;
        buf[2 + i] = (char)(nibble < 10u ? '0' + nibble : 'a' + nibble - 10u);
    }
    (void)sys_write(1, buf, sizeof(buf));
}

static usize str_len(const char *s) {
    usize n = 0;
    while (s != 0 && s[n] != 0) n += 1u;
    return n;
}

static int str_eq(const char *a, const char *b) {
    usize i = 0;
    while (a[i] != 0 && b[i] != 0) {
        if (a[i] != b[i]) return 0;
        i += 1u;
    }
    return a[i] == b[i];
}

static int str_starts(const char *s, const char *prefix) {
    usize i = 0;
    while (prefix[i] != 0) {
        if (s[i] != prefix[i]) return 0;
        i += 1u;
    }
    return 1;
}

static int str_ends(const char *s, const char *suffix) {
    usize s_len = str_len(s);
    usize suffix_len = str_len(suffix);
    usize i;
    if (suffix_len > s_len) return 0;
    for (i = 0; i < suffix_len; ++i) {
        if (s[s_len - suffix_len + i] != suffix[i]) return 0;
    }
    return 1;
}

static int parse_dec(const char *s, int *out_value) {
    int value = 0;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        any = 1;
        s += 1;
    }
    if (!any || *s != 0) return 0;
    *out_value = value;
    return 1;
}

static int parse_hex32(const char *s, u32 *out_value) {
    u32 value = 0;
    int any = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s != 0) {
        int digit = hex_value(*s);
        if (digit < 0) return 0;
        value = (value << 4) | (u32)digit;
        any = 1;
        s += 1;
    }
    if (!any) return 0;
    *out_value = value;
    return 1;
}

static int token_eq(const char *start, usize len, const char *word) {
    usize i;
    for (i = 0; i < len; ++i) {
        if (word[i] == 0 || start[i] != word[i]) return 0;
    }
    return word[len] == 0;
}

static int parse_trace_kinds(const char *s, u32 *out_mask) {
    u32 mask = 0;
    usize start = 0;
    usize pos = 0;
    while (1) {
        char ch = s[pos];
        if (ch == ',' || ch == '+' || ch == 0) {
            usize len = pos - start;
            if (len == 0u) return 0;
            if (token_eq(s + start, len, "base")) mask |= TRACE_BASE;
            else if (token_eq(s + start, len, "calls")) mask |= TRACE_CALLS;
            else if (token_eq(s + start, len, "unknown-mmio")) mask |= TRACE_UNKNOWN_MMIO;
            else if (token_eq(s + start, len, "all")) mask |= TRACE_BASE | TRACE_CALLS | TRACE_UNKNOWN_MMIO;
            else return 0;
            if (ch == 0) break;
            start = pos + 1u;
        }
        pos += 1u;
    }
    *out_mask = mask;
    return 1;
}

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

static void report_lcd_milestones(void) {
    if (!g_report_milestones) return;
    out("lcd milestones:");
    if (g_seen_lcd_command) {
        out(" first-cmd="); out_hex(g_first_lcd_command);
        out(" pc="); out_hex(g_first_lcd_command_pc);
        out(" cycles="); out_hex(g_first_lcd_command_cycles);
    } else {
        out(" first-cmd=none");
    }
    if (g_seen_lcd_pixel) {
        out(" first-pixel x="); out_hex(g_first_lcd_pixel_x);
        out(" y="); out_hex(g_first_lcd_pixel_y);
        out(" rgb="); out_hex(g_first_lcd_pixel_rgb);
        out(" pc="); out_hex(g_first_lcd_pixel_pc);
        out(" cycles="); out_hex(g_first_lcd_pixel_cycles);
    } else {
        out(" first-pixel=none");
    }
    if (g_seen_lcd_nonblack) {
        out(" first-nonblack x="); out_hex(g_first_lcd_nonblack_x);
        out(" y="); out_hex(g_first_lcd_nonblack_y);
        out(" rgb="); out_hex(g_first_lcd_nonblack_rgb);
        out(" pc="); out_hex(g_first_lcd_nonblack_pc);
        out(" cycles="); out_hex(g_first_lcd_nonblack_cycles);
    } else {
        out(" first-nonblack=none");
    }
    out("\n");
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

static void trace_text(const char *s) {
    if (g_trace_fd < 0 || g_trace_mask == 0u) return;
    (void)sys_write(g_trace_fd, s, str_len(s));
}

static int trace_enabled(u32 kind) {
    return g_trace_fd >= 0 && (g_trace_mask & kind) != 0u;
}

static void trace_hex32(u32 value) {
    char buf[10];
    int i;
    if (g_trace_fd < 0 || g_trace_mask == 0u) return;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i) {
        u32 nibble = (value >> (28 - i * 4)) & 15u;
        buf[2 + i] = (char)(nibble < 10u ? '0' + nibble : 'a' + nibble - 10u);
    }
    (void)sys_write(g_trace_fd, buf, sizeof(buf));
}

static void trace_pair(const char *name, u32 value) {
    if (g_trace_fd < 0 || g_trace_mask == 0u) return;
    trace_text(name);
    trace_text("=");
    trace_hex32(value);
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

static int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
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

static u32 read_le32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u16 read_le16(const u8 *p) {
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static void write_le32(u8 *p, u32 value) {
    p[0] = (u8)value;
    p[1] = (u8)(value >> 8);
    p[2] = (u8)(value >> 16);
    p[3] = (u8)(value >> 24);
}

static int flash_offset(u32 addr, usize *out_off) {
    if (addr < APP_BASE) return 0;
    *out_off = (usize)(addr - APP_BASE);
    return *out_off < g_flash_size;
}

static int ram_offset(u32 addr, usize *out_off) {
    if (addr < RAM_BASE) return 0;
    *out_off = (usize)(addr - RAM_BASE);
    return *out_off < RAM_SIZE;
}

static int executable_addr(u32 addr) {
    usize off;
    if ((addr & 1u) != 0u) return 0;
    if (flash_offset(addr, &off) && off + 2u <= g_flash_size) return 1;
    if (ram_offset(addr, &off) && off + 2u <= RAM_SIZE) return 1;
    return 0;
}

static int bootrom_read8(u32 addr, u8 *out_value) {
    u32 value;
    if (addr >= BOOTROM_SD_TABLE - 2u && addr < BOOTROM_SD_TABLE + 128u) { *out_value = 0; return 1; }
    if (addr >= BOOTROM_SF_TABLE - 2u && addr < BOOTROM_SF_TABLE + 128u) { *out_value = 0; return 1; }
    if (addr >= 0x14u && addr < 0x1au) {
        if (addr < 0x16u) value = BOOTROM_FUNC_TABLE;
        else if (addr < 0x18u) value = BOOTROM_DATA_TABLE;
        else value = BOOTROM_LOOKUP_ADDR | 1u;
        *out_value = (u8)(value >> ((addr & 1u) * 8u));
        return 1;
    }
    return 0;
}

static int bootrom_read16(u32 addr, u16 *out_value) {
    u8 lo;
    u8 hi;
    if (!bootrom_read8(addr, &lo)) return 0;
    if (!bootrom_read8(addr + 1u, &hi)) return 0;
    *out_value = (u16)((u16)lo | ((u16)hi << 8));
    return 1;
}

static int bootrom_read32(u32 addr, u32 *out_value) {
    u16 lo;
    u16 hi;
    if (!bootrom_read16(addr, &lo)) return 0;
    if (!bootrom_read16(addr + 2u, &hi)) return 0;
    *out_value = (u32)lo | ((u32)hi << 16);
    return 1;
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

static void trace_mmio(const char *kind, u32 addr, u32 value) {
    if (!trace_enabled(TRACE_BASE)) return;
    trace_text(kind);
    trace_text(" ");
    trace_pair("addr", addr);
    trace_text(" ");
    trace_pair("value", value);
    trace_text(" cycles=");
    trace_hex32(g_cycles);
    trace_text("\n");
}

static void trace_unknown_mmio(const char *kind, u32 addr, u32 value, u32 pc) {
    if (!trace_enabled(TRACE_UNKNOWN_MMIO)) return;
    if (pc == 0u) pc = g_current_pc;
    trace_text(kind);
    trace_text(" ");
    trace_pair("addr", addr);
    trace_text(" ");
    trace_pair("value", value);
    trace_text(" ");
    trace_pair("pc", pc);
    trace_text(" cycles=");
    trace_hex32(g_cycles);
    trace_text("\n");
}

static void trace_branch(const char *kind, u32 pc, u32 target, u32 lr, u16 op) {
    if (!trace_enabled(TRACE_CALLS)) return;
    trace_text(kind);
    trace_text(" ");
    trace_pair("pc", pc);
    trace_text(" ");
    trace_pair("target", target);
    trace_text(" ");
    trace_pair("lr", lr);
    trace_text(" ");
    trace_pair("op", (u32)op);
    trace_text(" cycles=");
    trace_hex32(g_cycles);
    trace_text("\n");
}

static int bootrom_lookup_call(Cpu *cpu, u32 return_pc) {
    u32 table = cpu->r[0];
    u32 code = cpu->r[1] & 0xffffu;
    u32 result = 0;
    if (table == BOOTROM_DATA_TABLE) {
        if (code == ROM_CODE_SD) result = BOOTROM_SD_TABLE;
        else if (code == ROM_CODE_SF) result = BOOTROM_SF_TABLE;
    } else if (table == BOOTROM_FUNC_TABLE) {
        if (code == ROM_CODE_L3) result = BOOTROM_FN_L3 | 1u;
        else if (code == ROM_CODE_P3) result = BOOTROM_FN_P3 | 1u;
        else if (code == ROM_CODE_R3) result = BOOTROM_FN_R3 | 1u;
        else if (code == ROM_CODE_T3) result = BOOTROM_FN_T3 | 1u;
        else if (code == ROM_CODE_MS) result = BOOTROM_FN_MS | 1u;
        else if (code == ROM_CODE_MC) result = BOOTROM_FN_MC | 1u;
        else if (code == ROM_CODE_S4) result = BOOTROM_FN_S4 | 1u;
        else if (code == ROM_CODE_C4) result = BOOTROM_FN_C4 | 1u;
        else if (code == ROM_CODE_IF) result = BOOTROM_FN_IF | 1u;
        else if (code == ROM_CODE_EX) result = BOOTROM_FN_EX | 1u;
        else if (code == ROM_CODE_FC) result = BOOTROM_FN_FC | 1u;
    }
    cpu->r[0] = result;
    cpu->r[15] = return_pc;
    if (trace_enabled(TRACE_CALLS)) {
        trace_text("bootrom lookup table="); trace_hex32(table);
        trace_text(" code="); trace_hex32(code);
        trace_text(" result="); trace_hex32(result);
        trace_text("\n");
    }
    return 1;
}

static u32 count_leading_zeroes(u32 value) {
    u32 count = 0;
    u32 bit = 0x80000000u;
    while (bit != 0u && (value & bit) == 0u) { count += 1u; bit >>= 1; }
    return count;
}

static u32 count_trailing_zeroes(u32 value) {
    u32 count = 0;
    u32 bit = 1u;
    while (bit != 0u && (value & bit) == 0u) { count += 1u; bit <<= 1; }
    return count;
}

static u32 count_bits(u32 value) {
    u32 count = 0;
    while (value != 0u) { count += value & 1u; value >>= 1; }
    return count;
}

static u32 reverse_bits(u32 value) {
    u32 out_value = 0;
    int i;
    for (i = 0; i < 32; ++i) {
        out_value = (out_value << 1) | (value & 1u);
        value >>= 1;
    }
    return out_value;
}

static int bootrom_function_call(Cpu *cpu, u32 target, u32 return_pc) {
    u32 dst;
    u32 src;
    u32 len;
    u32 i;
    if (target == BOOTROM_FN_L3) cpu->r[0] = count_leading_zeroes(cpu->r[0]);
    else if (target == BOOTROM_FN_P3) cpu->r[0] = count_bits(cpu->r[0]);
    else if (target == BOOTROM_FN_R3) cpu->r[0] = reverse_bits(cpu->r[0]);
    else if (target == BOOTROM_FN_T3) cpu->r[0] = count_trailing_zeroes(cpu->r[0]);
    else if (target == BOOTROM_FN_MS || target == BOOTROM_FN_S4) {
        dst = cpu->r[0];
        len = cpu->r[2];
        if (target == BOOTROM_FN_S4) {
            for (i = 0; i < len; i += 4u) mem_write32(dst + i, cpu->r[1]);
        } else {
            for (i = 0; i < len; ++i) mem_write8(dst + i, cpu->r[1]);
        }
    } else if (target == BOOTROM_FN_MC || target == BOOTROM_FN_C4) {
        dst = cpu->r[0];
        src = cpu->r[1];
        len = cpu->r[2];
        if (target == BOOTROM_FN_C4) {
            for (i = 0; i < len; i += 4u) mem_write32(dst + i, mem_read32(src + i));
        } else {
            for (i = 0; i < len; ++i) mem_write8(dst + i, mem_read8(src + i));
        }
    } else if (target == BOOTROM_FN_IF || target == BOOTROM_FN_EX || target == BOOTROM_FN_FC) {
        cpu->r[0] = 0;
    } else return 0;
    cpu->r[15] = return_pc;
    if (trace_enabled(TRACE_CALLS)) {
        trace_text("bootrom fn target="); trace_hex32(target);
        trace_text(" return="); trace_hex32(return_pc);
        trace_text("\n");
    }
    return 1;
}

static void spi_rx_push(u8 byte) {
    if (g_spi.rx_count < SPI_FIFO_SIZE) {
        g_spi.rx[(g_spi.rx_head + g_spi.rx_count) & (SPI_FIFO_SIZE - 1)] = byte;
        g_spi.rx_count += 1;
        g_spi.last_rx = byte;
    } else {
        g_spi.overrun = 1;
    }
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
    if (limit > 0x100000u) limit = 0x100000u;
    dma->ctrl_trig |= DMA_CTRL_BUSY;
    while (limit > 0u) {
        u32 value;
        if (write_addr == SPI_SSPDR && dreq == DREQ_SPI1_TX) {
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
        if (write_addr == SPI_SSPDR && dreq == DREQ_SPI1_TX) spi_service(0);
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

static u32 mmio_read32(u32 addr) {
    u32 value;
    if (dma_read32(addr, &value)) return value;
    if (clock_read32(addr, &value)) return value;
    if (register_bank_read(addr, IO_BANK0_BASE, IO_BANK0_SIZE, g_io_bank0, &value)) return value;
    if (register_bank_read(addr, PADS_BANK0_BASE, PADS_BANK0_SIZE, g_pads_bank0, &value)) return value;
    if (addr == PPB_SYST_CSR) return g_core.syst_csr;
    if (addr == PPB_SYST_RVR) return g_core.syst_rvr;
    if (addr == PPB_SYST_CVR) return g_core.syst_cvr;
    if (addr == PPB_NVIC_ISER) return g_core.nvic_enable;
    if (addr == PPB_NVIC_ISPR) return g_core.nvic_pending;
    if (addr == PPB_SCB_ICSR) return g_core.icsr | (g_core.nvic_pending != 0u ? (1u << 22) : 0u);
    if (addr == PPB_SCB_VTOR) return g_core.vtor;
    if (addr == TIMERAWH) return 0u;
    if (addr == TIMERAWL) return g_cycles;
    if (addr == XIP_SSI_SR) return 0x0du;
    if (addr == SIO_CPUID) return 0u;
    if (addr == SIO_DIV_UDIVIDEND || addr == SIO_DIV_SDIVIDEND) return g_sio_dividend;
    if (addr == SIO_DIV_UDIVISOR || addr == SIO_DIV_SDIVISOR) return g_sio_divisor;
    if (addr == SIO_DIV_QUOTIENT) return g_sio_quotient;
    if (addr == SIO_DIV_REMAINDER) return g_sio_remainder;
    if (addr == SIO_DIV_CSR) return SIO_DIV_CSR_READY;
    if (addr >= SIO_SPINLOCK_BASE && addr < SIO_SPINLOCK_END && (addr & 3u) == 0u) return 1u;
    if (addr == RESETS_RESET) return g_resets_reset;
    if (addr == RESETS_DONE) return RESET_DONE_MASK & ~g_resets_reset;
    if (addr == SPI_SSPCR0) return g_spi.cr0;
    if (addr == SPI_SSPCR1) return g_spi.cr1;
    if (addr == SPI_SSPDR) return spi_read_data();
    if (addr == SPI_SSPSR) return spi_status();
    if (addr == SPI_SSPCPSR) return g_spi.cpsr;
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
    if (addr == I2C_IC_RAW_INTR) return g_i2c.raw_intr | I2C_RAW_TX_EMPTY;
    if (addr == I2C_IC_CLR_TX_ABRT) { g_i2c.raw_intr &= ~I2C_RAW_TX_ABRT; return 0; }
    if (addr == I2C_IC_CLR_STOP) { g_i2c.raw_intr &= ~I2C_RAW_STOP_DET; return 0; }
    if (addr == I2C_IC_ENABLE) return g_i2c.enable;
    if (addr == I2C_IC_ENABLE_STATUS) return g_i2c.enable & 1u;
    if (addr == I2C_IC_STATUS) return (g_i2c.tx_level == 0 ? I2C_STATUS_TFE : 0u) |
        (g_i2c.tx_level < I2C_FIFO_SIZE ? I2C_STATUS_TFNF : 0u) |
        (g_i2c.rx_count > 0 ? I2C_STATUS_RFNE : 0u);
    if (addr == I2C_IC_SDA_HOLD) return g_i2c.sda_hold;
    if (addr == I2C_IC_TX_ABRT) return g_i2c.tx_abrt;
    if (addr == I2C_IC_FS_SPKLEN) return g_i2c.fs_spklen;
    trace_unknown_mmio("mmior", addr, 0u, 0u);
    return 0u;
}

static void sio_div_update(void) {
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

static void lcd_set_pixel(int x, int y, u8 r, u8 g, u8 b) {
    usize off;
    u32 rgb;
    if (x < 0 || y < 0 || x >= LCD_W || y >= LCD_H) return;
    rgb = ((u32)r << 16) | ((u32)g << 8) | (u32)b;
    if (!g_seen_lcd_pixel) {
        g_seen_lcd_pixel = 1;
        g_first_lcd_pixel_x = (u32)x;
        g_first_lcd_pixel_y = (u32)y;
        g_first_lcd_pixel_rgb = rgb;
        g_first_lcd_pixel_pc = g_current_pc;
        g_first_lcd_pixel_cycles = g_cycles;
    }
    if (rgb != 0u && !g_seen_lcd_nonblack) {
        g_seen_lcd_nonblack = 1;
        g_first_lcd_nonblack_x = (u32)x;
        g_first_lcd_nonblack_y = (u32)y;
        g_first_lcd_nonblack_rgb = rgb;
        g_first_lcd_nonblack_pc = g_current_pc;
        g_first_lcd_nonblack_cycles = g_cycles;
    }
    off = ((usize)y * LCD_W + (usize)x) * 3u;
    g_framebuffer[off] = r;
    g_framebuffer[off + 1u] = g;
    g_framebuffer[off + 2u] = b;
}

static int param_be16(int off) {
    return ((int)g_lcd.params[off] << 8) | (int)g_lcd.params[off + 1];
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

static void lcd_apply_params(void) {
    if (g_lcd.command == 0x2au) {
        g_lcd.x0 = param_be16(0);
        g_lcd.x1 = param_be16(2);
    } else if (g_lcd.command == 0x2bu) {
        g_lcd.y0 = param_be16(0);
        g_lcd.y1 = param_be16(2);
    } else if (g_lcd.command == 0x36u) {
        g_lcd.madctl = g_lcd.params[0];
    } else if (g_lcd.command == 0x3au) {
        g_lcd.colmod = g_lcd.params[0];
    }
    if (g_trace_fd >= 0) {
        trace_text("lcd params cmd="); trace_hex32(g_lcd.command);
        trace_text(" count="); trace_hex32((u32)g_lcd.param_count);
        trace_text("\n");
    }
}

static void lcd_gpio_sync(void) {
    g_lcd.cs = (g_gpio_out & (1u << LCD_CS_PIN)) != 0u;
    g_lcd.dc = (g_gpio_out & (1u << LCD_DC_PIN)) != 0u;
}

static void lcd_data(u8 byte) {
    int expected = lcd_expected_params(g_lcd.command);
    if (expected > 0) {
        if (g_lcd.param_count < (int)sizeof(g_lcd.params)) g_lcd.params[g_lcd.param_count++] = byte;
        if (g_lcd.param_count == expected) lcd_apply_params();
        return;
    }
    if (g_lcd.command == 0x2cu) {
        g_lcd.pixel[g_lcd.pixel_count++] = byte;
        if (g_lcd.pixel_count == 3) {
            lcd_set_pixel(g_lcd.x, g_lcd.y, g_lcd.pixel[0], g_lcd.pixel[1], g_lcd.pixel[2]);
            g_frame_hash = (g_frame_hash * 16777619u) ^ ((u32)g_lcd.pixel[0] << 16) ^ ((u32)g_lcd.pixel[1] << 8) ^ g_lcd.pixel[2];
            g_lcd.pixel_count = 0;
            g_lcd.x += 1;
            if (g_lcd.x > g_lcd.x1) {
                g_lcd.x = g_lcd.x0;
                g_lcd.y += 1;
                if (g_lcd.y > g_lcd.y1) g_lcd.y = g_lcd.y0;
            }
        }
    }
}

static void lcd_command(u8 command) {
    g_lcd.command = command;
    g_lcd.param_count = 0;
    g_lcd.pixel_count = 0;
    if (!g_seen_lcd_command) {
        g_seen_lcd_command = 1;
        g_first_lcd_command = command;
        g_first_lcd_command_pc = g_current_pc;
        g_first_lcd_command_cycles = g_cycles;
    }
    if (g_trace_fd >= 0) {
        trace_text("lcd cmd="); trace_hex32(command); trace_text(" cycles="); trace_hex32(g_cycles); trace_text("\n");
    }
    if (command == 0x2cu) {
        g_lcd.x = g_lcd.x0;
        g_lcd.y = g_lcd.y0;
    } else if (command == 0x10u) {
        g_lcd.sleep = 1;
    } else if (command == 0x11u) {
        g_lcd.sleep = 0;
    } else if (command == 0x20u) {
        g_lcd.invert = 0;
    } else if (command == 0x21u) {
        g_lcd.invert = 1;
    } else if (command == 0x28u) {
        g_lcd.display_on = 0;
    } else if (command == 0x29u) {
        g_lcd.display_on = 1;
    } else if (command == 0x01u) {
        g_lcd.sleep = 1;
        g_lcd.display_on = 0;
        g_lcd.invert = 0;
        g_lcd.madctl = 0;
        g_lcd.colmod = 0;
        g_lcd.x0 = 0;
        g_lcd.x1 = LCD_W - 1;
        g_lcd.y0 = 0;
        g_lcd.y1 = LCD_H - 1;
    }
}

static void mmio_write32(u32 addr, u32 value) {
    if (dma_write32(addr, value)) return;
    if (clock_write32(addr, value)) { trace_mmio("clkw", addr, value); return; }
    if (register_bank_write(addr, value, IO_BANK0_BASE, IO_BANK0_SIZE, g_io_bank0)) return;
    if (register_bank_write(addr, value, PADS_BANK0_BASE, PADS_BANK0_SIZE, g_pads_bank0)) return;
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
        lcd_gpio_sync();
        return;
    }
    if (addr == SIO_GPIO_OUT_CLR) {
        g_gpio_out &= ~value;
        lcd_gpio_sync();
        return;
    }
    if (addr == SIO_GPIO_OE_SET) { g_gpio_oe |= value; return; }
    if (addr == SIO_GPIO_OE_CLR) { g_gpio_oe &= ~value; return; }
    if (addr == SIO_DIV_UDIVIDEND) { g_sio_div_signed = 0; g_sio_dividend = value; sio_div_update(); return; }
    if (addr == SIO_DIV_UDIVISOR) { g_sio_div_signed = 0; g_sio_divisor = value; sio_div_update(); return; }
    if (addr == SIO_DIV_SDIVIDEND) { g_sio_div_signed = 1; g_sio_dividend = value; sio_div_update(); return; }
    if (addr == SIO_DIV_SDIVISOR) { g_sio_div_signed = 1; g_sio_divisor = value; sio_div_update(); return; }
    if (addr >= SIO_SPINLOCK_BASE && addr < SIO_SPINLOCK_END && (addr & 3u) == 0u) return;
    if (addr == SPI_SSPCR0) { g_spi.cr0 = value; return; }
    if (addr == SPI_SSPCR1) { g_spi.cr1 = value; return; }
    if (addr == SPI_SSPCPSR) { g_spi.cpsr = value; return; }
    if (addr == SPI_SSPICR) { g_spi.overrun = 0; return; }
    if (addr == SPI_SSPDR) {
        spi_write_data((u8)value);
        return;
    }
    if (addr == I2C_IC_CON) { g_i2c.con = value; return; }
    if (addr == I2C_IC_TAR) { g_i2c.tar = value; return; }
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
    if (addr == I2C_IC_ENABLE) { g_i2c.enable = value; return; }
    if (addr == I2C_IC_SDA_HOLD) { g_i2c.sda_hold = value; return; }
    if (addr == I2C_IC_FS_SPKLEN) { g_i2c.fs_spklen = value; return; }
    trace_unknown_mmio("mmiow", addr, value, 0u);
}

static void lcd_spi_byte(u8 byte) {
    if (g_lcd.cs != 0) return;
    if (g_lcd.dc == 0) lcd_command(byte);
    else lcd_data(byte);
}

static u32 mem_read32(u32 addr) {
    usize off;
    u32 value;
    if (flash_offset(addr, &off) && off + 4u <= g_flash_size) return read_le32(g_flash + off);
    if (ram_offset(addr, &off) && off + 4u <= RAM_SIZE) return read_le32(g_ram + off);
    if (bootrom_read32(addr, &value)) return value;
    return mmio_read32(addr);
}

static u16 mem_read16(u32 addr) {
    usize off;
    u16 value;
    if (flash_offset(addr, &off) && off + 2u <= g_flash_size) return read_le16(g_flash + off);
    if (ram_offset(addr, &off) && off + 2u <= RAM_SIZE) return read_le16(g_ram + off);
    if (bootrom_read16(addr, &value)) return value;
    return (u16)mem_read32(addr);
}

static u8 mem_read8(u32 addr) {
    usize off;
    u8 value;
    if (flash_offset(addr, &off) && off < g_flash_size) return g_flash[off];
    if (ram_offset(addr, &off) && off < RAM_SIZE) return g_ram[off];
    if (bootrom_read8(addr, &value)) return value;
    return (u8)mem_read32(addr);
}

static void mem_write32(u32 addr, u32 value) {
    usize off;
    if (ram_offset(addr, &off) && off + 4u <= RAM_SIZE) {
        write_le32(g_ram + off, value);
        return;
    }
    mmio_write32(addr, value);
}

static void mem_write16(u32 addr, u32 value) {
    usize off;
    if (ram_offset(addr, &off) && off + 2u <= RAM_SIZE) {
        g_ram[off] = (u8)value;
        g_ram[off + 1u] = (u8)(value >> 8);
        return;
    }
    mmio_write32(addr, value & 0xffffu);
}

static void mem_write8(u32 addr, u32 value) {
    usize off;
    if (ram_offset(addr, &off) && off < RAM_SIZE) {
        g_ram[off] = (u8)value;
        return;
    }
    mmio_write32(addr, value & 0xffu);
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

static u32 sx(u32 value, int bits) {
    u32 sign = 1u << (bits - 1);
    return (value ^ sign) - sign;
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
    if (g_trace_fd >= 0) { trace_text("exception return pc="); trace_hex32(cpu->r[15]); trace_text("\n"); }
    return 1;
}

static void peripherals_tick(Cpu *cpu) {
    (void)cpu;
    spi_service(0);
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

static int load_file(const char *path) {
    long fd = sys_openat(AT_FDCWD, path, O_RDONLY, 0);
    usize off = 0;
    if (fd < 0) return -1;
    while (off < sizeof(g_flash)) {
        long got = sys_read((int)fd, g_flash + off, sizeof(g_flash) - off);
        if (got < 0) return -1;
        if (got == 0) break;
        off += (usize)got;
    }
    g_flash_size = off;
    return off > 8u ? 0 : -1;
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

static void write_png(const char *path) {
    int fd = (int)sys_openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)png_write_rgb8(png_fd_write, &fd, g_framebuffer, LCD_W, LCD_H, g_png_work, sizeof(g_png_work));
}

static void write_image(const char *path) {
    if (str_ends(path, ".ppm")) write_ppm(path);
    else write_png(path);
}

static void write_frame_output(const char *image_path) {
    char frame_path[512];
    u32 hash = framebuffer_hash();
    g_last_output_hash = hash;
    if (path_has_frame_pattern(image_path)) {
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

static int run_bin(const char *in_path, const char *image_path, const char *key_script, int target_frames) {
    Cpu cpu;
    u32 sp;
    u32 reset;
    int i;
    for (i = 0; i < 16; ++i) cpu.r[i] = 0;
    cpu.n = cpu.z = cpu.c = cpu.v = cpu.primask = cpu.ipsr = cpu.steps = 0;
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
    g_resets_reset = 0;
    g_gpio_out = (1u << LCD_CS_PIN) | (1u << LCD_DC_PIN) | (1u << LCD_RST_PIN);
    g_gpio_oe = 0;
    g_sio_dividend = 0;
    g_sio_divisor = 0;
    g_sio_quotient = 0xffffffffu;
    g_sio_remainder = 0;
    g_sio_div_signed = 0;
    for (i = 0; i < (int)(IO_BANK0_SIZE / 4u); ++i) g_io_bank0[i] = 0;
    for (i = 0; i < (int)(PADS_BANK0_SIZE / 4u); ++i) g_pads_bank0[i] = 0;
    for (i = 0; i < (int)(CLOCKS_SIZE / 4u); ++i) g_clocks[i] = 0;
    for (i = 0; i < (int)(XOSC_SIZE / 4u); ++i) g_xosc[i] = 0;
    for (i = 0; i < (int)(PLL_SIZE / 4u); ++i) {
        g_pll_sys[i] = 0;
        g_pll_usb[i] = 0;
    }
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
    g_core.vtor = APP_BASE;
    g_core.icsr = 0;
    g_core.nvic_enable = 0;
    g_core.nvic_pending = 0;
    g_core.syst_csr = 0;
    g_core.syst_rvr = 0;
    g_core.syst_cvr = 0;
    g_saved_termios_valid = 0;
    lcd_gpio_sync();
    if (str_eq(key_script == 0 ? "" : key_script, "-")) {
        long flags = sys_fcntl(0, F_GETFL, 0);
        if (flags >= 0) (void)sys_fcntl(0, F_SETFL, flags | O_NONBLOCK);
        g_live_stdin = 1;
        g_key_script = 0;
    } else {
        g_live_stdin = 0;
    }
    terminal_enter_live();
    if (load_file(in_path) != 0) {
        out("failed to load input bin\n");
        terminal_leave_live();
        return 1;
    }
    sp = read_le32(g_flash);
    reset = read_le32(g_flash + 4u);
    if (sp != 0x20042000u || (reset & 1u) == 0u) {
        out("unexpected vector table sp="); out_hex(sp); out(" reset="); out_hex(reset); out("\n");
        terminal_leave_live();
        return 1;
    }
    cpu.r[13] = sp;
    cpu.r[15] = reset & ~1u;
    while (cpu.steps < g_max_steps) {
        u32 pc = cpu.r[15];
        u16 op = mem_read16(pc);
        if (!executable_addr(pc)) {
            out("invalid execute pc="); out_hex(pc); out("\n");
            report_lcd_milestones();
            terminal_leave_live();
            return 1;
        }
        if (op == 0xe7feu) break;
        if (try_accelerate_delay_loop(&cpu)) continue;
        if (try_accelerate_memory_delay_loop(&cpu)) {
            spi_service(1);
            if (path_has_frame_pattern(image_path)) {
                write_frame_output(image_path);
                if (g_frame_index >= g_target_frames) break;
                g_frame_ready = 0;
                continue;
            }
            break;
        }
        if (step(&cpu) != 0) {
            out("emulation stopped at step "); out_hex(cpu.steps);
            out(" pc="); out_hex(pc);
            out(" op="); out_hex(op);
            out(" r0="); out_hex(cpu.r[0]); out(" r1="); out_hex(cpu.r[1]);
            out(" r2="); out_hex(cpu.r[2]); out(" r3="); out_hex(cpu.r[3]);
            out(" sp="); out_hex(cpu.r[13]); out(" lr="); out_hex(cpu.r[14]);
            out("\n");
            report_lcd_milestones();
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
        out("frame ready at simulated ms="); out_hex(g_sim_ms); out(" pc="); out_hex(cpu.r[15]); out("\n");
    } else if (cpu.steps >= g_max_steps) {
        out("frame budget reached pc="); out_hex(cpu.r[15]);
        out(" r0="); out_hex(cpu.r[0]); out(" r1="); out_hex(cpu.r[1]);
        out(" r2="); out_hex(cpu.r[2]); out(" r3="); out_hex(cpu.r[3]);
        out(" r4="); out_hex(cpu.r[4]); out(" r5="); out_hex(cpu.r[5]);
        out(" r6="); out_hex(cpu.r[6]); out(" r7="); out_hex(cpu.r[7]);
        out(" sp="); out_hex(cpu.r[13]); out(" lr="); out_hex(cpu.r[14]);
        out("\n");
    }
    if (!path_has_frame_pattern(image_path) || g_frame_index == 0 || g_frame_index < g_target_frames) write_frame_output(image_path);
    report_lcd_milestones();
    if (g_fail_on_budget && cpu.steps >= g_max_steps) {
        terminal_leave_live();
        out("step budget failure\n");
        return 1;
    }
    if (g_expect_hash && g_last_output_hash != g_expected_hash) {
        terminal_leave_live();
        out("hash mismatch expected="); out_hex(g_expected_hash); out(" actual="); out_hex(g_last_output_hash); out("\n");
        return 1;
    }
    terminal_leave_live();
    out("emulated "); out_hex(cpu.steps); out(" steps, wrote "); out(image_path); out(" hash="); out_hex(g_last_output_hash); out("\n");
    return 0;
}

__attribute__((used)) int emu_main(int argc, char **argv) {
    const char *key_script = 0;
    const char *trace_path = 0;
    int frames = 1;
    int i;
    if (argc < 3) {
        out("usage: bin_emu input.bin output.png [key-script|-|@file] [--frames=N] [--trace[=path]] [--trace-kinds=base,calls,unknown-mmio|all] [--expect-hash=HEX] [--max-steps=N] [--fail-on-budget] [--report-milestones] [--live-terminal]\n");
        return 1;
    }
    g_trace_fd = -1;
    g_trace_mask = 0;
    g_expect_hash = 0;
    g_live_terminal = 0;
    g_fail_on_budget = 0;
    g_report_milestones = 0;
    g_max_steps = 20000000u;
    for (i = 3; i < argc; ++i) {
        if (str_starts(argv[i], "--frames=")) {
            if (!parse_dec(argv[i] + 9, &frames)) {
                out("invalid --frames value\n");
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