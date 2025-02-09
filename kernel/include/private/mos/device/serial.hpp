// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.hpp>

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
} serial_register_t;

typedef enum
{
    CHAR_LENGTH_5 = 0x0,
    CHAR_LENGTH_6 = 0x1,
    CHAR_LENGTH_7 = 0x2,
    CHAR_LENGTH_8 = 0x3,
} serial_charlength_t;

typedef enum
{
    STOP_BITS_1,       // 1 stop bit
    STOP_BITS_15_OR_2, // 1.5 or 2 stop bits
} serial_stopbits_t;

typedef enum
{
    PARITY_NONE = 0,  // No parity
    PARITY_ODD = 1,   // Odd parity
    PARITY_EVEN = 2,  // Even parity
    PARITY_MARK = 3,  // Parity bit always 1
    PARITY_SPACE = 4, // Parity bit always 0
} serial_parity_t;

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
} serial_baudrate_t;

typedef struct _serial_device serial_device_t;

typedef struct _serial_driver
{
    u8 (*read_data)(serial_device_t *dev);
    void (*write_data)(serial_device_t *dev, u8 data);

    u8 (*read_register)(serial_device_t *dev, serial_register_t offset);
    void (*write_register)(serial_device_t *dev, serial_register_t offset, u8 value);
} serial_driver_t;

typedef struct _serial_device
{
    const serial_driver_t *driver;
    void *driver_data;

    serial_baudrate_t baudrate_divisor;
    serial_charlength_t char_length;
    serial_stopbits_t stop_bits;
    serial_parity_t parity;
} serial_device_t;

bool serial_device_setup(serial_device_t *device);

int serial_device_read(serial_device_t *device, char *data, size_t length);

int serial_device_write(serial_device_t *device, const char *data, size_t length);

bool serial_dev_get_data_ready(serial_device_t *device);
