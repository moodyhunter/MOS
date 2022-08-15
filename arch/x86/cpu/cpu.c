// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpu.h"

void cpu_get_msr(u32 msr, u32 *lo, u32 *hi)
{
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

void cpu_set_msr(u32 msr, u32 lo, u32 hi)
{
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

void x86_cpu_halt()
{
    __asm__ volatile("hlt");
}
