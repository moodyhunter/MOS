// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

void plic_enable_irq(u32 irq);

u32 plic_claim_irq();

void plic_complete(u32 irq);
