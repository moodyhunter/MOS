// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "stdtypes.h"

typedef u16 port_t;

u8 inb(port_t port);
u16 inw(port_t port);
u32 inl(port_t port);

void outb(port_t port, u8 value);
void outw(port_t port, u16 value);
void outl(port_t port, u32 value);
