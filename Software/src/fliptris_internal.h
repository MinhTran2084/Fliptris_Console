/**
 * @file fliptris_internal.h
 * @brief Constants, pin assignments, shared variables and functions
 *        used between the Fliptris source files.
 */

#ifndef FLIPTRIS_INTERNAL_H
#define FLIPTRIS_INTERNAL_H

#include "buzzer.h"
#include "config.h"
#include "uart.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <util/atomic.h>
#include <util/delay.h>

/* ===============================================================
 * PLAYFIELD
 * =============================================================== */

#define WIDTH 8
#define HEIGHT 16

#define TICK 10 /* main loop period (ms) */

#define START_ROW 1 /* spawn row for blocks with a cell above the pivot */
#define START_COL 4 /* spawn column of the pivot */

#define SHAPES 7
#define CELLS 4
#define SHAPE_O 3

/* ===============================================================
 * MAX7219 LED DRIVERS (PORTA)
 * =============================================================== */

#define LOAD_PIN PIN7_bm
#define CLK_PIN PIN6_bm
#define DATA_PIN PIN4_bm

#define DRIVERS 2
#define DRIVER_ROWS 8

#define REG_NOOP 0x00
#define REG_DIGIT0 0x01
#define REG_DECODE 0x09
#define REG_INTENSITY 0x0A
#define REG_SCANLIMIT 0x0B
#define REG_SHUTDOWN 0x0C
#define REG_TEST 0x0F

#define BRIGHTNESS 0x04

/* ===============================================================
 * SNES CONTROLLER (PORTC)
 * =============================================================== */

#define CLOCK_PIN PIN0_bm
#define LATCH_PIN PIN1_bm
#define BUTTON_DATA_PIN PIN2_bm

#define BUTTON_COUNT 16
#define HALF_CLOCK_US 24
#define LATCH_HOLD_US 48

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

#define REPEAT_DELAY_TICKS 25 /* hold 250 ms before left/right auto-repeat */
#define REPEAT_TICKS 12 /* then repeat every 120 ms */
#define FAST_DROP_TICK 5

/* ===============================================================
 * LIS3DH ACCELEROMETER (I2C)
 * =============================================================== */

#define LIS3DH_ADDR 0x18
#define CAL_SAMPLES 32
#define TILT_THRESHOLD_45_DEG 11585 /* 1 g x sin(45) at +-2 g full scale */

/* ===============================================================
 * TEXT DISPLAY
 * =============================================================== */

#define TEAM_NUMBER 33
#define TEXT_SCROLL_MS 40 /* ms per pixel column */
#define TEXT_TOP 4 /* row that vertically centres 5x7 text */

/* ===============================================================
 * SHARED TYPES AND VARIABLES
 * =============================================================== */

/** @brief One cell of a block, as an offset from the block's pivot. */
typedef struct {
    int8_t down;
    int8_t right;
} Cell;

extern const Cell SHAPE_CELLS[SHAPES][CELLS];
extern uint8_t board[HEIGHT];
extern uint8_t nextShape;
extern uint16_t score;
extern bool finished;
extern bool previewing;
extern uint16_t lastButtons;
extern uint8_t holdTicks;

/* ===============================================================
 * SHARED FUNCTIONS
 * =============================================================== */

/* timer.c */
void timerInit(void);
uint32_t millis(void);

/* accelerometer.c */
void i2c_init(void);
bool i2c_write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
bool i2c_read_registers(uint8_t dev_addr, uint8_t reg_addr, uint8_t* data, uint8_t length);
void calibrate_accelerometer(void);
int16_t read_accelerometer_x(void);
void apply_compaction(void);

/* game.c */
void seedBlockRandomiser(void);
void moveBlock(int8_t step);
void turnBlock(bool clockwise);
bool dropBlock(void);
void startGame(void);
uint8_t rowPattern(uint8_t row);

/* display.c */
void displayInit(void);
void displayShow(void);
void displayShowBuffer(const uint8_t buffer[HEIGHT]);
void clearPhysicalDisplay(void);

/* controller.c */
void controllerInit(void);
uint16_t readButtons(void);
uint16_t readButtonsDebounced(void);
bool isHeld(uint16_t buttons, uint8_t button);
bool wasJustPressed(uint16_t buttons, uint8_t button);
void handleRotation(uint16_t buttons);
void handleSideways(uint16_t buttons);
uint32_t dropInterval(uint16_t buttons);

/* text.c */
void showTeamIntro(void);
void waitForStart(void);
void showGameOverMessage(void);

#endif /* FLIPTRIS_INTERNAL_H */
