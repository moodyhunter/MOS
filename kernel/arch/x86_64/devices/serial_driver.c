// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/devices/serial_driver.h"

#include "mos/device/serial.h"
#include "mos/x86/devices/port.h"

static u8 serial_read_data(serial_device_t *device)
{
    x86_com_port_t port = (x86_com_port_t) (ptr_t) device->driver_data;
    return port_inb(port);
}

static void serial_write_data(serial_device_t *device, u8 data)
{
    x86_com_port_t port = (x86_com_port_t) (ptr_t) device->driver_data;
    port_outb(port, data);
}

static u8 serial_read_register(serial_device_t *device, serial_register_t reg)
{
    x86_com_port_t port = (x86_com_port_t) (ptr_t) device->driver_data;
    return port_inb(port + reg);
}

static void serial_write_register(serial_device_t *device, serial_register_t reg, u8 data)
{
    x86_com_port_t port = (x86_com_port_t) (ptr_t) device->driver_data;
    port_outb(port + reg, data);
}

const serial_driver_t x86_serial_driver = {
    .read_data = serial_read_data,
    .write_data = serial_write_data,
    .read_register = serial_read_register,
    .write_register = serial_write_register,
};
