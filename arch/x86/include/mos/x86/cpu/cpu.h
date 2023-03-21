// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/types.h>

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

should_inline void x86_cpu_set_cr3(reg_t cr3)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3));
}

should_inline void x86_cpu_invlpg(uintptr_t addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

should_inline void x86_cpu_invlpg_all(void)
{
    __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" : : : "eax");
}

should_inline void x86_cpu_invlpg_range(uintptr_t start, uintptr_t end)
{
    for (uintptr_t addr = start; addr < end; addr += MOS_PAGE_SIZE)
    {
        x86_cpu_invlpg(addr);
    }
}
