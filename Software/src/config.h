/**
 * @file config.h
 * @brief Game settings, stored in EEPROM and set from the PC over UART.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* ===============================================================
 * CONFIGURATION DATA
 * =============================================================== */

/**
 * @brief User-configurable game settings.
 *
 * These values control the normal and fast drop rates, the amount
 * that the drop interval decreases with score, and whether test mode
 * is enabled.
 */
typedef struct {
    uint16_t slow_interval; /* ms per row at score 0 (1-1000) */
    uint16_t fast_interval; /* fastest ms per row (1-1000, < slow) */
    uint32_t score_step; /* us faster per point (0-100000) */
    bool test_mode; /* true = alphabetical blocks, test buttons on */
} GameConfig;

/* Active configuration used by the rest of the game */
extern GameConfig config;

/* ===============================================================
 * CONFIGURATION SETUP AND UART SERVICE
 * =============================================================== */

/**
 * @brief Initialise configuration handling and load saved EEPROM values.
 */
void config_init(void);

/**
 * @brief Process any pending configuration commands received over UART.
 */
void config_service(void);

/* ===============================================================
 * RESET REQUEST HANDLING
 * =============================================================== */

/**
 * @brief Check whether new settings require a return to PRESS START.
 * @return true if a reset is currently requested.
 */
bool config_reset_requested(void);

/**
 * @brief Clear the pending configuration reset request.
 */
void config_clear_reset_request(void);

/* ===============================================================
 * CURRENT DROP INTERVAL
 * =============================================================== */

/**
 * @brief Record the current effective block drop interval.
 *
 * This value is reported to the PC when the current configuration
 * is requested.
 *
 * @param interval_us Current drop interval in microseconds.
 */
void config_set_current_drop_interval(uint32_t interval_us);

#endif /* CONFIG_H */