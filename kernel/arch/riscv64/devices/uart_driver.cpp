// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/riscv64/devices/uart_driver.hpp"

#include "mos/device/serial.hpp"

#include <mos/types.hpp>

static u8 riscv64_serial_read_register(serial_device_t *dev, serial_register_t reg)
{
    volatile u8 *const ptr = (volatile u8 *) dev->driver_data;
    return READ_ONCE(ptr[reg]);
}

static void riscv64_serial_write_register(serial_device_t *dev, serial_register_t reg, u8 value)
{
    volatile u8 *const ptr = (volatile u8 *) dev->driver_data;
    ptr[reg] = value;
}

static u8 riscv64_serial_read_data(serial_device_t *dev)
{
    volatile u8 *const ptr = (volatile u8 *) dev->driver_data;
    return ptr[0];
}

static void riscv64_serial_write_data(serial_device_t *dev, u8 data)
{
    volatile u8 *const ptr = (volatile u8 *) dev->driver_data;
    ptr[0] = data;
}

const serial_driver_t riscv64_uart_driver = {
    .read_data = riscv64_serial_read_data,
    .write_data = riscv64_serial_write_data,
    .read_register = riscv64_serial_read_register,
    .write_register = riscv64_serial_write_register,
};
