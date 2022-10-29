// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/x86/tasks/tss_types.h"
#include "mos/x86/x86_platform.h"

// TSS used by the system for ring-changing-interrupts
tss32_t tss_entry;

void x86_tss_init()
{
    memzero(&tss_entry, sizeof(tss_entry));
    tss_entry.ss0 = GDT_SEGMENT_KDATA;
    tss_entry.esp0 = 0; // we will use an allocated kernel heap for the stack later, when we have kmalloc
    tss32_flush(GDT_SEGMENT_TSS);
}
