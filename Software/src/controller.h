/**
 * @file controller.h
 * @brief SNES controller reading and button handling.
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

/* ===============================================================
 * BUTTON DEFINITIONS
 * =============================================================== */

/* Bit position of each button in the 16-bit controller read */
#define BUTTON_B 0
#define BUTTON_Y 1
#define BUTTON_SELECT 2
#define BUTTON_START 3
#define BUTTON_UP 4
#define BUTTON_DOWN 5
#define BUTTON_LEFT 6
#define BUTTON_RIGHT 7
#define BUTTON_A 8
#define BUTTON_X 9
#define BUTTON_L 10
#define BUTTON_R 11

/* ===============================================================
 * MOVEMENT TIMING
 * =============================================================== */

/* Number of ticks between repeated sideways movements */
#define REPEAT_TICKS 12

/* Tick value associated with fast dropping */
#define FAST_DROP_TICK 5

/* ===============================================================
 * CONTROLLER SETUP AND READING
 * =============================================================== */

/**
 * @brief Initialise the SNES controller interface.
 */
void controllerInit(void);

/**
 * @brief Read the current state of all controller buttons.
 * @return 16-bit button-state bitmask.
 */
uint16_t readButtons(void);

/* ===============================================================
 * BUTTON STATE HELPERS
 * =============================================================== */

/**
 * @brief Check whether a specific button is currently held.
 * @param buttons Controller button-state bitmask.
 * @param button Button bit position to test.
 * @return true if the button is held.
 */
bool isHeld(uint16_t buttons, uint8_t button);

/**
 * @brief Check whether a specific button has just been pressed.
 * @param buttons Current controller button-state bitmask.
 * @param button Button bit position to test.
 * @return true if the button was newly pressed.
 */
bool wasJustPressed(uint16_t buttons, uint8_t button);

/* ===============================================================
 * GAMEPLAY CONTROLS
 * =============================================================== */

/**
 * @brief Handle block rotation from controller input.
 * @param buttons Current controller button-state bitmask.
 */
void handleRotation(uint16_t buttons);

/**
 * @brief Handle left and right block movement.
 * @param buttons Current controller button-state bitmask.
 */
void handleSideways(uint16_t buttons);

/**
 * @brief Calculate the current automatic block drop interval.
 * @param buttons Current controller button-state bitmask.
 * @return Drop interval in microseconds.
 */
uint32_t dropInterval(uint16_t buttons);

#endif /* CONTROLLER_H */