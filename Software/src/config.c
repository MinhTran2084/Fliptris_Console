/**
 * @file config.c
 * @brief Game settings: PC commands over UART and EEPROM storage.
 *
 * Commands (each ends with a newline):
 *   GET                  reply CONFIG,slow,fast,step,test,current_interval_ms
 *   SET,slow,fast,step,test   reply OK, or ERR,FORMAT / ERR,RANGE / ERR,COMMAND
 *
 * A valid SET is saved to EEPROM and returns the game to PRESS START.
 * GET never affects the game.
 */

#include "config.h"
#include "uart.h"

#include <avr/eeprom.h>
#include <stdlib.h>
#include <string.h>

#define COMMAND_BUFFER_SIZE 48

/* ===============================================================
 * DEFAULT CONFIGURATION
 * =============================================================== */

/* Default settings, used until valid ones are loaded or received */
GameConfig config = {
    500, /* slow_interval in ms */
    100, /* fast_interval in ms */
    10000, /* score_step in us */
    false /* test_mode */
};

/* ===============================================================
 * UART COMMAND STATE
 * =============================================================== */

/* Holds one complete incoming command before it is processed */
static char commandBuffer[COMMAND_BUFFER_SIZE];

/* Next free position in the command buffer */
static uint8_t commandIndex = 0;

/* Set after a successful SET so the game can return to PRESS START */
static bool resetRequested = false;

/* Latest effective drop interval, reported to the PC by GET */
static uint32_t currentDropIntervalUs = 500000UL; /* reported by GET */

/* ===============================================================
 * HELPERS
 * =============================================================== */

/**
 * @brief Convert decimal text to a number.
 * @param text Text to convert.
 * @param value Where to store the result.
 * @return false if the text is empty or not entirely digits.
 */
static bool parse_number(const char* text, uint32_t* value)
{
    /* Empty strings are not valid numeric parameters */
    if (text == NULL || *text == '\0') {
        return false;
    }

    /* strtoul reports where numeric parsing stopped */
    char* end;
    unsigned long result = strtoul(text, &end, 10);

    /* Reject anything containing non-numeric trailing characters */
    if (*end != '\0') {
        return false; /* something after the number */
    }

    *value = (uint32_t)result;
    return true;
}

/**
 * @brief Check settings against the specification.
 *
 * slow and fast: 1-1000 ms, with fast < slow.
 * score_step: 0-100000 us. test mode: 0 or 1.
 *
 * @return true if every value is in range.
 */
static bool config_values_valid(uint32_t slow, uint32_t fast, uint32_t scoreStep, uint32_t testMode)
{
    /* Slow interval must be within the allowed millisecond range */
    if (slow < 1 || slow > 1000) {
        return false;
    }

    /* Fast interval uses the same allowed millisecond range */
    if (fast < 1 || fast > 1000) {
        return false;
    }

    /* Fast interval must always represent a faster rate than slow */
    if (fast >= slow) {
        return false;
    }

    /* Score step is specified in microseconds */
    if (scoreStep > 100000UL) {
        return false;
    }

    /* Test mode is transferred as either 0 or 1 */
    if (testMode > 1) {
        return false;
    }

    return true;
}

/* ===============================================================
 * EEPROM STORAGE
 * =============================================================== */

#define CONFIG_MAGIC 0xF117u /* marks the EEPROM as holding saved settings */

/**
 * @brief Settings as stored in EEPROM, with a magic number and checksum.
 *
 * The magic value identifies valid Fliptris data. The checksum is used
 * to reject incomplete or corrupted EEPROM contents.
 */
typedef struct {
    uint16_t magic;
    uint16_t slow;
    uint16_t fast;
    uint32_t step;
    uint8_t test;
    uint8_t check;
} StoredConfig;

/* EEPROM-backed copy of the saved configuration */
static StoredConfig EEMEM storedConfig;

/**
 * @brief Checksum over every byte except the checksum itself.
 * @param s Stored settings.
 * @return Checksum byte.
 */
static uint8_t stored_checksum(const StoredConfig* s)
{
    /* Treat the structure as a sequence of bytes */
    const uint8_t* p = (const uint8_t*)s;

    /* Non-zero starting value helps distinguish empty data */
    uint8_t sum = 0xA5;

    /* Rotate and XOR each stored byte except the checksum field */
    for (uint8_t i = 0; i < sizeof(StoredConfig) - 1; i++) {
        sum = (uint8_t)((sum << 1) | (sum >> 7)) ^ p[i];
    }

    return sum;
}

/**
 * @brief Save the current settings. Only bytes that changed are written.
 */
static void config_save(void)
{
    /* Build an EEPROM representation from the active configuration */
    StoredConfig s = {
        CONFIG_MAGIC,
        config.slow_interval,
        config.fast_interval,
        config.score_step,
        config.test_mode ? 1 : 0,
        0
    };

    /* Calculate checksum after all other fields have been filled */
    s.check = stored_checksum(&s);

    /* eeprom_update_block avoids rewriting bytes that already match */
    eeprom_update_block(&s, &storedConfig, sizeof s);
}

/**
 * @brief Load saved settings. If the EEPROM is blank or corrupt, the
 *        defaults are kept.
 */
static void config_load(void)
{
    StoredConfig s;

    /* Copy the stored EEPROM bytes into RAM for checking */
    eeprom_read_block(&s, &storedConfig, sizeof s);

    /*
     * Accept the saved values only when the magic number, checksum,
     * and specification ranges are all valid.
     */
    if (s.magic == CONFIG_MAGIC && s.check == stored_checksum(&s) && config_values_valid(s.slow, s.fast, s.step, s.test)) {
        config.slow_interval = s.slow;
        config.fast_interval = s.fast;
        config.score_step = s.step;
        config.test_mode = (s.test != 0);
    }
}

