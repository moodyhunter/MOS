// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/x86_interrupt.h"

void pic_remap_irq(int offset_master, int offset_slave);
void pic_mask_irq(x86_irq_enum_t irq);
void pic_unmask_irq(x86_irq_enum_t irq);
void pic_disable(void);
