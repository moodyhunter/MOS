// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

typedef enum
{
    COM1 = 0x3F8,
    COM2 = 0x2F8,
    COM3 = 0x3E8,
    COM4 = 0x2E8,
    COM5 = 0x5F8,
    COM6 = 0x4F8,
    COM7 = 0x5E8,
    COM8 = 0x4E8
} serial_port_t;

typedef enum
{
    OFFSET_INTERRUPT_ENABLE = 1,  // Interrupt Enable Register
    OFFSET_INTERRUPT_ID_FIFO = 2, // Interrupt ID Register and FIFO Control Register
    OFFSET_LINE_CONTROL = 3,      // Line Control Register
    OFFSET_MODEM_CONTROL = 4,     // Modem Control Register
    OFFSET_LINE_STATUS = 5,       // Line Status Register
    OFFSET_MODEM_STATUS = 6,      // Modem Status Register
    OFFSET_SCRATCH = 7,           // Scratch Register

    OFFSET_DLAB_DIVISOR_LSB = 0, // With DLAB set to 1, this is the least significant byte of the divisor value for setting the baud rate.
    OFFSET_DLAB_DIVISOR_MSB = 1, // With DLAB set to 1, this is the most significant byte of the divisor value.
} serial_register_offset_t;

typedef enum
{
    CHAR_LENGTH_5 = 0x0,
    CHAR_LENGTH_6 = 0x1,
    CHAR_LENGTH_7 = 0x2,
    CHAR_LENGTH_8 = 0x3,
} serial_char_length_t;

typedef enum
{
    STOP_BITS_1,       // 1 stop bit
    STOP_BITS_15_OR_2, // 1.5 or 2 stop bits
} serial_stop_bits_t;

typedef enum
{
    PARITY_NONE = 0,  // No parity
    PARITY_ODD = 1,   // Odd parity
    PARITY_EVEN = 2,  // Even parity
    PARITY_MARK = 3,  // Parity bit always 1
    PARITY_SPACE = 4, // Parity bit always 0
} serial_port_parity_t;

typedef enum
{
    INTERRUPT_DATA_AVAILABLE = 1 << 0,    // Data ready interrupt
    INTERRUPT_TRANSMITTER_EMPTY = 1 << 1, // Transmitter empty interrupt
    INTERRUPT_BREAK_ERROR = 1 << 2,       // Break error interrupt
    INTERRUPT_STATUS_CHANGE = 1 << 3,     // Status change interrupt
    INTERRUPT_NONE = 0,                   // No interrupts
    INTERRUPT_ALL = INTERRUPT_DATA_AVAILABLE | INTERRUPT_TRANSMITTER_EMPTY | INTERRUPT_BREAK_ERROR | INTERRUPT_STATUS_CHANGE,
} serial_interrupt_t;

typedef enum
{
    MODEM_DTR = 1 << 0,         // Data Terminal Ready
    MODEM_RTS = 1 << 1,         // Request To Send
    MODEM_UNUSED_PIN1 = 1 << 2, // Unused
    MODEM_IRQ = 1 << 3,         // Interrupt Request
    MODEM_LOOP = 1 << 4,        // Loopback
} serial_modem_control_t;

typedef enum
{
    LINE_DATA_READY = 1 << 0,          // Data ready to be read.
    LINE_ERR_OVERRUN = 1 << 1,         // There has been data lost.
    LINE_ERR_PARITY = 1 << 2,          // Parity error.
    LINE_ERR_FRAMING = 1 << 3,         // Stop bit is missing.
    LINE_ERR_BREAK = 1 << 4,           // Break detected.
    LINE_TRANSMITR_BUF_EMPTY = 1 << 5, // (transmitter buffer is empty) Data can be sent.
    LINE_TRANSMITR_EMPTY = 1 << 6,     // Transmitter is not doing anything.
    LINE_ERR_IMPENDING = 1 << 7,       // There is an error with a word in the input buffer
} serial_line_status_t;

typedef enum
{
    MODEM_DCTS = 1 << 0, // Clear To Send input has changed since last read.
    MODEM_DDSR = 1 << 1, // Data Set Ready input has changed since last read.
    MODEM_TERI = 1 << 2, // Ring Indicator input has changed since last read.
    MODEM_DDCD = 1 << 3, // Data Carrier Detect input has changed since last read.
    MODEM_CLEAR_TO_SEND = 1 << 4,
    MODEM_DATA_SET_READY = 1 << 5,
    MODEM_RING_INDICATOR = 1 << 6,
    MODEM_DATA_CARRIER_DETECT = 1 << 7,
} serial_modem_status_t;

typedef enum
{
    BAUD_RATE_115200 = 1,
    BAUD_RATE_57600 = 2,
    BAUD_RATE_38400 = 3,
    BAUD_RATE_19200 = 4,
    BAUD_RATE_9600 = 5,
    BAUD_RATE_4800 = 6,
    BAUD_RATE_2400 = 7,
    BAUD_RATE_1200 = 8,
    BAUD_RATE_600 = 9,
    BAUD_RATE_300 = 10,
    BAUD_RATE_110 = 11,
} serial_baud_rate;

typedef struct
{
    serial_port_t port;
    serial_baud_rate baud_rate;
    serial_char_length_t char_length;
    serial_stop_bits_t stop_bits;
    serial_port_parity_t parity;
} serial_device_t;

bool serial_device_setup(serial_device_t *device);
int serial_device_read(serial_device_t *device, char *data, size_t length);
int serial_device_write(serial_device_t *device, const char *data, size_t length);
bool serial_dev_get_data_ready(serial_device_t *device);
