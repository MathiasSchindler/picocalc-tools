#ifndef PICOCALC_BARE_RP2040_REGS_H
#define PICOCALC_BARE_RP2040_REGS_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long size_t;

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RESETS_BASE       0x4000c000u
#define RESETS_RESET      REG32(RESETS_BASE + 0x00u)
#define RESETS_DONE       REG32(RESETS_BASE + 0x08u)

#define RESET_IO_BANK0    (1u << 5)
#define RESET_I2C1        (1u << 4)
#define RESET_PADS_BANK0  (1u << 8)
#define RESET_SPI1        (1u << 17)

#define IO_BANK0_BASE     0x40014000u
#define PADS_BANK0_BASE   0x4001c000u
#define SIO_BASE          0xd0000000u

#define GPIO_FUNC_SPI     1u
#define GPIO_FUNC_I2C     3u
#define GPIO_FUNC_SIO     5u

#define SIO_GPIO_OUT_SET  REG32(SIO_BASE + 0x14u)
#define SIO_GPIO_OUT_CLR  REG32(SIO_BASE + 0x18u)
#define SIO_GPIO_OE_SET   REG32(SIO_BASE + 0x24u)
#define SIO_GPIO_OE_CLR   REG32(SIO_BASE + 0x28u)

#define SPI1_BASE         0x40040000u
#define SPI_SSPCR0        REG32(SPI1_BASE + 0x00u)
#define SPI_SSPCR1        REG32(SPI1_BASE + 0x04u)
#define SPI_SSPDR         REG32(SPI1_BASE + 0x08u)
#define SPI_SSPSR         REG32(SPI1_BASE + 0x0cu)
#define SPI_SSPCPSR       REG32(SPI1_BASE + 0x10u)
#define SPI_SSPICR        REG32(SPI1_BASE + 0x20u)

#define SPI_SR_TFE        (1u << 0)
#define SPI_SR_TNF        (1u << 1)
#define SPI_SR_RNE        (1u << 2)
#define SPI_SR_BSY        (1u << 4)

#define I2C1_BASE         0x40048000u
#define I2C_IC_CON        REG32(I2C1_BASE + 0x00u)
#define I2C_IC_TAR        REG32(I2C1_BASE + 0x04u)
#define I2C_IC_DATA_CMD   REG32(I2C1_BASE + 0x10u)
#define I2C_IC_FS_HCNT    REG32(I2C1_BASE + 0x1cu)
#define I2C_IC_FS_LCNT    REG32(I2C1_BASE + 0x20u)
#define I2C_IC_RAW_INTR   REG32(I2C1_BASE + 0x34u)
#define I2C_IC_CLR_TX_ABRT REG32(I2C1_BASE + 0x54u)
#define I2C_IC_CLR_STOP   REG32(I2C1_BASE + 0x60u)
#define I2C_IC_ENABLE     REG32(I2C1_BASE + 0x6cu)
#define I2C_IC_STATUS     REG32(I2C1_BASE + 0x70u)
#define I2C_IC_SDA_HOLD   REG32(I2C1_BASE + 0x7cu)
#define I2C_IC_TX_ABRT    REG32(I2C1_BASE + 0x80u)
#define I2C_IC_FS_SPKLEN  REG32(I2C1_BASE + 0xa0u)

#define I2C_CON_MASTER    (1u << 0)
#define I2C_CON_SPEED_FAST (2u << 1)
#define I2C_CON_RESTART   (1u << 5)
#define I2C_CON_SLAVE_DISABLE (1u << 6)
#define I2C_CON_TX_EMPTY_CTRL (1u << 8)
#define I2C_DATA_CMD_READ (1u << 8)
#define I2C_DATA_CMD_STOP (1u << 9)
#define I2C_RAW_STOP_DET  (1u << 9)
#define I2C_RAW_TX_ABRT   (1u << 6)
#define I2C_RAW_TX_EMPTY  (1u << 4)
#define I2C_STATUS_RFNE   (1u << 3)
#define I2C_STATUS_TFNF   (1u << 1)

#define DMA_BASE          0x50000000u
#define DMA_CH0_READ_ADDR REG32(DMA_BASE + 0x00u)
#define DMA_CH0_WRITE_ADDR REG32(DMA_BASE + 0x04u)
#define DMA_CH0_TRANS_COUNT REG32(DMA_BASE + 0x08u)
#define DMA_CH0_CTRL_TRIG REG32(DMA_BASE + 0x0cu)
#define DMA_CTRL_EN       (1u << 0)
#define DMA_CTRL_SIZE_8   (0u << 2)
#define DMA_CTRL_INCR_READ (1u << 4)
#define DMA_CTRL_DREQ_SPI1_TX (18u << 15)
#define DMA_CTRL_BUSY     (1u << 24)

#define PPB_SYST_CSR      REG32(0xe000e010u)
#define PPB_SYST_RVR      REG32(0xe000e014u)
#define PPB_SYST_CVR      REG32(0xe000e018u)
#define PPB_NVIC_ISER     REG32(0xe000e100u)
#define PPB_NVIC_ISPR     REG32(0xe000e200u)
#define PPB_NVIC_ICPR     REG32(0xe000e280u)
#define SYST_CSR_ENABLE   (1u << 0)
#define SYST_CSR_TICKINT  (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)

static inline void reg_wait_cycles(unsigned int count) {
    while (count-- != 0u) {
        __asm__ volatile ("nop");
    }
}

static inline void reset_unreset(uint32_t bits) {
    RESETS_RESET |= bits;
    reg_wait_cycles(16u);
    RESETS_RESET &= ~bits;
    while ((RESETS_DONE & bits) != bits) {
    }
}

static inline void gpio_set_function(int pin, uint32_t function) {
    REG32(IO_BANK0_BASE + 0x04u + (uint32_t)pin * 8u) = function;
}

static inline void gpio_pad_default(int pin) {
    REG32(PADS_BANK0_BASE + 0x04u + (uint32_t)pin * 4u) = 0x56u;
}

static inline void gpio_pad_pull_up(int pin) {
    REG32(PADS_BANK0_BASE + 0x04u + (uint32_t)pin * 4u) = 0x5au;
}

static inline void gpio_out_enable(int pin) {
    SIO_GPIO_OE_SET = 1u << pin;
}

static inline void gpio_out_disable(int pin) {
    SIO_GPIO_OE_CLR = 1u << pin;
}

static inline void gpio_put(int pin, int value) {
    if (value) SIO_GPIO_OUT_SET = 1u << pin;
    else SIO_GPIO_OUT_CLR = 1u << pin;
}

#endif