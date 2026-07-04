#ifndef PICOCALC_EMU_STATE_H
#define PICOCALC_EMU_STATE_H

#include "gif_writer.h"
#include "png_writer.h"

#ifndef PICOCALC_HOST_NOLIBC_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long usize;

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
#endif

#ifndef RAM_SIZE
#define RAM_SIZE   0x00042000u
#endif
#ifndef LCD_W
#define LCD_W 320
#endif
#ifndef LCD_H
#define LCD_H 320
#endif
#ifndef IO_BANK0_SIZE
#define IO_BANK0_SIZE          0x00002000u
#endif
#ifndef PADS_BANK0_SIZE
#define PADS_BANK0_SIZE        0x00001000u
#endif
#ifndef CLOCKS_SIZE
#define CLOCKS_SIZE            0x00001000u
#endif
#ifndef XOSC_SIZE
#define XOSC_SIZE              0x00001000u
#endif
#ifndef PLL_SIZE
#define PLL_SIZE               0x00001000u
#endif
#ifndef RTC_SIZE
#define RTC_SIZE               0x00001000u
#endif
#ifndef UART_SIZE
#define UART_SIZE              0x00001000u
#endif
#ifndef PIO_SIZE
#define PIO_SIZE               0x00001000u
#endif
#ifndef LIVE_COLS
#define LIVE_COLS 80
#endif
#ifndef LIVE_ROWS
#define LIVE_ROWS 40
#endif

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
    u32 dmacr;
    u32 last_rx;
    u8 tx[8];
    u8 rx[8];
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
    u32 rx_tl;
    u32 tx_tl;
    u32 dma_cr;
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
    u8 flash[2u * 1024u * 1024u];
    usize flash_size;
    u8 ram[RAM_SIZE];
    u8 framebuffer[LCD_W * LCD_H * 3u];
    u8 png_work[PNG_RGB8_WORK_SIZE(LCD_W, LCD_H)];
    GifWriter gif;
    const char *key_script;
    char key_file[4096];
    usize key_script_pos;
    u32 cycles;
    u32 sim_ms;
    int frame_ready;
    int live_stdin;
    int trace_fd;
    u32 trace_mask;
    u32 frame_hash;
    u32 last_output_hash;
    u32 expected_hash;
    int expect_hash;
    int live_terminal;
    int fail_on_budget;
    int report_milestones;
    u32 max_steps;
    int gif_active;
    int gif_fd;
    int gif_fps;
    const char *flash_state_path;
    u32 current_pc;
    int frame_index;
    int target_frames;
    int seen_lcd_command;
    u32 first_lcd_command;
    u32 first_lcd_command_pc;
    u32 first_lcd_command_cycles;
    int seen_lcd_pixel;
    u32 first_lcd_pixel_x;
    u32 first_lcd_pixel_y;
    u32 first_lcd_pixel_rgb;
    u32 first_lcd_pixel_pc;
    u32 first_lcd_pixel_cycles;
    int seen_lcd_nonblack;
    u32 first_lcd_nonblack_x;
    u32 first_lcd_nonblack_y;
    u32 first_lcd_nonblack_rgb;
    u32 first_lcd_nonblack_pc;
    u32 first_lcd_nonblack_cycles;
    int skip_tick_after_exception_return;
    u32 resets_reset;
    u32 gpio_out;
    u32 gpio_oe;
    u32 sio_dividend;
    u32 sio_divisor;
    u32 sio_quotient;
    u32 sio_remainder;
    int sio_div_signed;
    int sio_div_dirty;
    u32 io_bank0[IO_BANK0_SIZE / 4u];
    u32 pads_bank0[PADS_BANK0_SIZE / 4u];
    u32 clocks[CLOCKS_SIZE / 4u];
    u32 xosc[XOSC_SIZE / 4u];
    u32 pll_sys[PLL_SIZE / 4u];
    u32 pll_usb[PLL_SIZE / 4u];
    u32 rtc[RTC_SIZE / 4u];
    u32 uart0[UART_SIZE / 4u];
    u32 pio0[PIO_SIZE / 4u];
    u32 pio1[PIO_SIZE / 4u];
    u32 xip_dr0_command;
    u32 xip_dr0_reads;
    Lcd lcd;
    Spi spi;
    I2c i2c;
    DmaChannel dma[12];
    Core core;
    Termios saved_termios;
    int saved_termios_valid;
    char live_buffer[LIVE_ROWS * LIVE_COLS * 32u + 256u];
} EmuState;

#endif