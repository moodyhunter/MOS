// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdlib.h"

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

u8 outb(u16 port, u8 value)
{
    __asm__ volatile("outb %1, %0" : : "dN"(port), "a"(value));
    return value;
}

u8 inb(u16 port)
{
    u8 value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "dN"(port));
    return value;
}
