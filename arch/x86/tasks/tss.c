// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/x86/tasks/tss_types.h"
#include "mos/x86/x86_platform.h"

// TSS used by the system for ring-changing-interrupts
x86_percpu_tss_t x86_tss;

void x86_tss_init()
{
    memzero(&x86_tss, sizeof(x86_tss));
    per_cpu(x86_tss.tss)->ss0 = GDT_SEGMENT_KDATA;
    per_cpu(x86_tss.tss)->esp0 = 0; // we will use an allocated kernel heap for the stack later, when we have kmalloc
    tss32_flush(GDT_SEGMENT_TSS);
}
