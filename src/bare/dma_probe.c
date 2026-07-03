#include "picocalc_lcd_bare.h"
#include "rp2040_regs.h"

#define LCD_CS  13
#define LCD_DC  14
#define CMD_COLADDR 0x2au
#define CMD_PAGEADDR 0x2bu
#define CMD_MEMWRITE 0x2cu
#define DMA_W 48
#define DMA_H 36
#define SPI_SSPDR_ADDR (SPI1_BASE + 0x08u)

static uint8_t g_dma_pixels[DMA_W * DMA_H * 3u];

static void spi_finish_probe(void) {
    while ((SPI_SSPSR & SPI_SR_TFE) == 0u) {
    }
    while ((SPI_SSPSR & SPI_SR_BSY) != 0u) {
    }
    while ((SPI_SSPSR & SPI_SR_RNE) != 0u) {
        (void)SPI_SSPDR;
    }
    SPI_SSPICR = 3u;
}

static void spi_byte_probe(uint8_t byte) {
    while ((SPI_SSPSR & SPI_SR_TNF) == 0u) {
    }
    SPI_SSPDR = byte;
}

static void lcd_command_probe(uint8_t command) {
    gpio_put(LCD_DC, 0);
    gpio_put(LCD_CS, 0);
    spi_byte_probe(command);
    spi_finish_probe();
    gpio_put(LCD_CS, 1);
}

static void lcd_data_probe(const uint8_t *data, uint32_t count) {
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    while (count-- != 0u) spi_byte_probe(*data++);
    spi_finish_probe();
    gpio_put(LCD_CS, 1);
}

static void lcd_window_probe(int x1, int y1, int x2, int y2) {
    uint8_t coords[4];
    coords[0] = (uint8_t)(x1 >> 8);
    coords[1] = (uint8_t)x1;
    coords[2] = (uint8_t)(x2 >> 8);
    coords[3] = (uint8_t)x2;
    lcd_command_probe(CMD_COLADDR);
    lcd_data_probe(coords, sizeof(coords));
    coords[0] = (uint8_t)(y1 >> 8);
    coords[1] = (uint8_t)y1;
    coords[2] = (uint8_t)(y2 >> 8);
    coords[3] = (uint8_t)y2;
    lcd_command_probe(CMD_PAGEADDR);
    lcd_data_probe(coords, sizeof(coords));
    lcd_command_probe(CMD_MEMWRITE);
}

static void fill_dma_pixels(void) {
    int y;
    int x;
    for (y = 0; y < DMA_H; ++y) {
        for (x = 0; x < DMA_W; ++x) {
            uint32_t off = ((uint32_t)y * DMA_W + (uint32_t)x) * 3u;
            g_dma_pixels[off + 0u] = (uint8_t)(40 + x * 4);
            g_dma_pixels[off + 1u] = (uint8_t)(40 + y * 5);
            g_dma_pixels[off + 2u] = (uint8_t)(220 - x * 2);
        }
    }
}

static void dma_to_lcd_probe(void) {
    lcd_window_probe(136, 112, 136 + DMA_W - 1, 112 + DMA_H - 1);
    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);
    DMA_CH0_READ_ADDR = (uint32_t)g_dma_pixels;
    DMA_CH0_WRITE_ADDR = SPI_SSPDR_ADDR;
    DMA_CH0_TRANS_COUNT = sizeof(g_dma_pixels);
    DMA_CH0_CTRL_TRIG = DMA_CTRL_EN | DMA_CTRL_SIZE_8 | DMA_CTRL_INCR_READ | DMA_CTRL_DREQ_SPI1_TX;
    while ((DMA_CH0_CTRL_TRIG & DMA_CTRL_BUSY) != 0u) {
    }
    spi_finish_probe();
    gpio_put(LCD_CS, 1);
}

void bare_main(void) {
    fill_dma_pixels();
    picocalc_lcd_init();
    picocalc_lcd_clear(0x081018u);
    picocalc_lcd_puts_scale(28, 72, "DMA SPI TEST", 0xffffffu, 0x081018u, 1);
    picocalc_lcd_fill_rect(128, 104, 191, 155, 0xffffffu);
    dma_to_lcd_probe();
    picocalc_lcd_puts_scale(76, 184, "DMA BLOCK OK", 0x80ff80u, 0x081018u, 1);
    while (1) {
    }
}