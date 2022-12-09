// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/x86_interrupt.h"

void pic_remap_irq(int offset_master, int offset_slave);
