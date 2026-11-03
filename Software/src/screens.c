/**
 * @file screens.c
 * @brief Scrolling text screens: "Team NN", "PRESS START", the score
 *        message, and the controller test screen.
 */

#ifndef F_CPU
#define F_CPU 4000000UL
#endif
#include "fliptris_internal.h"

/* ===============================================================
 * FONT
 * =============================================================== */

/* 5x7 font: five columns per character, bit 0 = top pixel */
static const uint8_t FONT_LETTERS[26][5] = {
    /* A */ { 0x7E, 0x11, 0x11, 0x11, 0x7E },
    /* B */ { 0x7F, 0x49, 0x49, 0x49, 0x36 },
    /* C */ { 0x3E, 0x41, 0x41, 0x41, 0x22 },
    /* D */ { 0x7F, 0x41, 0x41, 0x22, 0x1C },
    /* E */ { 0x7F, 0x49, 0x49, 0x49, 0x41 },
    /* F */ { 0x7F, 0x09, 0x09, 0x09, 0x01 },
    /* G */ { 0x3E, 0x41, 0x49, 0x49, 0x7A },
    /* H */ { 0x7F, 0x08, 0x08, 0x08, 0x7F },
    /* I */ { 0x00, 0x41, 0x7F, 0x41, 0x00 },
    /* J */ { 0x20, 0x40, 0x41, 0x3F, 0x01 },
    /* K */ { 0x7F, 0x08, 0x14, 0x22, 0x41 },
    /* L */ { 0x7F, 0x40, 0x40, 0x40, 0x40 },
    /* M */ { 0x7F, 0x02, 0x0C, 0x02, 0x7F },
    /* N */ { 0x7F, 0x04, 0x08, 0x10, 0x7F },
    /* O */ { 0x3E, 0x41, 0x41, 0x41, 0x3E },
    /* P */ { 0x7F, 0x09, 0x09, 0x09, 0x06 },
    /* Q */ { 0x3E, 0x41, 0x51, 0x21, 0x5E },
    /* R */ { 0x7F, 0x09, 0x19, 0x29, 0x46 },
    /* S */ { 0x46, 0x49, 0x49, 0x49, 0x31 },
    /* T */ { 0x01, 0x01, 0x7F, 0x01, 0x01 },
    /* U */ { 0x3F, 0x40, 0x40, 0x40, 0x3F },
    /* V */ { 0x1F, 0x20, 0x40, 0x20, 0x1F },
    /* W */ { 0x7F, 0x20, 0x18, 0x20, 0x7F },
    /* X */ { 0x63, 0x14, 0x08, 0x14, 0x63 },
    /* Y */ { 0x03, 0x04, 0x78, 0x04, 0x03 },
    /* Z */ { 0x61, 0x51, 0x49, 0x45, 0x43 }
};

/* 5x7 glyphs for decimal digits 0-9 */
static const uint8_t FONT_DIGITS[10][5] = {
    /* 0 */ { 0x3E, 0x51, 0x49, 0x45, 0x3E },
    /* 1 */ { 0x00, 0x42, 0x7F, 0x40, 0x00 },
    /* 2 */ { 0x42, 0x61, 0x51, 0x49, 0x46 },
    /* 3 */ { 0x21, 0x41, 0x45, 0x4B, 0x31 },
    /* 4 */ { 0x18, 0x14, 0x12, 0x7F, 0x10 },
    /* 5 */ { 0x27, 0x45, 0x45, 0x45, 0x39 },
    /* 6 */ { 0x3C, 0x4A, 0x49, 0x49, 0x30 },
    /* 7 */ { 0x01, 0x71, 0x09, 0x05, 0x03 },
    /* 8 */ { 0x36, 0x49, 0x49, 0x49, 0x36 },
    /* 9 */ { 0x06, 0x49, 0x49, 0x29, 0x1E }
};

/**
 * @brief One column of a character's 5x7 glyph.
 *
 * Lowercase letters are shown as uppercase. Unknown characters and
 * spaces are blank.
 *
 * @param c Character.
 * @param column Glyph column, 0-4.
 * @return Column bits, bit 0 = top pixel.
 */
static uint8_t getGlyphColumn(char c, uint8_t column)
{
    /* Convert lowercase letters so only one alphabet table is needed */
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    /* Alphabetic characters use the letter glyph table */
    if (c >= 'A' && c <= 'Z') {
        return FONT_LETTERS[c - 'A'][column];
    }

    /* Decimal characters use the digit glyph table */
    if (c >= '0' && c <= '9') {
        return FONT_DIGITS[c - '0'][column];
    }

    /* A few punctuation marks used by game messages have custom glyphs */
    switch (c) {
    case ':': {
        static const uint8_t glyph[5] = { 0x00, 0x36, 0x36, 0x00, 0x00 };
        return glyph[column];
    }

    case '-': {
        static const uint8_t glyph[5] = { 0x08, 0x08, 0x08, 0x08, 0x08 };
        return glyph[column];
    }

    case '!': {
        static const uint8_t glyph[5] = { 0x00, 0x00, 0x5F, 0x00, 0x00 };
        return glyph[column];
    }

    /* Spaces and unsupported characters are displayed blank */
    case ' ':
    default:
        return 0;
    }
}

