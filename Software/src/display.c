/**
 * @file display.c
 * @brief Drives the two MAX7219 LED matrices (8 wide x 16 tall),
 *        including the Select block preview.
 */

#ifndef F_CPU
#define F_CPU 4000000UL
#endif
#include "fliptris_internal.h"
/* share header for:
HEIGHT, WIDTH, CELLS, SHAPE_CELLS, nextShape, rowPattern, register numbers  */

/* ===============================================================
 * BLOCK PREVIEW
 * =============================================================== */

/**
 * @brief Pivot row that centres a shape vertically on the display.
 * @param cells Shape cells (each block shape is 4 cells)
 * Cells store how far from block's pivot
 * @return Pivot row.
 */
static int8_t verticalCentre(const Cell cells[CELLS])
{
    int8_t highest = cells[0].down; /* initialise rows below pivot */
    int8_t lowest = cells[0].down;
    for (uint8_t i = 1; i < CELLS; i++) { /* go through all other 3 cells */
        if (cells[i].down < highest) /* keep smallest cell in highest */
            highest = cells[i].down;
        if (cells[i].down > lowest) /* keep largest cell in lowest */
            lowest = cells[i].down;
    }
    /* calculate pivot row that puts shape's mid at screen's mid */
    return (int8_t)((HEIGHT - (highest + lowest)) / 2);
}

/**
 * @brief Pivot column that centres a shape horizontally on the display.
 * @param cells Shape cells.
 * @return Pivot column.
 */
static int8_t horizontalCentre(const Cell cells[CELLS])
{
    int8_t leftMost = cells[0].right; /* initialise rows right of pivot */
    int8_t rightMost = cells[0].right;
    for (uint8_t i = 1; i < CELLS; i++) { /* go through all other 3 cells */
        if (cells[i].right < leftMost)
            leftMost = cells[i].right; /* keep smallest cell in leftmost */
        if (cells[i].right > rightMost)
            rightMost = cells[i].right; /* keep largest cell in rightmost */
    }
    /* calculate pivot row */
    return (int8_t)((WIDTH - (leftMost + rightMost)) / 2);
}

/**
 * @brief One row of the preview screen: the next block, centred.
 * @param row Display row.
 * @return Row bit pattern.
 */
static uint8_t previewRowPattern(uint8_t row)
{
    /* get 4 cells of next block shape from game.c */
    const Cell* cells = SHAPE_CELLS[nextShape];
    /* where to place pivot */
    int8_t pivotRow = verticalCentre(cells);
    int8_t pivotCol = horizontalCentre(cells);
    uint8_t pattern = 0; /* start with LED off */
    for (uint8_t i = 0; i < CELLS; i++) { /* for each of 4 cells, find row */
        if (pivotRow + cells[i].down != (int8_t)row)
            continue;
        int8_t col = pivotCol + cells[i].right; /* work out column */
        if (col >= 0 && col < WIDTH) /* if column in on screen */
            pattern |= (uint8_t)(1 << col); /* set that bit to pattern */
    }
    return pattern;
}

/**
 * @brief What to show on a row: the preview while Select is held,
 *        otherwise the playfield.
 * @param row Display row.
 * @return Row bit pattern.
 */
static uint8_t visibleRow(uint8_t row)
{
    if (previewing) /* true while Select held (set in app.c) */
        return previewRowPattern(row);
    return rowPattern(row); /* from game.c with falling board */
}

/* ===============================================================
 * MAX7219 DRIVER
 * =============================================================== */

/**
 * @brief Set the LOAD, CLK and DATA pins as outputs.
 */
static void spiInit(void)
{
    /* make 3 wires to MAX7219s outputs */
    PORTA.DIRSET = LOAD_PIN | CLK_PIN | DATA_PIN;
    /* starting point */
    PORTA.OUTSET = LOAD_PIN; /* idle not receiving */
    PORTA.OUTCLR = CLK_PIN | DATA_PIN; /* CLK and DATA low */
}

/**
 * @brief Shift one byte out, most significant bit first.
 * @param value Byte to send.
 */
static void shiftByte(uint8_t value)
{
    /* one bit at a time, leftmost bit first */
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (value & 0x80) /* if 1 */
            PORTA.OUTSET = DATA_PIN; /* data set high */
        else
            PORTA.OUTCLR = DATA_PIN; /* data set low */

        /* pulse clock high (send data) then low */
        PORTA.OUTSET = CLK_PIN;
        PORTA.OUTCLR = CLK_PIN;
        value <<= 1; /* shift next bit into 7 */
    }
}

