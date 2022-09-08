// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/x86_interrupt.h"

#define PIC1         0x20 // IO base address for master PIC
#define PIC2         0xA0 // IO base address for slave  PIC
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

// End-of-interrupt command code
#define PIC_EOI 0x20

void pic_remap_irq(int offset_master, int offset_slave);
void pic_mask_irq(x86_irq_enum_t irq);
void pic_unmask_irq(x86_irq_enum_t irq);
void pic_disable(void);
