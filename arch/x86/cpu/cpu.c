// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpu.h"

#include "mos/x86/cpu/cpuid.h"

u32 x86_cpu_get_id(void)
{
    int lapic_id = 0;
    __asm__ volatile("cpuid" : "=b"(lapic_id) : "a"(0x01), "c"(0x00) : "edx");
    return lapic_id >> 24;
}
