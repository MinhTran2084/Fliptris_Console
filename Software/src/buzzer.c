/**
 * @file buzzer.c
 * @brief Non-blocking buzzer on PD4.
 *
 * Notes are queued and played in the background. TCB1 interrupts at
 * twice the note frequency and toggles the pin, giving a square wave.
 * buzzer_tick() (called every 1 ms from timer.c) moves to the next note.
 */

#ifndef F_CPU
#define F_CPU 4000000UL
#endif

#include "buzzer.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#define BUZZER_PIN PIN4_bm /* pin is on PD4 */
#define NOTE_MS 100 /* length of each note ms */
#define GAP_MS 100 /* silence between line-clear beeps */
#define QUEUE_SIZE 16 /* to do list holds up to 16 notes */

/* ===============================================================
 * NOTE QUEUE
 * =============================================================== */

/** @brief One queued note: a frequency (0 = silence) and a length in ms. */
typedef struct {
    uint16_t freq; /* pitch in Hz */
    uint16_t ms; /* how long to play */
} Note;

static volatile Note queue[QUEUE_SIZE]; /* to do list of notes */
static volatile uint8_t head = 0; /* next free slot for note */
static volatile uint8_t tail = 0; /* next note to play */
static volatile uint16_t msLeft = 0; /* time left on current note */

/* ===============================================================
 * TONE GENERATION (TCB1)
 * =============================================================== */

/**
 * @brief TCB1 interrupt. Toggles the buzzer pin to make the square wave.
 */
ISR(TCB1_INT_vect)
{
    TCB1.INTFLAGS = TCB_CAPT_bm; /* clear flag so it can fire again */
    PORTD.OUTTGL = BUZZER_PIN; /* flip pin, two flips = one sound wave */
}

/**
 * @brief Start playing a frequency (one wave does two flips), or stop the
 * sound.
 * @param freq Frequency in Hz, or 0 for silence.
 */
static void startTone(uint16_t freq)
{
    TCB1.CTRLA = 0; /* stop the timer */
    PORTD.OUTCLR = BUZZER_PIN; /* pin low, so never left stuck on */

    if (freq == 0) {
        return; /* silence */
    }

    TCB1.CNT = 0; /* reset timer counter */
    /* how high to count before ringing- pitch */
    TCB1.CCMP = (uint16_t)(F_CPU / (2UL * freq) - 1);
    TCB1.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;
}

/* ===============================================================
 * PUBLIC FUNCTIONS
 * =============================================================== */

/**
 * @brief Set up the buzzer pin and TCB1 (left off until a note plays).
 */
void buzzer_init(void)
{
    PORTD.DIRSET = BUZZER_PIN; /* make buzzer pin an output */
    PORTD.OUTCLR = BUZZER_PIN; /* start */

    TCB1.CTRLB = TCB_CNTMODE_INT_gc; /* timer mode: count up, ring,
    start again */
    TCB1.INTCTRL = TCB_CAPT_bm; /* allow timer to trigger ISR */
    TCB1.CTRLA = 0; /* timer off until a note plays */

    CPUINT.LVL1VEC = TCB1_INT_vect_num; /* make ISR high priority,
    lands on time */
}

/**
 * @brief Called every 1 ms. Starts the next queued note when the
 *        current one ends, or goes quiet when the queue is empty.
 */
void buzzer_tick(void)
{
    if (msLeft > 0) { /* is note playing? */
        msLeft--; /* take off 1ms */
        if (msLeft > 0) { /* still time left */
            return; /* current note still playing */
        }
    }

    if (head == tail) { /* queue empty */
        if (TCB1.CTRLA != 0) { /* if buzzer not switched off */
            startTone(0); /* switch off */
        }
        return;
    }

    startTone(queue[tail].freq); /* looks at next note, read pitch and plays */
    msLeft = queue[tail].ms; /* read next note's length and set countdown */
    tail = (uint8_t)((tail + 1) % QUEUE_SIZE); /* set next note, if past 15,
    wrap back to 0 */
}

/**
 * @brief Add a note to the end of the queue. Dropped if the queue is full.
 * @param freq Frequency in Hz, or 0 for silence.
 * @param ms Length of the note in milliseconds.
 */
static void addNote(uint16_t freq, uint16_t ms)
{
    /* pause all interrupts until end of block, no interrupt change */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        /* check where next head is*/
        uint8_t next = (uint8_t)((head + 1) % QUEUE_SIZE);
        if (next != tail) { /* if list isn't full, add note */
            queue[head].freq = freq;
            queue[head].ms = ms;
            head = next; /* move head to next slot in queue */
        }
    }
}

/* ===============================================================
 * GAME SOUNDS
 * =============================================================== */

/**
 * @brief Block fixed in place: C6 (1046 Hz) for 100 ms.
 * For game.c when block lands- adds one note to queue- 1046Hz for 100ms
 */
void play_block_fixed(void)
{
    addNote(1046, NOTE_MS);
}

/**
 * @brief Lines cleared: C7 (2093 Hz) once per line, with a gap between.
 * Used in game.c when rows are cleared- add notes to queue- 2093Hz for 100ms
 * Per line cleared
 * @param lines Number of lines cleared at once (1-4).
 */
void play_lines_cleared(uint8_t lines)
{
    for (uint8_t i = 0; i < lines; i++) {
        addNote(2093, NOTE_MS);
        if (i < lines - 1) { // if not last pass
            addNote(0, GAP_MS); // ad silent gap between beeps
        }
    }
}

/**
 * @brief Compaction: A6, G6, E6 (1760, 1567, 1318 Hz), 100 ms each.
 * Called by game.c when compaction- adds three notes- 1760Hz, 1567Hz, 1318Hz
 */
void play_compaction(void)
{
    addNote(1760, NOTE_MS);
    addNote(1567, NOTE_MS);
    addNote(1318, NOTE_MS);
}
