/**
 * @file uart.c
 * @brief USART0 serial link to the PC (PA0 = TX, PA1 = RX).
 *
 * Received bytes are stored in a ring buffer by an interrupt, so no
 * bytes are lost while the game is busy.
 */

#include "uart.h"

#include <avr/interrupt.h>

#define UART_RX_BUFFER_SIZE 64 /* plenty for "SET,500,100,10000,1\n" */

/* ===============================================================
 * RECEIVE BUFFER
 * =============================================================== */

/* Circular buffer holding bytes received from the PC */
static volatile char rxBuffer[UART_RX_BUFFER_SIZE];

/* Index where the next received byte will be stored */
static volatile uint8_t rxHead = 0;

/* Index of the next unread byte */
static volatile uint8_t rxTail = 0;

/* ===============================================================
 * SETUP
 * =============================================================== */

/**
 * @brief Work out the USART BAUD register value.
 * @param baud_rate Baud rate.
 * @return BAUD register value.
 */
static uint16_t calculate_baud_register(uint32_t baud_rate)
{
    /*
     * AVR Dx USART BAUD setting for normal asynchronous mode.
     * F_CPU is the processor clock frequency.
     */
    return (uint16_t)((64UL * F_CPU) / (16UL * baud_rate));
}

/**
 * @brief Start USART0 at 8N1 with the receive interrupt, and enable
 *        global interrupts.
 * @param baud_rate Baud rate.
 */
void uart_init(uint32_t baud_rate)
{
    /* Use the default USART0 pins: PA0 for TX and PA1 for RX */
    PORTMUX.USARTROUTEA = PORTMUX_USART0_DEFAULT_gc; /* PA0 TX, PA1 RX */

    /* Set the requested baud rate and standard 8N1 frame format */
    USART0.BAUD = calculate_baud_register(baud_rate);
    USART0.CTRLC = USART_CHSIZE_8BIT_gc; /* 8 data bits, no parity, 1 stop */

    /* Configure the USART pins in the required directions */
    PORTA.DIRSET = PIN0_bm; /* TX output */
    PORTA.DIRCLR = PIN1_bm; /* RX input */

    /* Start with an empty receive ring buffer */
    rxHead = 0;
    rxTail = 0;

    /* Enable both transmitting and receiving */
    USART0.CTRLB = USART_TXEN_bm | USART_RXEN_bm;

    /* Interrupt whenever a new receive byte is available */
    USART0.CTRLA = USART_RXCIE_bm; /* receive-complete interrupt */

    /* Allow USART and other enabled interrupts to run */
    sei();
}

/* ===============================================================
 * RECEIVE
 * =============================================================== */

/**
 * @brief Receive interrupt. Stores each byte in the ring buffer.
 *
 * If the buffer is full, the newest byte is dropped rather than
 * overwriting unread data.
 */
ISR(USART0_RXC_vect)
{
    /* Reading RXDATAL clears the received-byte condition */
    char received = USART0.RXDATAL;

    /* Work out where the head would move after storing this byte */
    uint8_t nextHead = (uint8_t)((rxHead + 1) % UART_RX_BUFFER_SIZE);

    /*
     * If nextHead would equal rxTail the ring buffer is full.
     * Leave the existing unread data untouched and drop this byte.
     */
    if (nextHead != rxTail) {
        rxBuffer[rxHead] = received;
        rxHead = nextHead;
    }
}

/**
 * @brief Check for received data.
 * @return true if at least one byte is waiting.
 */
bool uart_available(void)
{
    /* Head and tail differ whenever unread bytes are present */
    return rxHead != rxTail;
}

/**
 * @brief Take the next received byte.
 * @return The byte, or '\0' if none is waiting.
 */
char uart_read_char(void)
{
    /* Empty buffer: there is nothing available to return */
    if (rxHead == rxTail) {
        return '\0';
    }

    /* Read the oldest unread character */
    char c = rxBuffer[rxTail];

    /* Advance the tail and wrap around at the end of the buffer */
    rxTail = (uint8_t)((rxTail + 1) % UART_RX_BUFFER_SIZE);

    return c;
}

/* ===============================================================
 * TRANSMIT
 * =============================================================== */

/**
 * @brief Send one character, waiting until the transmitter is ready.
 * @param c Character to send.
 */
void uart_send_char(char c)
{
    /*
     * Wait until the USART data register is empty and ready
     * to accept another byte.
     */
    while ((USART0.STATUS & USART_DREIF_bm) == 0) {
    }

    /* Writing TXDATAL starts transmission of this character */
    USART0.TXDATAL = c;
}

/**
 * @brief Send a null-terminated string.
 * @param text String to send.
 */
void uart_send_string(const char* text)
{
    /* Send characters one at a time until the C string terminator */
    while (*text != '\0') {
        uart_send_char(*text);
        text++;
    }
}

/**
 * @brief Send an unsigned number as decimal text.
 * @param value Number to send.
 */
void uart_send_uint32(uint32_t value)
{
    /* Maximum uint32_t value requires 10 decimal digits */
    char digits[10];
    uint8_t count = 0;

    /* Zero needs special handling because the loop below would not run */
    if (value == 0) {
        uart_send_char('0');
        return;
    }

    /* digits come out backwards */
    while (value > 0) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }

    /* Send the stored digits in reverse order to restore normal order */
    while (count > 0) {
        uart_send_char(digits[--count]);
    }
}