/* ===============================================================
 * SCROLLING TEXT
 * =============================================================== */

/**
 * @brief Draw one frame of scrolling text.
 * @param text Text to draw.
 * @param xOffset Screen column of the first character (negative =
 *                partly scrolled off the left).
 */
static void renderTextFrame(const char* text, int16_t xOffset)
{
    /* Temporary frame buffer containing only the current text frame */
    uint8_t buffer[HEIGHT] = { 0 };

    /* Number of characters that must be rendered */
    uint8_t length = (uint8_t)strlen(text);

    /* Draw each character at its current horizontal position */
    for (uint8_t character = 0; character < length; character++) {
        /* 5 glyph columns + 1 blank column per character */
        int16_t characterX = xOffset + ((int16_t)character * 6);

        /* Draw the five columns that make up the character */
        for (uint8_t glyphColumn = 0; glyphColumn < 5; glyphColumn++) {
            int16_t screenX = characterX + glyphColumn;

            /* Ignore portions of text that are currently off-screen */
            if (screenX < 0 || screenX >= WIDTH) {
                continue;
            }

            /* Read the vertical pixel pattern for this glyph column */
            uint8_t bits = getGlyphColumn(text[character], glyphColumn);

            /* Copy each lit glyph pixel into the display buffer */
            for (uint8_t glyphRow = 0; glyphRow < 7; glyphRow++) {
                if (bits & (1U << glyphRow)) {
                    uint8_t screenY = TEXT_TOP + glyphRow;
                    buffer[screenY] |= (uint8_t)(1U << screenX);
                }
            }
        }
    }

    /* Send the completed text frame to the physical display */
    displayShowBuffer(buffer);
}

/**
 * @brief Scroll a message from right to left once, checking for PC
 *        commands every 10 ms.
 *
 * If new settings arrive, it stops straight away and leaves the reset
 * flag set for waitForStart() to handle.
 *
 * @param text Message to scroll.
 * @return true if it finished, false if interrupted by new settings.
 */
static bool scrollTextOnce(const char* text)
{
    /* Six columns per character: five visible columns plus one gap */
    int16_t textWidth = (int16_t)strlen(text) * 6;

    /* start fully off the right, finish fully off the left */
    for (int16_t x = WIDTH; x >= -textWidth; x--) {
        renderTextFrame(text, x);

        /*
         * Break the scroll delay into 10 ms pieces so PC commands
         * remain responsive while the text is moving.
         */
        for (uint8_t i = 0; i < TEXT_SCROLL_MS / 10; i++) {
            config_service();

            /* A SET command interrupts the current screen immediately */
            if (config_reset_requested()) {
                clearPhysicalDisplay();
                return false;
            }

            _delay_ms(10);
        }
    }

    /* Remove the message once it has completely left the display */
    clearPhysicalDisplay();
    return true;
}

/**
 * @brief Scroll "Team NN" once at power-up.
 */
void showTeamIntro(void)
{
    char teamText[] = "Team 00";

    /* fill in TEAM_NUMBER */
    teamText[5] = (char)('0' + ((TEAM_NUMBER / 10) % 10));
    teamText[6] = (char)('0' + (TEAM_NUMBER % 10));

    /* Display the completed team number as a scrolling message */
    scrollTextOnce(teamText);
}

/* ===============================================================
 * CONTROLLER TEST SCREEN
 * =============================================================== */

/**
 * @brief Show one pixel per button, lit while pressed.
 *
 * Layout (rows 4, 7, 10; columns 1, 3, 5, 7):
 *   B    Y     SELECT START
 *   UP   DOWN  LEFT   RIGHT
 *   A    X     L      R
 *
 * @param buttons Button bits.
 */
static void showControllerTest(uint16_t buttons)
{
    /* Controller test is drawn into its own temporary display buffer */
    uint8_t buffer[HEIGHT] = { 0 };

    /* Map each of the 12 controller buttons into a 3x4 pixel grid */
    for (uint8_t button = 0; button < 12; button++) {
        uint8_t gridRow = button / 4;
        uint8_t gridCol = button % 4;

        /* Spread the test pixels apart so each button is easy to identify */
        uint8_t row = (uint8_t)(4 + gridRow * 3);
        uint8_t col = (uint8_t)(1 + gridCol * 2);

        /* Light the corresponding pixel whenever this button is held */
        if (isHeld(buttons, button)) {
            buffer[row] |= (uint8_t)(1U << col);
        }
    }

    /* Show the complete controller-state grid */
    displayShowBuffer(buffer);
}

