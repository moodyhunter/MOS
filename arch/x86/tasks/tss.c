// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/x86/tasks/tss_types.h"
#include "mos/x86/x86_platform.h"

// TSS used by the system for ring-changing-interrupts
tss32_t tss_entry;

extern char stack_top;

void x86_tss_init()
{
    memset(&tss_entry, 0, sizeof(tss_entry));
    tss_entry.ss0 = GDT_SEGMENT_KDATA;
    tss32_flush(GDT_SEGMENT_TSS);
}
