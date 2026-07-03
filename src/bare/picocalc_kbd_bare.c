#include "picocalc_kbd_bare.h"
#include "rp2040_regs.h"

#define KBD_SDA 6
#define KBD_SCL 7
#define KBD_ADDR 0x1fu
#define KBD_REG_KEY 0x09u

static void kbd_delay_ms(unsigned int ms) {
    while (ms-- != 0u) {
        reg_wait_cycles(60000u);
    }
}

static int i2c_wait(unsigned int mask, unsigned int value) {
    unsigned int timeout = 2000000u;
    while ((I2C_IC_STATUS & mask) != value) {
        if ((I2C_IC_RAW_INTR & I2C_RAW_TX_ABRT) != 0u) {
            (void)I2C_IC_TX_ABRT;
            (void)I2C_IC_CLR_TX_ABRT;
            return -1;
        }
        if (timeout-- == 0u) return -1;
    }
    return 0;
}

static int i2c_wait_stop(void) {
    unsigned int timeout = 2000000u;
    while ((I2C_IC_RAW_INTR & I2C_RAW_STOP_DET) == 0u) {
        if ((I2C_IC_RAW_INTR & I2C_RAW_TX_ABRT) != 0u) {
            (void)I2C_IC_TX_ABRT;
            (void)I2C_IC_CLR_TX_ABRT;
            return -1;
        }
        if (timeout-- == 0u) return -1;
    }
    (void)I2C_IC_CLR_STOP;
    return 0;
}

static int i2c_write_byte(uint8_t address, uint8_t byte) {
    I2C_IC_ENABLE = 0u;
    I2C_IC_TAR = address;
    I2C_IC_ENABLE = 1u;

    if (i2c_wait(I2C_STATUS_TFNF, I2C_STATUS_TFNF) != 0) return -1;
    I2C_IC_DATA_CMD = (uint32_t)byte | I2C_DATA_CMD_STOP;

    while ((I2C_IC_RAW_INTR & I2C_RAW_TX_EMPTY) == 0u) {
        if ((I2C_IC_RAW_INTR & I2C_RAW_TX_ABRT) != 0u) {
            (void)I2C_IC_TX_ABRT;
            (void)I2C_IC_CLR_TX_ABRT;
            return -1;
        }
    }
    return i2c_wait_stop();
}

static int i2c_read(uint8_t address, uint8_t *dst, int count) {
    int i;
    I2C_IC_ENABLE = 0u;
    I2C_IC_TAR = address;
    I2C_IC_ENABLE = 1u;

    for (i = 0; i < count; ++i) {
        uint32_t command = I2C_DATA_CMD_READ;
        if (i + 1 == count) command |= I2C_DATA_CMD_STOP;
        if (i2c_wait(I2C_STATUS_TFNF, I2C_STATUS_TFNF) != 0) return -1;
        I2C_IC_DATA_CMD = command;
        if (i2c_wait(I2C_STATUS_RFNE, I2C_STATUS_RFNE) != 0) return -1;
        dst[i] = (uint8_t)I2C_IC_DATA_CMD;
    }
    return i2c_wait_stop();
}

void picocalc_kbd_init(void) {
    reset_unreset(RESET_I2C1);

    gpio_pad_pull_up(KBD_SDA);
    gpio_pad_pull_up(KBD_SCL);
    gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);

    I2C_IC_ENABLE = 0u;
    I2C_IC_CON = I2C_CON_MASTER | I2C_CON_SPEED_FAST | I2C_CON_RESTART | I2C_CON_SLAVE_DISABLE | I2C_CON_TX_EMPTY_CTRL;
    I2C_IC_FS_HCNT = 5320u;
    I2C_IC_FS_LCNT = 7980u;
    I2C_IC_FS_SPKLEN = 498u;
    I2C_IC_SDA_HOLD = 40u;
    I2C_IC_ENABLE = 1u;
}

int picocalc_kbd_read_key(void) {
    uint8_t bytes[2];
    uint32_t raw;

    if (i2c_write_byte(KBD_ADDR, KBD_REG_KEY) != 0) return -1;
    kbd_delay_ms(16);
    if (i2c_read(KBD_ADDR, bytes, 2) != 0) return -1;

    raw = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8);
    if ((raw & 0xffu) == 1u) {
        return (int)(raw >> 8);
    }
    return -1;
}