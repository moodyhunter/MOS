// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef u16 port_t;

u8 port_inb(port_t port);
u16 port_inw(port_t port);
u32 port_inl(port_t port);

void port_outb(port_t port, u8 value);
void port_outw(port_t port, u16 value);
void port_outl(port_t port, u32 value);
