// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/drivers/port.h"

u8 port_inb(u16 port)
{
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

u16 port_inw(port_t port)
{
    u16 value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

u32 port_inl(port_t port)
{
    u32 value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

void port_outb(u16 port, u8 value)
{
    __asm__ volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

void port_outw(port_t port, u16 value)
{
    __asm__ volatile("outw %1, %0" : : "dN"(port), "a"(value));
}

void port_outl(port_t port, u32 value)
{
    __asm__ volatile("outl %1, %0" : : "dN"(port), "a"(value));
}