/* ===============================================================
 * COMMANDS
 * =============================================================== */

/**
 * @brief Reply to GET, e.g. "CONFIG,500,100,10000,0,500".
 *
 * The last field is the drop interval currently in use, in ms.
 */
static void send_configuration(void)
{
    /* Prefix identifies the response as configuration data */
    uart_send_string("CONFIG,");

    /* Send configured slow interval */
    uart_send_uint32(config.slow_interval);
    uart_send_char(',');

    /* Send configured fast interval */
    uart_send_uint32(config.fast_interval);
    uart_send_char(',');

    /* Send score-dependent interval reduction */
    uart_send_uint32(config.score_step);
    uart_send_char(',');

    /* Test mode is transmitted as a single 0 or 1 */
    if (config.test_mode) {
        uart_send_char('1');
    } else {
        uart_send_char('0');
    }

    uart_send_char(',');

    /* Convert the live interval from microseconds to milliseconds */
    uart_send_uint32(currentDropIntervalUs / 1000UL);

    /* Commands and responses are newline terminated */
    uart_send_char('\n');
}

/**
 * @brief Handle one complete command line (GET or SET).
 * @param command Command text, without the newline.
 */
static void process_command(char* command)
{
    /* GET: report settings, no effect on the game */
    if (strcmp(command, "GET") == 0) {
        send_configuration();
        return;
    }

    /* SET,slow,fast,step,test */
    char* token = strtok(command, ",");

    /* The first token must identify a SET command */
    if (token == NULL || strcmp(token, "SET") != 0) {
        uart_send_string("ERR,COMMAND\n");
        return;
    }

    /* Read the four expected configuration fields */
    char* slowText = strtok(NULL, ",");
    char* fastText = strtok(NULL, ",");
    char* scoreText = strtok(NULL, ",");
    char* testText = strtok(NULL, ",");

    /* A fifth field would mean the command format is invalid */
    char* extra = strtok(NULL, ","); /* must be NULL: exactly four fields */

    /*
     * Every required field must be present and there must not
     * be any additional fields after test mode.
     */
    if (slowText == NULL || fastText == NULL || scoreText == NULL || testText == NULL || extra != NULL) {
        uart_send_string("ERR,FORMAT\n");
        return;
    }

    /* Parsed numeric values are stored here before validation */
    uint32_t slow;
    uint32_t fast;
    uint32_t scoreStep;
    uint32_t testMode;

    /* Reject the command if any field is not purely numeric */
    if (!parse_number(slowText, &slow)
        || !parse_number(fastText, &fast)
        || !parse_number(scoreText, &scoreStep)
        || !parse_number(testText, &testMode)) {
        uart_send_string("ERR,FORMAT\n");
        return;
    }

    /* Reject values outside the assignment specification */
    if (!config_values_valid(slow, fast, scoreStep, testMode)) {
        uart_send_string("ERR,RANGE\n");
        return;
    }

    /* apply, save, confirm */
    config.slow_interval = (uint16_t)slow;
    config.fast_interval = (uint16_t)fast;
    config.score_step = scoreStep;
    config.test_mode = (testMode != 0);

    /* Save the accepted values so they survive a power cycle */
    config_save();

    /* Confirm successful receipt and storage */
    uart_send_string("OK\n");

    /* spec: sending new settings returns the game to PRESS START */
    resetRequested = true;
}

/* ===============================================================
 * PUBLIC FUNCTIONS
 * =============================================================== */

/**
 * @brief Reset the command parser and load saved settings.
 */
void config_init(void)
{
    /* Start with an empty UART command buffer */
    commandIndex = 0;

    /* No game reset is pending at startup */
    resetRequested = false;

    /* Replace defaults with valid EEPROM settings if available */
    config_load();
}

/**
 * @brief Process any received characters. Non-blocking; call often.
 *
 * Accepts lines ending in \n or \r\n.
 */
void config_service(void)
{
    /* Process every byte currently waiting in the UART receive buffer */
    while (uart_available()) {
        char c = uart_read_char();

        /* Ignore carriage returns so both \n and \r\n are accepted */
        if (c == '\r') {
            continue;
        }

        /* newline: command complete */
        if (c == '\n') {
            /* Turn the collected bytes into a normal C string */
            commandBuffer[commandIndex] = '\0';

            /* Ignore completely empty command lines */
            if (commandIndex > 0) {
                process_command(commandBuffer);
            }

            /* Start collecting the next command from the beginning */
            commandIndex = 0;
            continue;
        }

        /* Store normal characters while there is space remaining */
        if (commandIndex < COMMAND_BUFFER_SIZE - 1) {
            commandBuffer[commandIndex] = c;
            commandIndex++;
        } else {
            /* too long: discard and start again */
            commandIndex = 0;
            uart_send_string("ERR,TOOLONG\n");
        }
    }
}

/**
 * @brief Check whether a SET has asked for a return to PRESS START.
 * @return true if a reset is pending.
 */
bool config_reset_requested(void)
{
    return resetRequested;
}

/**
 * @brief Clear the pending reset once it has been handled.
 */
void config_clear_reset_request(void)
{
    resetRequested = false;
}

/**
 * @brief Record the drop interval in use, so GET can report it.
 * @param interval_us Drop interval in microseconds.
 */
void config_set_current_drop_interval(uint32_t interval_us)
{
    /* Stored in microseconds and converted to milliseconds when sent */
    currentDropIntervalUs = interval_us;
}