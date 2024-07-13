// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/serial.h"

#include <mos/mos_global.h>
#include <mos/syslog/printk.h>

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

static void set_baudrate_divisor(serial_device_t *dev)
{
    // Set the most significant bit of the Line Control Register. This is the DLAB bit, and allows access to the divisor registers.
    u8 reg = dev->driver->read_register(dev, OFFSET_LINE_CONTROL);
    reg |= 0x80; // set DLAB bit

    dev->driver->write_register(dev, OFFSET_LINE_CONTROL, reg);

    // Send the least significant byte of the divisor value to [PORT + 0].
    dev->driver->write_register(dev, OFFSET_DLAB_DIVISOR_LSB, dev->baudrate_divisor & 0xFF);

    // Send the most significant byte of the divisor value to [PORT + 1].
    dev->driver->write_register(dev, OFFSET_DLAB_DIVISOR_MSB, dev->baudrate_divisor >> 8);

    // Clear the most significant bit of the Line Control Register.
    reg &= ~0x80;
    dev->driver->write_register(dev, OFFSET_LINE_CONTROL, reg);
}

static void set_data_bits(serial_device_t *dev)
{
    u8 control = dev->driver->read_register(dev, OFFSET_LINE_CONTROL);
    control &= dev->char_length;
    dev->driver->write_register(dev, OFFSET_LINE_CONTROL, control);
}

static void set_stop_bits(serial_device_t *dev)
{
    byte_t control = { .byte = dev->driver->read_register(dev, OFFSET_LINE_CONTROL) };
    control.bits.b1 = dev->stop_bits == STOP_BITS_15_OR_2;
    dev->driver->write_register(dev, OFFSET_LINE_CONTROL, control.byte);
}

static void set_parity(serial_device_t *dev, serial_parity_t parity)
{
    u8 byte = dev->driver->read_register(dev, OFFSET_LINE_CONTROL);
    byte |= ((u8) parity) << 3;
    dev->driver->write_register(dev, OFFSET_LINE_CONTROL, byte);
}

static void serial_set_interrupts(serial_device_t *dev, int interrupts)
{
    char control = dev->driver->read_register(dev, OFFSET_INTERRUPT_ENABLE);
    control = interrupts;
    dev->driver->write_register(dev, OFFSET_INTERRUPT_ENABLE, control);
}

static void serial_set_modem_options(serial_device_t *dev, serial_modem_control_t control, bool enable)
{
    byte_t byte = { .byte = dev->driver->read_register(dev, OFFSET_MODEM_CONTROL) };
    switch (control)
    {
        case MODEM_DTR: byte.bits.b0 = enable; break;
        case MODEM_RTS: byte.bits.b1 = enable; break;
        case MODEM_UNUSED_PIN1: byte.bits.b2 = enable; break;
        case MODEM_IRQ: byte.bits.b3 = enable; break;
        case MODEM_LOOP: byte.bits.b4 = enable; break;
    }
    dev->driver->write_register(dev, OFFSET_MODEM_CONTROL, byte.byte);
}

static char serial_get_line_status(serial_device_t *dev)
{
    return dev->driver->read_register(dev, OFFSET_LINE_STATUS);
}

__maybe_unused static char serial_get_modem_status(serial_device_t *dev)
{
    return dev->driver->read_register(dev, OFFSET_MODEM_STATUS);
}

bool serial_device_setup(serial_device_t *device)
{
    serial_set_interrupts(device, INTERRUPT_NONE);
    set_baudrate_divisor(device);
    set_data_bits(device);
    set_stop_bits(device);
    set_parity(device, device->parity);

    serial_set_modem_options(device, MODEM_DTR, true);
    serial_set_modem_options(device, MODEM_RTS, true);

    // Try send a byte to the serial port.
    // If it fails, then the serial port is not connected.
    {
        const char challenge = 'H';
        char response = { 0 };
        serial_set_modem_options(device, MODEM_LOOP, true);
        serial_device_write(device, &challenge, 1);
        serial_device_read(device, &response, 1);
        serial_set_modem_options(device, MODEM_LOOP, false);
        if (response != 'H')
            return false;
    }

    serial_set_modem_options(device, MODEM_IRQ, true);
    serial_set_interrupts(device, INTERRUPT_DATA_AVAILABLE);
    return true;
}

bool serial_dev_get_data_ready(serial_device_t *device)
{
    return serial_get_line_status(device) & LINE_DATA_READY;
}

static void serial_dev_wait_ready_to_read(serial_device_t *device)
{
    while (!serial_dev_get_data_ready(device))
        ;
}

static void serial_dev_wait_ready_to_write(serial_device_t *device)
{
    while (!(serial_get_line_status(device) & LINE_TRANSMITR_BUF_EMPTY))
        ;
}

int serial_device_write(serial_device_t *device, const char *data, size_t length)
{
    size_t i = 0;
    for (; i < length; i++)
    {
        serial_dev_wait_ready_to_write(device);
        device->driver->write_data(device, data[i]);
    }
    return i;
}

int serial_device_read(serial_device_t *device, char *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        serial_dev_wait_ready_to_read(device);
        data[i] = device->driver->read_data(device);
    }

    return length;
}
