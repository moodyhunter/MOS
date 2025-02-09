// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.hpp>

typedef u16 x86_port_t;

should_inline u8 port_inb(u16 port)
{
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

should_inline u16 port_inw(x86_port_t port)
{
    u16 value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

should_inline u32 port_inl(x86_port_t port)
{
    u32 value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "dN"(port));
    return value;
}

should_inline void port_outb(u16 port, u8 value)
{
    __asm__ volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

should_inline void port_outw(x86_port_t port, u16 value)
{
    __asm__ volatile("outw %1, %0" : : "dN"(port), "a"(value));
}

should_inline void port_outl(x86_port_t port, u32 value)
{
    __asm__ volatile("outl %1, %0" : : "dN"(port), "a"(value));
}
