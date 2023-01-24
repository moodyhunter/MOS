// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/types.h"

should_inline void cpu_get_msr(u32 msr, u32 *lo, u32 *hi)
{
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

should_inline void cpu_set_msr(u32 msr, u32 lo, u32 hi)
{
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

should_inline noreturn void x86_cpu_halt(void)
{
    __asm__ volatile("hlt");
    mos_panic("halt failed");
}

should_inline reg_t x86_get_cr3(void)
{
    reg_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

always_inline u32 x86_cpu_get_id(void)
{
    int lapic_id = 0;
    __asm__ volatile("cpuid" : "=b"(lapic_id) : "a"(0x01), "c"(0x00) : "edx");
    return lapic_id >> 24;
}
