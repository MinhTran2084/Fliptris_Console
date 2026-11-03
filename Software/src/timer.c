/**
 * @file timer.c
 * @brief 1 ms system timer (TCB0).
 *
 * Gives accurate time for the drop rate however long the main loop
 * takes, and steps the buzzer note queue every millisecond.
 */

#ifndef F_CPU
#define F_CPU 4000000UL
#endif
#include "fliptris_internal.h"
/* share header for:
timer register names, ATOMIC_BLOCK, buzzer_tick() */

/* how many milisec since power-up */
static volatile uint32_t msCount = 0;

/**
 * @brief TCB0 interrupt, every 1 ms. Counts time and steps the buzzer.
 */
ISR(TCB0_INT_vect)
{
    TCB0.INTFLAGS = TCB_CAPT_bm; /* clear interrupt flag */
    msCount++;
    buzzer_tick(); /* play queued tones in the background */
}

/**
 * @brief Start TCB0 as a 1 ms periodic interrupt.
 *
 * Global interrupts are enabled later by uart_init().
 */
void timerInit(void)
{
    TCB0.CCMP = (uint16_t)(F_CPU / 1000UL - 1); /* 1 ms period */
    TCB0.CTRLB = TCB_CNTMODE_INT_gc; /* periodic interrupt */
    TCB0.INTCTRL = TCB_CAPT_bm; /* fire the interrupt */
    /* starts the timer, count at 4MHz, switch on */
    TCB0.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;
}

/**
 * @brief Milliseconds since power-up.
 * @return Current millisecond count (read atomically).
 * Used by app.c to time block drops and 10ms main loop
 */
uint32_t millis(void)
{
    uint32_t m;
    /* copy msCount with interrupts paused */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        m = msCount;
    }
    return m;
}
