/**
 * @file app.c
 * @brief Fliptris start-up and main game loop.
 *
 * Runs every TICK ms: handles PC commands, reads the controller,
 * moves and drops the block, and redraws the display.
 */

#ifndef F_CPU
#define F_CPU 4000000UL
#endif
#include "fliptris_internal.h"

/**
 * @brief Initialise all hardware and run the game forever.
 * @return Never returns.
 */
int fliptris_run(void)
{
    /* Accumulates elapsed time until the active block should drop */
    uint32_t dropElapsedUs = 0; /* time built up towards the next drop (us) */

    /* Previous millisecond timestamp used to measure real elapsed time */
    uint32_t lastMs = 0; /* last time the drop timer was updated */

    /* Timestamp marking the beginning of each main-loop iteration */
    uint32_t loopStart = 0; /* start of the current loop pass */

    /* ===========================================================
     * HARDWARE INITIALISATION
     * =========================================================== */

    /*
     * Initialise each hardware subsystem before entering any
     * start-up screens or game logic.
     */
    timerInit();
    displayInit();
    controllerInit();
    i2c_init();
    buzzer_init();

    /* Start serial communication and restore saved configuration */
    uart_init(9600); /* also enables interrupts */
    config_init(); /* loads saved settings from EEPROM */

    /* ===========================================================
     * ACCELEROMETER
     * =========================================================== */

    /* set up and calibrate the LIS3DH while the device is upright and still */
    uint8_t who_am_i = 0;

    /*
     * Check the LIS3DH identity register before configuring it.
     * 0x33 is the expected WHO_AM_I response for this accelerometer.
     */
    if (i2c_read_registers(LIS3DH_ADDR, 0x0F, &who_am_i, 1)
        && (who_am_i == 0x33)) {
        i2c_write_register(LIS3DH_ADDR, 0x20, 0x47); /* 50 Hz, X/Y/Z on */
        calibrate_accelerometer();
    }

    /* Initialise the random sequence used for new game blocks */
    seedBlockRandomiser();

    /* ===========================================================
     * START-UP SCREENS AND FIRST GAME
     * =========================================================== */

    /* Show the team number once after power-up */
    showTeamIntro();

    /* Stay at PRESS START until the player begins */
    waitForStart();

    /* Set up all variables needed for the first game */
    startGame();

    /* Start the automatic-drop timing from zero */
    dropElapsedUs = 0;
    lastMs = millis();
    loopStart = lastMs;

    /* a button already held (e.g. Start) doesn't count as a new press */
    lastButtons = readButtons();

    /* Draw the initial game state */
    displayShow();

    /* ===========================================================
     * MAIN GAME LOOP
     * =========================================================== */

    while (1) {
        /* wait for the next tick, timed by the hardware timer */
        while ((uint32_t)(millis() - loopStart) < TICK) {
        }

        /* Record the start time of this loop iteration */
        loopStart = millis();

        /* -------------------------------------------------------
         * PC CONFIGURATION: new settings return to PRESS START
         * ------------------------------------------------------- */

        /* Process any complete GET or SET commands received over UART */
        config_service();

        /*
         * A successful SET requests an immediate return to
         * the PRESS START screen as required by the specification.
         */
        if (config_reset_requested()) {
            config_clear_reset_request();

            /* Ensure preview mode and old display contents are removed */
            previewing = false;
            clearPhysicalDisplay();

            /* Wait for the player before starting with the new settings */
            waitForStart();
            startGame();

            /* Restart all automatic-drop timing for the new game */
            dropElapsedUs = 0;
            lastMs = millis();
            loopStart = lastMs;

            /* Ignore buttons that were already held when the game starts */
            lastButtons = readButtons();

            /* Draw the fresh game state */
            displayShow();
            continue;
        }

        /* -------------------------------------------------------
         * CONTROLLER INPUT
         * ------------------------------------------------------- */

        /* Read a debounced snapshot of all controller buttons */
        uint16_t buttons = readButtonsDebounced();

        /* Select: show the next block (game keeps running) */
        previewing = isHeld(buttons, BUTTON_SELECT);

        /* test mode X: end the game now */
        if (config.test_mode && wasJustPressed(buttons, BUTTON_X)) {
            finished = true;
        }

        /* test mode R: pause automatic dropping (other controls still work) */
        bool automaticDropPaused = config.test_mode && isHeld(buttons, BUTTON_R);

        /* A/B rotate, left/right move (not if X just ended the game) */
        if (!finished) {
            handleRotation(buttons);
            handleSideways(buttons);
        }

        /* -------------------------------------------------------
         * AUTOMATIC DROP
         *
         * Real elapsed time is added up, so the rate is exact even
         * when the interval is shorter than the loop (several rows
         * then drop in one pass). Nothing is added while R pauses.
         * ------------------------------------------------------- */

        {
            /* Measure the real elapsed time since the previous update */
            uint32_t now = millis();
            uint32_t elapsedMs = now - lastMs;
            lastMs = now;

            /*
             * Do not build drop time after game over or while test-mode
             * R is deliberately pausing automatic falling.
             */
            if (!finished && !automaticDropPaused) {
                /* Work out the current interval from controls and score */
                uint32_t intervalUs = dropInterval(buttons);

                /*
                 * Save the effective interval so GET can report the
                 * current live drop rate to the PC GUI.
                 */
                config_set_current_drop_interval(intervalUs);

                if (dropElapsedUs >= intervalUs) {
                    /* rate just got faster (e.g. Down): drop one row now, not several */
                    dropElapsedUs = intervalUs;
                } else {
                    /* Convert elapsed milliseconds to microseconds */
                    dropElapsedUs += elapsedMs * 1000UL;
                }

                /*
                 * Drop as many rows as needed if more than one complete
                 * interval elapsed before this loop iteration.
                 */
                while (!finished && dropElapsedUs >= intervalUs) {
                    dropElapsedUs -= intervalUs;

                    /*
                     * dropBlock() reports when the falling block lands
                     * and a new active block is created.
                     */
                    if (dropBlock()) {
                        /* block landed: the new block starts with a fresh timer */
                        dropElapsedUs = 0;
                        lastMs = millis();
                        break;
                    }
                }
            }
        }

        /* -------------------------------------------------------
         * GAME OVER (no room to spawn, or X in test mode)
         * ------------------------------------------------------- */

        if (finished) {
            /* Never show the preview block during the game-over screen */
            previewing = false;

            /* Show the required final-score message before restarting */
            showGameOverMessage(); /* "Score: XYZ - WELL DONE!" twice */

            /* Wait for Start before beginning another game */
            waitForStart();

            /* Reset all game-state variables for the next game */
            startGame();

            /* Begin the next game's drop timing from zero */
            dropElapsedUs = 0;
            lastMs = millis();
            loopStart = lastMs;

            /* buttons still held don't count as new presses */
            lastButtons = readButtons();

            /* Draw the newly started game */
            displayShow();
            continue;
        }

        /* -------------------------------------------------------
         * END OF TICK
         * ------------------------------------------------------- */

        /* Store this button state for edge detection next tick */
        lastButtons = buttons; /* needed by wasJustPressed() */

        /* Redraw the game after all movement and timing updates */
        displayShow();
    }
}