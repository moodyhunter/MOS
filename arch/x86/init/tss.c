// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_init.h"

// TSS used by the system for ring-changing-interrupts
tss32_t tss = { .ss0 = GDT_SEGMENT_KDATA, .iomap = sizeof(tss32_t) + 1 /* invalid iomap, as it is not used */ };

void x86_tss_init()
{
    tss32_flush(GDT_SEGMENT_TSS);
}
