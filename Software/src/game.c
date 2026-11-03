/**
 * @file game.c
 * @brief Game logic: playfield, blocks, movement, rotation, line
 *        clearing, scoring and the 7-bag randomiser.
 */

#ifndef F_CPU
#define F_CPU 4000000UL
#endif
#include "fliptris_internal.h"

/* ===============================================================
 * SHAPES AND GAME STATE
 * =============================================================== */

/* Cells of each shape as offsets from its pivot, in order I J L O S T Z */
/* down: rows below the pivot */
/* right: columns right of the pivot */
const Cell SHAPE_CELLS[SHAPES][CELLS] = {
    /* I */ { { 0, -1 }, { 0, 0 }, { 0, 1 }, { 0, 2 } },
    /* J */ { { -1, -1 }, { 0, -1 }, { 0, 0 }, { 0, 1 } },
    /* L */ { { -1, 1 }, { 0, -1 }, { 0, 0 }, { 0, 1 } },
    /* O */ { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } },
    /* S */ { { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 0 } },
    /* T */ { { -1, 0 }, { 0, -1 }, { 0, 0 }, { 0, 1 } },
    /* Z */ { { -1, -1 }, { -1, 0 }, { 0, 0 }, { 0, 1 } }
};

uint8_t board[HEIGHT]; /* fixed cells, one byte per row, bit = column */
static int8_t blockRow; /* falling block pivot row */
static int8_t blockCol; /* falling block pivot column */
static uint8_t blockShape; /* what shape is falling */
static Cell blockCells[CELLS]; /* falling block's 4 cell offsets */

uint8_t nextShape; /* shown by the Select preview */
uint16_t score;

static uint8_t shapeBag[SHAPES]; /* shapes left in the current bag */
static uint8_t bagRemaining = 0; /* no. of shapes left in the bag */
static uint16_t rngState = 0xACE1u; /* random number generator's current state*/
static uint8_t testShape = 0; /* next shape in test mode (alphabetical) */

bool finished;
bool previewing; /* select held? */

/* used by controller.c */
uint16_t lastButtons;
uint8_t holdTicks; /* how long left/right held */

/* ===============================================================
 * PLAYFIELD
 * =============================================================== */

/**
 * @brief Check whether a cell is blocked.
 *
 * Walls and the floor count as filled. Space above the screen counts
 * as empty, so blocks can rotate as soon as they spawn.
 *
 * @param row Playfield row.
 * @param col Playfield column.
 * @return true if the cell is a wall, the floor or a fixed cell.
 */
static bool isFilled(int8_t row, int8_t col)
{
    /* if position is wall or floor, count as filled */
    if (col < 0 || col >= WIDTH || row >= HEIGHT)
        return true;
    if (row < 0) /* if ceiling, counts as empty */
        return false; /* above the screen counts as empty */
    return board[row] & (1 << col); /* true if fixed cell there */
}

/**
 * @brief Remove every full row and shift the rows above down.
 * @return Number of rows removed.
 */
static uint8_t clearRows(void)
{
    uint8_t cleared = 0;
    int8_t row = HEIGHT - 1; /* start checking from bottom row- row 15 */
    while (row >= 0) { /* keep checking until row 0 */
        if (board[row] != 0xFF) { /* if not full row */
            row--; /* move up one row */
            continue;
        }
        cleared++;
        /* copy every row above it down one place to clear */
        for (int8_t above = row; above > 0; above--) {
            board[above] = board[above - 1];
        }
        board[0] = 0; /* top row as nothing to copy- empty */
        /* row is not moved on, so the row that shifted down is checked too */
    }
    return cleared;
}

/**
 * @brief Check whether a block fits at a position.
 * @param row Pivot row.
 * @param col Pivot column.
 * @param cells Block cells.
 * @return true if no cell is blocked.
 */
static bool fits(int8_t row, int8_t col, const Cell cells[CELLS])
{
    for (uint8_t i = 0; i < CELLS; i++) {
        if (isFilled(row + cells[i].down, col + cells[i].right))
            return false;
    }
    return true;
}

/* ===============================================================
 * 7-BAG RANDOMISER
 * =============================================================== */

/**
 * @brief Step the 16-bit maximal-length LFSR.
 * @return Next value (1 to 65535, never 0).
 */
static uint16_t randomNext(void)
{
    /* saves rightmost bit of current state */
    uint16_t bit = rngState & 1U;

    rngState >>= 1;

    if (bit) { /* if bit was 1, flip certain bits using XOR with fixeed
                * pattern 0x8400 */
        rngState ^= 0xB400U;
    }

    return rngState;
}

