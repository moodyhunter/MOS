// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/serial.hpp"

#include <mos/mos_global.h>
#include <mos/syslog/printk.hpp>

bool ISerialDevice::setup()
{
    SetInterrupts(INTERRUPT_NONE);
    SetBaudrateDivisor();
    SetDataBits();
    SetStopBits();
    SetParity();

    SetModemOptions(MODEM_DTR, true);
    SetModemOptions(MODEM_RTS, true);

    // Try send a byte to the serial port.
    // If it fails, then the serial port is not connected.
    {
        const char challenge = 'H';
        SetModemOptions(MODEM_LOOP, true);
        WriteByte(challenge);
        const auto response = ReadByte();
        SetModemOptions(MODEM_LOOP, false);
        if (response != 'H')
            return false;
    }

    SetModemOptions(MODEM_IRQ, true);
    SetInterrupts(INTERRUPT_DATA_AVAILABLE);
    return true;
}

int ISerialDevice::read_data(char *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        WaitReadyToRead();
        data[i] = ReadByte();
    }

    return length;
}

int ISerialDevice::write_data(const char *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        WaitReadyToWrite();
        WriteByte(data[i]);
    }

    return length;
}

void ISerialDevice::SetBaudrateDivisor()
{
    // Set the most significant bit of the Line Control Register. This is the DLAB bit, and allows access to the divisor registers.
    u8 reg = read_register(OFFSET_LINE_CONTROL);
    reg |= 0x80; // set DLAB bit

    write_register(OFFSET_LINE_CONTROL, reg);

    // Send the least significant byte of the divisor value to [PORT + 0].
    write_register(OFFSET_DLAB_DIVISOR_LSB, baudrate_divisor & 0xFF);

    // Send the most significant byte of the divisor value to [PORT + 1].
    write_register(OFFSET_DLAB_DIVISOR_MSB, baudrate_divisor >> 8);

    // Clear the most significant bit of the Line Control Register.
    reg &= ~0x80;
    write_register(OFFSET_LINE_CONTROL, reg);
}

void ISerialDevice::SetDataBits()
{
    u8 control = read_register(OFFSET_LINE_CONTROL);
    control &= char_length;
    write_register(OFFSET_LINE_CONTROL, control);
}

void ISerialDevice::SetStopBits()
{
    byte_t control = { .byte = read_register(OFFSET_LINE_CONTROL) };
    control.bits.b1 = stop_bits == STOP_BITS_15_OR_2;
    write_register(OFFSET_LINE_CONTROL, control.byte);
}

void ISerialDevice::SetParity()
{
    u8 byte = read_register(OFFSET_LINE_CONTROL);
    byte |= ((u8) parity) << 3;
    write_register(OFFSET_LINE_CONTROL, byte);
}

void ISerialDevice::SetInterrupts(int interrupts)
{
    char control = read_register(OFFSET_INTERRUPT_ENABLE);
    control = interrupts;
    write_register(OFFSET_INTERRUPT_ENABLE, control);
}

void ISerialDevice::SetModemOptions(serial_modem_control_t control, bool enable)
{
    byte_t byte = { .byte = read_register(OFFSET_MODEM_CONTROL) };
    switch (control)
    {
        case MODEM_DTR: byte.bits.b0 = enable; break;
        case MODEM_RTS: byte.bits.b1 = enable; break;
        case MODEM_UNUSED_PIN1: byte.bits.b2 = enable; break;
        case MODEM_IRQ: byte.bits.b3 = enable; break;
        case MODEM_LOOP: byte.bits.b4 = enable; break;
    }
    write_register(OFFSET_MODEM_CONTROL, byte.byte);
}

char ISerialDevice::GetLineStatus()
{
    return read_register(OFFSET_LINE_STATUS);
}

__maybe_unused char ISerialDevice::GetModelStatus()
{
    return read_register(OFFSET_MODEM_STATUS);
}

bool ISerialDevice::GetDataReady()
{
    return GetLineStatus() & LINE_DATA_READY;
}

void ISerialDevice::WaitReadyToRead()
{
    while (!GetDataReady())
        ;
}

void ISerialDevice::WaitReadyToWrite()
{
    while (!(GetLineStatus() & LINE_TRANSMITR_BUF_EMPTY))
        ;
}