/**
 * @brief Write the same register value to both drivers.
 * @param reg MAX7219 register.
 * @param value Value to write.
 */
static void writeBoth(uint8_t reg, uint8_t value)
{
    /* load low- message is starting */
    PORTA.OUTCLR = LOAD_PIN;
    /* send 2 byte message twice to both drivers */
    for (uint8_t i = 0; i < DRIVERS; i++) {
        shiftByte(reg);
        shiftByte(value);
    }

    /* load high- chips act on recieved */
    PORTA.OUTSET = LOAD_PIN;
}

/**
 * @brief Write one row to each driver. The bottom value is sent first
 *        because it travels further along the daisy chain.
 * @param reg Digit register (row),each driver had 1 digit registers per row x8
 * @param topValue Pattern for the top module.
 * @param bottomValue Pattern for the bottom module.
 */
static void writeRow(uint8_t reg, uint8_t topValue, uint8_t bottomValue)
{
    PORTA.OUTCLR = LOAD_PIN;
    /* first message pushed to chip 2 (bottom module) */
    shiftByte(reg);
    shiftByte(bottomValue);
    /* second message pushed to chip 1 (top module)*/
    shiftByte(reg);
    shiftByte(topValue);
    PORTA.OUTSET = LOAD_PIN;
}

/**
 * @brief Set up both MAX7219s and clear the display.
 */
void displayInit(void)
{
    spiInit(); /* set up 3 wires */
    _delay_ms(10);
    writeBoth(REG_TEST, 0x00); /* display test off */
    writeBoth(REG_DECODE, 0x00); /* no decoding */
    writeBoth(REG_SCANLIMIT, 0x07); /* use all 8 rows */
    writeBoth(REG_INTENSITY, BRIGHTNESS); /* brightness = 0x04 */
    writeBoth(REG_SHUTDOWN, 0x01); /* wake up */
    /* clear all 8 rows on both, so every LED starts off */
    for (uint8_t row = 0; row < DRIVER_ROWS; row++) {
        writeBoth(REG_DIGIT0 + row, 0x00);
    }
}

/**
 * @brief Reorder a row's bits to match how the matrix columns are wired.
 *
 * The display shows C1 C2 C3 C4 as C2 C3 C4 C1, so this applies the
 * inverse (C4 C1 C2 C3), then rotates the whole row right by one.
 *
 * @param pattern Row in game order (bit 0 = left column).
 * @return Row in wiring order.
 */
static uint8_t alignRow(uint8_t pattern)
{
    uint8_t corrected = (pattern & 0xF0) | /* keep C5-C8 */
        ((pattern << 1) & 0x0E) | /* C1->C2, C2->C3, C3->C4 */
        ((pattern >> 3) & 0x01); /* C4->C1 */

    return (uint8_t)((corrected >> 1) | ((corrected & 0x01) << 7));
}

/* ===============================================================
 * DRAWING
 * =============================================================== */

/**
 * @brief Draw the playfield (or the preview) on both modules.
 */
void displayShow(void)
{
    /* repeat for each row register */
    for (uint8_t row = 0; row < DRIVER_ROWS; row++) {
        /* top module register : 7-row from game row go in*/
        uint8_t topRow = (uint8_t)(DRIVER_ROWS - 1 - row);
        /* bottom module register: 15-row from game row go in */
        uint8_t bottomRow = (uint8_t)(HEIGHT - 1 - row);
        /* before sending */
        writeRow(REG_DIGIT0 + row,
            alignRow(visibleRow(topRow)), /* get what should be shown and fix
                                           * bit order */
            alignRow(visibleRow(bottomRow)));
    }
}

/**
 * @brief Draw any 16-row picture (used for screen.c scrolling text, controller
 * test screen).
 * @param buffer One byte per row, bit 0 = left column.
 */
void displayShowBuffer(const uint8_t buffer[HEIGHT])
{
    /* 16 byte array passed in for rows */
    for (uint8_t row = 0; row < DRIVER_ROWS; row++) {
        uint8_t topRow = (uint8_t)(DRIVER_ROWS - 1 - row);
        uint8_t bottomRow = (uint8_t)(HEIGHT - 1 - row);
        writeRow(REG_DIGIT0 + row,
            alignRow(buffer[topRow]),
            alignRow(buffer[bottomRow]));
    }
}

/**
 * @brief Turn every LED off.
 */
void clearPhysicalDisplay(void)
{
    uint8_t blank[HEIGHT] = { 0 };
    displayShowBuffer(blank);
}