/**
 * @brief Uniform random number, using rejection to avoid modulo bias.
 * Makes it extra random
 * @param limit Upper bound (exclusive).
 * @return Number from 0 to limit - 1.
 */
static uint8_t randomBelow(uint8_t limit)
{
    if (limit <= 1) { /* only one choice, answer is always 0 */
        return 0;
    }

    /* randomNext() - 1 gives 0..65534; reject the uneven top end */
    /* work out how many no. to keep to make sure range divides evenly */
    uint16_t usable = (uint16_t)(65535UL - (65535UL % limit)); /* subtract leftover */

    /* value is 0-656534, if value is too big throw away */
    uint16_t value;
    do {
        value = (uint16_t)(randomNext() - 1U);
    } while (value >= usable);

    /* value is now a range that divides evenly by limit */
    /*  % limit gives 0 to limit-1, all no. are exactly equally likely */
    return (uint8_t)(value % limit);
}

/**
 * @brief Fill the bag with one of each shape.
 */
static void refillBag(void)
{
    for (uint8_t i = 0; i < SHAPES; i++) {
        shapeBag[i] = i;
    }

    bagRemaining = SHAPES; /* set no. of shapes in bag to 7 */
}

/**
 * @brief Take a random shape out of the bag, refilling it when empty.
 *
 * A shape cannot appear again until the bag has been emptied.
 *
 * @return Shape index.
 */
static uint8_t drawFromBag(void)
{
    if (bagRemaining == 0) {
        refillBag();
    }

    uint8_t index = randomBelow(bagRemaining);
    uint8_t selectedShape = shapeBag[index];

    /* remove it by moving the last remaining shape into its slot */
    shapeBag[index] = shapeBag[bagRemaining - 1];
    bagRemaining--;

    return selectedShape;
}

/**
 * @brief Choose the next block: 7-bag normally, I J L O S T Z in
 *        test mode.
 * @return Shape index.
 */
static uint8_t getNextShape(void)
{
    /* if in test mode, hand in order */
    if (config.test_mode) {
        uint8_t shape = testShape;

        testShape++;

        if (testShape >= SHAPES) {
            testShape = 0;
        }

        return shape;
    }

    return drawFromBag(); /* otherwise use bag randomiser */
}

/**
 * @brief Seed the randomiser from accelerometer noise, so each power-up
 *        gives a different sequence. Call after the LIS3DH is set up.
 */
void seedBlockRandomiser(void)
{
    /* use non-zero starting value */
    uint16_t seed = 0xACE1U;

    /* take 16 readings from accelerometer */
    for (uint8_t i = 0; i < 16; i++) {
        int16_t accelerometerNoise = read_accelerometer_x();

        /* mix each reading into seed using XOR */
        seed ^= (uint16_t)accelerometerNoise;

        /* rotate so each sample affects different bits */
        seed = (uint16_t)((seed << 5) | (seed >> 11));

        /* mix in another random number */
        seed ^= (uint16_t)(i * 0x9E37U);

        _delay_ms(1);
    }

    /* the LFSR must never be zero */
    if (seed == 0) {
        seed = 0xACE1U;
    }

    /* store as rngState's starting state */
    rngState = seed;
}

/* ===============================================================
 * BLOCK MOVEMENT
 * =============================================================== */

/**
 * @brief Make the next shape the falling block and choose a new next
 *        shape. Ends the game if the block can't fit at the top.
 */
static void newBlock(void)
{
    blockShape = nextShape;

    /*
     * I and O have no cells above their pivot and sit one column right
     * of it, so spawn them one row up and one column left. Every block
     * then starts in row 0, centred.
     */
    bool evenWidth = (blockShape == 0 || blockShape == SHAPE_O);
    blockRow = evenWidth ? 0 : START_ROW;
    blockCol = evenWidth ? START_COL - 1 : START_COL;

    /* copy the shape's 4 offsets from table into blockCells */
    for (uint8_t i = 0; i < CELLS; i++) {
        blockCells[i] = SHAPE_CELLS[blockShape][i];
    }

    nextShape = getNextShape(); /* choose next shape straight away,
                                 * so preview can show */

    /* can the new block sit here w/o overlapping anything? */
    if (!fits(blockRow, blockCol, blockCells)) {
        finished = true; /* game over */
    }
}

/**
 * @brief Copy the falling block's cells into the board.
 */
