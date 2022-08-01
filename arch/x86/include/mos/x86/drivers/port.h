// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef u16 x86_port_t;

u8 port_inb(x86_port_t port);
u16 port_inw(x86_port_t port);
u32 port_inl(x86_port_t port);

void port_outb(x86_port_t port, u8 value);
void port_outw(x86_port_t port, u16 value);
void port_outl(x86_port_t port, u32 value);
