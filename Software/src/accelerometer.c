/**
 * @file accelerometer.c
 * @brief Talks to accelerometer over I2C and reads tilt to compact board
 */

#ifndef F_CPU
#define F_CPU 4000000UL /* 4MHz processor speed */
#endif
#include "fliptris_internal.h"
/* share header for:
board[], HEIGHT, WIDTH, LIS3DH_ADDR, CAL_SAMPLES, TILT_THRESHOLD_45_DEG */

static int16_t offset_x = 0; /* resting X reading, measured at power-up */

/* ===============================================================
 * I2C (TWI0)
 * =============================================================== */

/**
 * @brief Enable TWI0 as I2C host, with pull-ups on SDA/SCL (PA2/PA3).
 */
void i2c_init(void)
{
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm; /* pull up resistor for SDA */
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm; /* pull up resistor for SCL */
    TWI0.MBAUD = 15; /* set bus speed to around 100kHz */
    TWI0.MCTRLA = TWI_ENABLE_bm; /* enable chip I2C hardware as host */
    TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc; /* bus idle = I2C can start sending */
}

/**
 * @brief Write one byte to a device register.
 * @param dev_addr 7-bit I2C device address.
 * @param reg_addr Register to write.
 * @param data Value to write.
 * @return true if every byte was acknowledged.
 */
bool i2c_write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    TWI0.MADDR = (dev_addr << 1) | 0; /* shift to make room for r/w and send
    addr */
    while (!(TWI0.MSTATUS & TWI_WIF_bm)) /* write interrupt flag */
        ;
    if (TWI0.MSTATUS & TWI_RXACK_bm) /* NACK- no */
        return false;
    TWI0.MDATA = reg_addr; /* check register addr accepted */
    while (!(TWI0.MSTATUS & TWI_WIF_bm))
        ;
    if (TWI0.MSTATUS & TWI_RXACK_bm)
        return false;
    TWI0.MDATA = data; /* check data addr accepted */
    while (!(TWI0.MSTATUS & TWI_WIF_bm))
        ;
    if (TWI0.MSTATUS & TWI_RXACK_bm)
        return false;
    TWI0.MCTRLB = TWI_MCMD_STOP_gc; /* end transfer and free bus */
    return true; /* ACK- all three bytes accepted */
}

/**
 * @brief Read one or more consecutive device registers.
 * @param dev_addr 7-bit I2C device address.
 * @param reg_addr First register to read.
 * @param data Buffer for the bytes read.
 * @param length Number of bytes to read.
 * @return true if the device acknowledged.
 */
bool i2c_read_registers(uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint8_t length)
{
    TWI0.MADDR = (dev_addr << 1) | 0; /* send addr in write, START hardware */
    while (!(TWI0.MSTATUS & TWI_WIF_bm)) /* wait until byte is out */
        ;
    if (TWI0.MSTATUS & TWI_RXACK_bm) /* no answer at addr, give up */
        return false;
    TWI0.MDATA = reg_addr;
    while (!(TWI0.MSTATUS & TWI_WIF_bm))
        ;
    if (TWI0.MSTATUS & TWI_RXACK_bm)
        return false;
    TWI0.MADDR = (dev_addr << 1) | 1; /* send addr again but with read */
    while (!(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm))) /* while wait
    check if read (byte arrived) or write (something wrong) request acknow. */
        ;
    if (TWI0.MSTATUS & TWI_RXACK_bm)
        return false;
    for (uint8_t i = 0; i < length; i++) { /* loop once for each byte */
        while (!(TWI0.MSTATUS & TWI_RIF_bm)) /* wait until bye arrives */
            ;
        data[i] = TWI0.MDATA; /* store byte */
        if (i == (length - 1)) { /* last byte, stop (NACK) */
            TWI0.MCTRLB = TWI_ACKACT_NACK_gc | TWI_MCMD_STOP_gc;
        } else {
            TWI0.MCTRLB = TWI_ACKACT_ACK_gc | TWI_MCMD_RECVTRANS_gc;
        }
    }
    return true; /* success */
}

/* ===============================================================
 * ACCELEROMETER
 * =============================================================== */

/**
 * @brief Average the X axis at rest, so the resting reading becomes zero.
 *
 * Must be called with the device upright and still.
 */
void calibrate_accelerometer(void)
{
    uint8_t raw_data[2];
    int32_t sum_x = 0;
    for (uint8_t i = 0; i < CAL_SAMPLES; i++) { /* takes 32 samples */
        /* if success read 2 bytes (0x28 and 0x80) at register 0xA8
        (X reading) */
        if (i2c_read_registers(LIS3DH_ADDR, 0xA8, raw_data, 2)) {
            /* glue 2 bytes into one no. */
            sum_x += (int16_t)((raw_data[1] << 8) | raw_data[0]);
        }
        _delay_ms(20); /* take new reading every 20ms */
    }
    /* upright value saved for later */
    offset_x = (int16_t)(sum_x / CAL_SAMPLES);
}

/**
 * @brief Read the X axis with the resting offset removed.
 * @return Calibrated X reading, or 0 if the read failed.
 */
int16_t read_accelerometer_x(void)
{
    uint8_t raw_data[2];
    /* if success read 2 bytes at register 0xA8 (X value) */
    if (i2c_read_registers(LIS3DH_ADDR, 0xA8, raw_data, 2)) {
        int16_t raw_x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
        /* subtract the upright value
           upright = about 0
           tilted one way = negative
           tilted other way = positive */
        return raw_x - offset_x;
    }
    return 0; /* if read failed, pretend it's upright */
}

/* ===============================================================
 * COMPACTION
 * =============================================================== */

/**
 * @brief Count the filled cells in one row. Each row of the board
 * is stored as 8 on/off switches (one per column)
 * @param b Row bit pattern.
 * @return Number of set bits.
 */
static uint8_t count_row_bits(uint8_t b)
{
    uint8_t count = 0;
    while (b) { /* while any switches are on */
        count += (b & 1); /* look at rightmost switch, and if it's on add 1 */
        b >>= 1; /* slide everything one right */
    }
    return count; /* o. of switches on */
}

/**
 * @brief If the device is tilted more than 45 degrees, slide every
 *        row's cells as far left or right as they will go.
 *
 *        Runs every time a block lands
 */
void apply_compaction(void)
{
    int16_t cal_x = read_accelerometer_x();
    if (cal_x <= -TILT_THRESHOLD_45_DEG) { /* if tilted 45 degrees one way
        compact left */
        for (uint8_t row = 0; row < HEIGHT; row++) {
            uint8_t count = count_row_bits(board[row]);
            if (count == 8)
                board[row] = 0xFF; /* if the row is full, leave full */
            else if (count > 0) /* otherwise */
                board[row] = (uint8_t)((1 << count) - 1); /* replace row with
                same no. of blocks pushed left */
        }
    } else if (cal_x >= TILT_THRESHOLD_45_DEG) { /* if tilted 45 degrees other
        way */
        /* compact right */
        for (uint8_t row = 0; row < HEIGHT; row++) {
            uint8_t count = count_row_bits(board[row]);
            if (count == 8)
                board[row] = 0xFF;
            else if (count > 0)
                board[row] = (uint8_t)(0xFF << (WIDTH - count)); /* replace row
                with same no. of blocks pushed right */
        }
    }
}
