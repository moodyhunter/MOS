// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/drivers/serial_console.h"

#include "mos/x86/drivers/serial.h"

bool serial_console_setup(console_t *console)
{
    console->write = serial_console_write;
    console->read = serial_console_read;
    console->caps = CONSOLE_CAP_READ;
    linked_list_init(list_node(console));
    serial_console_t *serial_console = container_of(console, serial_console_t, console);
    return serial_device_setup(&serial_console->device);
}

int serial_console_write(console_t *console, const char *str, size_t len)
{
    serial_console_t *serial_console = container_of(console, serial_console_t, console);
    return serial_device_write(&serial_console->device, str, len);
}

int serial_console_read(console_t *console, char *str, size_t len)
{
    serial_console_t *serial_console = container_of(console, serial_console_t, console);
    return serial_device_read(&serial_console->device, str, len);
}