static void landBlock(void)
{
    /* for each of the 4 cells */
    for (uint8_t i = 0; i < CELLS; i++) {
        /* work out pivot + offset */
        int8_t row = blockRow + blockCells[i].down;
        int8_t col = blockCol + blockCells[i].right;

        /* if on screen, turn bit on, is part of the board*/
        if (row >= 0 && row < HEIGHT && col >= 0 && col < WIDTH) {
            board[row] |= (1 << col);
            /
        }
    }
}

/**
 * @brief Move the falling block sideways if there is room.
 * @param step -1 for left, +1 for right.
 */
void moveBlock(int8_t step)
{
    if (fits(blockRow, blockCol + step, blockCells))
        blockCol += step;
}

/**
 * @brief Rotate the falling block 90 degrees.
 *
 * If it doesn't fit, try shifting it 1 then 2 columns left or right
 * (smallest shift first). The O block never rotates.
 *
 * @param clockwise true for clockwise, false for counter-clockwise.
 */
void turnBlock(bool clockwise)
{
    /* allocate space for rotate shapes */
    Cell turned[CELLS];
    if (blockShape == SHAPE_O)
        return;
    for (uint8_t i = 0; i < CELLS; i++) {
        if (clockwise) {
            turned[i].down = blockCells[i].right;
            turned[i].right = -blockCells[i].down;
        } else {
            turned[i].down = -blockCells[i].right;
            turned[i].right = blockCells[i].down;
        }
    }
    /* if block doesn't fit, shift left and right, by 0,1,2 right/left */
    for (int8_t shift = 0; shift <= 2; shift++) {
        for (int8_t side = -1; side <= 1; side += 2) {
            int8_t tryCol = blockCol + shift * side; /* column change */
            if (fits(blockRow, tryCol, turned)) { /* at first position that fits, keep */
                blockCol = tryCol;
                for (uint8_t i = 0; i < CELLS; i++)
                    blockCells[i] = turned[i];
                return;
            }
            if (shift == 0) /* no need to try shift 0 twice */
                break;
        }
    }
}

/**
 * @brief Move the block down one row, or land it if it can't move.
 *
 * Landing fixes the block, clears rows, scores 2^(N-1), compacts if
 * tilted, plays the tones and spawns the next block.
 *
 * Used by app.c every time drop timer runs out
 *
 * @return true if the block landed, false if it just moved down.
 */
bool dropBlock(void)
{
    /* if block fits one row lower */
    if (fits(blockRow + 1, blockCol, blockCells)) {
        blockRow++; /* move down */
        return false; /* still falling */
    }

    /* 1. fix the block in place and queue landing beep */
    landBlock();
    play_block_fixed();

    /* 2. clear rows and score */
    uint8_t rowsCleared = clearRows(); /* remove full rows */
    if (rowsCleared > 0) {
        /* 2^(N-1) score clear */
        score += (uint16_t)1 << (rowsCleared - 1);
        play_lines_cleared(rowsCleared); /* queue beep per cleared row */
    }

    /* 3. compaction, with its tone only if the board changed */
    uint8_t board_copy[HEIGHT];
    memcpy(board_copy, board, HEIGHT); /* copy board as is */

    apply_compaction();

    /* if compaction changed board, play sound */
    if (memcmp(board, board_copy, HEIGHT) != 0) {
        play_compaction();
    }

    /* 4. spawn the next block */
    newBlock();

    return true; /* landed */
}

/**
 * @brief Clear the board, reset score and randomiser, spawn the first block.
 */
void startGame(void)
{
    for (uint8_t row = 0; row < HEIGHT; row++) {
        board[row] = 0;
    }

    score = 0;
    finished = false;
    previewing = false;

    bagRemaining = 0; /* fresh bag */
    testShape = 0; /* test mode starts at I */

    /* choose the first block; newBlock() then chooses the next one */
    nextShape = getNextShape();

    newBlock();
}

/**
 * @brief One row of the playfield, with the falling block drawn on top.
 *
 * @param row Playfield row.
 * @return Row bit pattern.
 */
uint8_t rowPattern(uint8_t row)
{
    /* start with fixed cells in row */
    uint8_t pattern = board[row];
    if (finished) /*if game over */
        return pattern; /* don't draw falling block */

    /* add falling block on top */
    for (uint8_t i = 0; i < CELLS; i++) {
        if (blockRow + blockCells[i].down != (int8_t)row) /* work out its row */
            continue;
        int8_t col = blockCol + blockCells[i].right;

        /* if the cell is n screen, turn bit on */
        if (col >= 0 && col < WIDTH)
            pattern |= (1 << col);
    }
    /* return finished row- fixed cells + falling block */
    return pattern;
}
