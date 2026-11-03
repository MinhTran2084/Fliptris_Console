/**
 * @file uart.h
 * @brief USART0 serial link to the PC (through the Xiao).
 */

#ifndef UART_H
#define UART_H

/* CPU clock frequency used for USART baud-rate calculations */
#ifndef F_CPU
#define F_CPU 4000000UL
#endif

#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>

/* ===============================================================
 * SETUP
 * =============================================================== */

/**
 * @brief Initialise USART0 for communication with the PC.
 * @param baud_rate Serial baud rate to use.
 */
void uart_init(uint32_t baud_rate);

/* ===============================================================
 * RECEIVE
 * =============================================================== */

/**
 * @brief Check whether received UART data is waiting.
 * @return true if at least one byte is available.
 */
bool uart_available(void);

/**
 * @brief Read the next received UART byte.
 * @return Next received character, or '\0' if none is available.
 */
char uart_read_char(void);

/* ===============================================================
 * TRANSMIT
 * =============================================================== */

/**
 * @brief Send one character over USART0.
 * @param c Character to send.
 */
void uart_send_char(char c);

/**
 * @brief Send a null-terminated string over USART0.
 * @param text String to send.
 */
void uart_send_string(const char* text);

/**
 * @brief Send an unsigned 32-bit value as decimal text.
 * @param value Number to send.
 */
void uart_send_uint32(uint32_t value);

#endif /* UART_H */