/**
 * @brief Run the controller test while L is held. Leaves early if new
 *        settings arrive from the PC.
 */
static void controllerTestMode(void)
{
    while (1) {
        /* Keep UART commands responsive while the test screen is active */
        config_service();

        /* New configuration ends the test and returns toward PRESS START */
        if (config_reset_requested()) {
            config_clear_reset_request();
            clearPhysicalDisplay();
            return;
        }

        /* Read raw controller state so every button can be displayed */
        uint16_t buttons = readButtons();

        if (!isHeld(buttons, BUTTON_L)) {
            break; /* test only runs while L is held */
        }

        showControllerTest(buttons);

        /* Short refresh delay keeps the test responsive */
        _delay_ms(20);
    }

    /* Remove the controller test display when L is released */
    clearPhysicalDisplay();
}

/* ===============================================================
 * PRESS START
 * =============================================================== */

/** @brief What ended one scroll of PRESS START. */
typedef enum {
    WAIT_EVENT_NONE,
    WAIT_EVENT_START,
    WAIT_EVENT_CONTROLLER_TEST
} WaitEvent;

/**
 * @brief Scroll "PRESS START" once, checking the controller and PC
 *        every 10 ms.
 * @return Start pressed, L held (controller test), or neither.
 */
static WaitEvent scrollPressStartOnce(void)
{
    const char* text = "PRESS START";

    /* Width includes the one-column space after each character */
    int16_t textWidth = (int16_t)strlen(text) * 6;

    /* Scroll one complete copy of PRESS START across the display */
    for (int16_t x = WIDTH; x >= -textWidth; x--) {
        renderTextFrame(text, x);

        /*
         * Poll several times during each text frame so controller
         * input and PC commands are not delayed by the scrolling.
         */
        for (uint8_t poll = 0; poll < (TEXT_SCROLL_MS / 10); poll++) {
            config_service();

            /* A new SET is already at the required PRESS START state */
            if (config_reset_requested()) {
                config_clear_reset_request(); /* already at PRESS START */
                clearPhysicalDisplay();
                return WAIT_EVENT_NONE;
            }

            /* Read controller buttons directly during the waiting screen */
            uint16_t buttons = readButtons();

            /* Start immediately when the Start button is detected */
            if (isHeld(buttons, BUTTON_START)) {
                return WAIT_EVENT_START;
            }

            /* Holding L temporarily enters controller test mode */
            if (isHeld(buttons, BUTTON_L)) {
                return WAIT_EVENT_CONTROLLER_TEST;
            }

            _delay_ms(10);
        }
    }

    /* Nothing happened during this complete scroll */
    return WAIT_EVENT_NONE;
}

/**
 * @brief Repeat "PRESS START" until Start is pressed. Holding L opens
 *        the controller test.
 */
void waitForStart(void)
{
    /* Ensure no previous game or text frame remains visible */
    clearPhysicalDisplay();

    while (1) {
        /* Run one complete PRESS START scroll and inspect how it ended */
        WaitEvent event = scrollPressStartOnce();

        /* Start exits the waiting screen and allows gameplay to begin */
        if (event == WAIT_EVENT_START) {
            clearPhysicalDisplay();
            return;
        }

        /* L opens the temporary controller diagnostic screen */
        if (event == WAIT_EVENT_CONTROLLER_TEST) {
            controllerTestMode();
            clearPhysicalDisplay(); /* L released: back to PRESS START */
        }
    }
}

/* ===============================================================
 * GAME OVER
 * =============================================================== */

/**
 * @brief Append a number to the end of a string.
 * @param destination String to append to (must have room).
 * @param value Number to append.
 */
static void appendUint16(char* destination, uint16_t value)
{
    /* Maximum uint16_t value requires five decimal digits */
    char digits[5];
    uint8_t count = 0;

    /* digits come out backwards */
    do {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && count < 5);

    /* Start writing after the existing text */
    uint8_t length = (uint8_t)strlen(destination);

    /* Append stored digits in reverse order to restore normal number order */
    while (count > 0) {
        destination[length++] = digits[--count];
    }

    /* Terminate the expanded string */
    destination[length] = '\0';
}

/**
 * @brief Scroll "Score: XYZ - WELL DONE!" twice (spec), unless new
 *        settings arrive.
 */
void showGameOverMessage(void)
{
    /* Build the complete game-over message in a local string buffer */
    char message[32] = "Score: ";

    /* Insert the final score before the congratulatory text */
    appendUint16(message, score);
    strcat(message, " - WELL DONE!");

    /*
     * The specification requires two complete scrolls.
     * If the first is interrupted by new settings, skip the second.
     */
    if (scrollTextOnce(message)) {
        scrollTextOnce(message);
    }

    /* Leave the display clear before returning to PRESS START */
    clearPhysicalDisplay();
}