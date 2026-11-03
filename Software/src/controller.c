#ifndef F_CPU
#define F_CPU 4000000UL
#endif
#include "fliptris_internal.h"

/* ===============================================================
 * CONTROLLER (SNES)
 * =============================================================== */

void controllerInit(void)
{
    PORTC.DIRSET = CLOCK_PIN | LATCH_PIN;
    PORTC.DIRCLR = BUTTON_DATA_PIN;
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm;
    PORTC.OUTSET = CLOCK_PIN;
    PORTC.OUTCLR = LATCH_PIN;
    lastButtons = 0;
    holdTicks = 0;
}

static void latchButtons(void)
{
    PORTC.OUTSET = LATCH_PIN;
    _delay_us(LATCH_HOLD_US);
    PORTC.OUTCLR = LATCH_PIN;
    _delay_us(HALF_CLOCK_US);
}

static bool readDataLine(void)
{
    return !(PORTC.IN & BUTTON_DATA_PIN);
}

uint16_t readButtons(void)
{
    uint16_t buttons = 0;
    latchButtons();
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        PORTC.OUTCLR = CLOCK_PIN;
        _delay_us(HALF_CLOCK_US);
        if (readDataLine())
            buttons |= (1U << i);
        PORTC.OUTSET = CLOCK_PIN;
        _delay_us(HALF_CLOCK_US);
    }
    return buttons;
}

/*
 * Debounced read for gameplay. A button only counts as
 * pressed/released once two readings in a row (10 ms apart)
 * agree, so contact bounce or a one-off glitch can't look
 * like a release followed by a new press.
 */
static uint16_t lastRaw;

uint16_t readButtonsDebounced(void)
{
    uint16_t raw = readButtons();
    uint16_t stable = (raw & lastRaw) | (lastButtons & (raw | lastRaw));
    lastRaw = raw;
    return stable;
}

bool isHeld(uint16_t buttons, uint8_t button)
{
    return (buttons & (uint16_t)(1 << button)) != 0;
}

bool wasJustPressed(uint16_t buttons, uint8_t button)
{
    return isHeld(buttons, button) && !isHeld(lastButtons, button);
}

void handleRotation(uint16_t buttons)
{
    if (wasJustPressed(buttons, BUTTON_A))
        turnBlock(true);
    if (wasJustPressed(buttons, BUTTON_B))
        turnBlock(false);
}

void handleSideways(uint16_t buttons)
{
    bool left = isHeld(buttons, BUTTON_LEFT);
    bool right = isHeld(buttons, BUTTON_RIGHT);
    if (!left && !right) {
        holdTicks = 0;
        return;
    }
    bool newPress = wasJustPressed(buttons, BUTTON_LEFT) || wasJustPressed(buttons, BUTTON_RIGHT);
    if (newPress) {
        holdTicks = 0;
    } else {
        holdTicks++;
        if (holdTicks < REPEAT_DELAY_TICKS)
            return;
        holdTicks = REPEAT_DELAY_TICKS - REPEAT_TICKS; /* next repeat in REPEAT_TICKS */
    }
    if (left)
        moveBlock(-1);
    else
        moveBlock(1);
}

/*
 * Returns the current drop interval in MICROSECONDS.
 */
uint32_t dropInterval(uint16_t buttons)
{
    uint32_t intervalUs;

    /*
     * DOWN always forces fast_interval.
     */
    if (isHeld(buttons, BUTTON_DOWN)) {
        intervalUs = (uint32_t)config.fast_interval * 1000UL;
    }

    /*
     * Test mode:
     * L forces slow_interval.
     */
    else if (
        config.test_mode && isHeld(buttons, BUTTON_L)) {
        intervalUs = (uint32_t)config.slow_interval * 1000UL;
    }

    /*
     * Normal score-dependent rate.
     */
    else {
        uint32_t slowUs = (uint32_t)config.slow_interval * 1000UL;

        uint32_t fastUs = (uint32_t)config.fast_interval * 1000UL;

        uint32_t maxReduction = slowUs - fastUs;

        /*
         * If score_step is zero, speed does not
         * increase with score.
         */
        if (config.score_step == 0 || score == 0) {
            intervalUs = slowUs;
        }

        /*
         * Check whether the score has already
         * reached the maximum allowed reduction.
         *
         * This avoids multiplying score by
         * score_step when it could overflow.
         */
        else if (
            (uint32_t)score >= (maxReduction + config.score_step - 1UL) / config.score_step) {
            intervalUs = fastUs;
        }

        /*
         * Safe multiplication:
         * reduction is guaranteed to be smaller
         * than maxReduction.
         */
        else {
            uint32_t reduction = (uint32_t)score * config.score_step;

            intervalUs = slowUs - reduction;
        }
    }

    return intervalUs;
}
