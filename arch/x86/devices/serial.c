// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/devices/serial.h"

#include "mos/printk.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/x86_interrupt.h"

static inline char serial_irq_read_char(serial_port_t port)
{
    u8 status = port_inb(port + 5);
    if (status & 0x01)
        return port_inb(port);
    return 0;
}

void serial_irq_handler(u32 irq)
{
    if (irq == IRQ_COM1)
    {
        char c = serial_irq_read_char(COM1);
        pr_info("COM1: (%2d) %c", c, c);
    }
    else if (irq == IRQ_COM2)
    {
    }
    else
    {
        pr_warn("Unknown serial IRQ: %d", irq);
    }
}

void set_baudrate_divisor(int com, serial_baud_rate divisor)
{
    // Set the most significant bit of the Line Control Register. This is the DLAB bit, and allows access to the divisor registers.
    byte_t control = to_union(byte_t) port_inb(com + OFFSET_LINE_CONTROL);
    control.bits.msb = true;

    port_outb(com + OFFSET_LINE_CONTROL, control.byte);

    // Send the least significant byte of the divisor value to [PORT + 0].
    port_outb(com + OFFSET_DLAB_DIVISOR_LSB, divisor & 0xFF);

    // Send the most significant byte of the divisor value to [PORT + 1].
    port_outb(com + OFFSET_DLAB_DIVISOR_MSB, divisor >> 8);

    // Clear the most significant bit of the Line Control Register.
    control.bits.msb = false; // clear DLAB bit
    port_outb(com + OFFSET_LINE_CONTROL, control.byte);
}

void set_data_bits(int com, serial_char_length_t length)
{
    char control = port_inb(com + OFFSET_LINE_CONTROL);
    control &= length;
    port_outb(com + OFFSET_LINE_CONTROL, control);
}

void set_stop_bits(int com, serial_stop_bits_t stop_bits)
{
    byte_t control = to_union(byte_t) port_inb(com + OFFSET_LINE_CONTROL);
    control.bits.b1 = stop_bits == STOP_BITS_15_OR_2;
    port_outb(com + OFFSET_LINE_CONTROL, control.byte);
}

void set_parity(int com, serial_port_parity_t parity)
{
    u8 byte = port_inb(com + OFFSET_LINE_CONTROL);
    byte |= ((u8) parity) << 3;
    port_outb(com + OFFSET_LINE_CONTROL, byte);
}

void serial_set_interrupts(int com, int interrupts)
{
    char control = port_inb(com + OFFSET_INTTERUPT_ENABLE);
    control = interrupts;
    port_outb(com + OFFSET_INTTERUPT_ENABLE, control);
}

void serial_set_modem_options(int com, serial_modem_control_t control, bool enable)
{
    byte_t byte = to_union(byte_t) port_inb(com + OFFSET_MODEM_CONTROL);
    switch (control)
    {
        case MODEM_DTR: byte.bits.b0 = enable; break;
        case MODEM_RTS: byte.bits.b1 = enable; break;
        case MODEM_UNUSED_PIN1: byte.bits.b2 = enable; break;
        case MODEM_IRQ: byte.bits.b3 = enable; break;
        case MODEM_LOOP: byte.bits.b4 = enable; break;
    }
    port_outb(com + OFFSET_MODEM_CONTROL, byte.byte);
}

char serial_get_line_status(int com)
{
    return port_inb(com + OFFSET_LINE_STATUS);
}

char serial_get_modem_status(int com)
{
    return port_inb(com + OFFSET_MODEM_STATUS);
}

bool serial_device_setup(serial_device_t *device)
{
    serial_port_t port = device->port;

    serial_set_interrupts(port, INTERRUPT_NONE);
    set_baudrate_divisor(port, device->baud_rate);
    set_data_bits(port, device->char_length);
    set_stop_bits(port, device->stop_bits);
    set_parity(port, device->parity);

    serial_set_modem_options(port, MODEM_DTR, true);
    serial_set_modem_options(port, MODEM_RTS, true);

    // Try send a byte to the serial port.
    // If it fails, then the serial port is not connected.
    {
        const char challenge = 'H';
        char response = { 0 };
        serial_set_modem_options(port, MODEM_LOOP, true);
        serial_device_write(device, &challenge, 1);
        serial_device_read(device, &response, 1);
        serial_set_modem_options(port, MODEM_LOOP, false);
        if (response != 'H')
            return false;
    }

    serial_set_modem_options(port, MODEM_IRQ, true);
    serial_set_interrupts(port, INTERRUPT_DATA_AVAILABLE);
    return true;
}

void serial_dev_wait_ready_to_read(serial_device_t *device)
{
    serial_port_t port = device->port;
    while (!(serial_get_line_status(port) & LINE_DATA_READY))
        ;
}

void serial_dev_wait_ready_to_write(serial_device_t *device)
{
    serial_port_t port = device->port;
    while (!(serial_get_line_status(port) & LINE_TRANSMITR_BUF_EMPTY))
        ;
}

int serial_device_write(serial_device_t *device, const char *data, size_t length)
{
    serial_port_t port = device->port;
    size_t i = 0;
    for (; i < length; i++)
    {
        serial_dev_wait_ready_to_write(device);
        port_outb(port, data[i]);
    }
    return i;
}

int serial_device_read(serial_device_t *device, char *data, size_t length)
{
    serial_port_t port = device->port;
    size_t i = 0;
    while (i < length)
    {
        serial_dev_wait_ready_to_read(device);
        data[i] = port_inb(port);
        i++;
    }
    return i;
}

int serial_dev_readline(serial_device_t *device, char *buffer, int max_length)
{
    serial_port_t port = device->port;
    int i = 0;
    while (i < max_length)
    {
        serial_dev_wait_ready_to_read(device);
        char c = port_inb(port);
        if (c == '\r')
            break;
        if (c == '\n')
            break;
        buffer[i] = c;
        i++;
    }
    buffer[i] = '\0';
    return i;
}
