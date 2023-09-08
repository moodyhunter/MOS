// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cpuid.h>
#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/types.h>

should_inline void cpu_get_msr(u32 msr, u32 *lo, u32 *hi)
{
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}
#define cpu_get_msr64(msr, val) cpu_get_msr(msr, (u32 *) val, (u32 *) (val >> 32))

should_inline void cpu_set_msr(u32 msr, u32 lo, u32 hi)
{
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}
#define cpu_set_msr64(msr, val) cpu_set_msr(msr, (u32) val, (u32) (val >> 32))

should_inline noreturn void x86_cpu_halt(void)
{
    __asm__ volatile("hlt");
    mos_panic("halt failed");
}

should_inline u32 x86_cpu_get_id(void)
{
    reg_t a = 0, b = 0, c = 0, d = 0;
    __cpuid(1, a, b, c, d);
    return b >> 24;
}

#define x86_cpuid(return_reg, ...)                                                                                                                                       \
    __extension__({                                                                                                                                                      \
        reg_t a = 0, b = 0, c = 0, d = 0, leaf = 0;                                                                                                                      \
        __VA_ARGS__;                                                                                                                                                     \
        __cpuid(leaf, a, b, c, d);                                                                                                                                       \
        return_reg;                                                                                                                                                      \
    })

should_inline reg_t x86_cpu_get_cr3(void)
{
    reg_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

should_inline void x86_cpu_set_cr3(reg_t cr3)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

should_inline reg_t x86_cpu_get_cr4(void)
{
    reg_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    return cr4;
}

should_inline void x86_cpu_set_cr4(reg_t cr4)
{
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
}

should_inline void x86_cpu_invlpg(ptr_t addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

should_inline void x86_cpu_invlpg_all(void)
{
    x86_cpu_set_cr3(x86_cpu_get_cr3());
}

should_inline void x86_cpu_invlpg_range(ptr_t start, ptr_t end)
{
    for (ptr_t addr = start; addr < end; addr += MOS_PAGE_SIZE)
    {
        x86_cpu_invlpg(addr);
    }
}
