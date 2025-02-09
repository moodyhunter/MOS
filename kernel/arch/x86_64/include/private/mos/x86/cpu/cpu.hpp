// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cpuid.h>
#include <mos/types.hpp>

should_inline u64 cpu_rdmsr(u32 msr)
{
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64) hi << 32) | lo;
}

should_inline void cpu_wrmsr(u32 msr, u64 val)
{
    u32 lo = val & 0xFFFFFFFF;
    u32 hi = val >> 32;
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

[[noreturn]] should_inline void x86_cpu_halt(void)
{
    while (true)
        __asm__ volatile("hlt");
}

#define x86_cpuid(return_reg, leaf, subleaf)                                                                                                                             \
    __extension__({                                                                                                                                                      \
        reg32_t a = 0, b = 0, c = 0, d = 0;                                                                                                                              \
        __get_cpuid_count(leaf, subleaf, &a, &b, &c, &d);                                                                                                                \
        return_reg;                                                                                                                                                      \
    })

// clang-format off
#define x86_cpu_get_crx(x) __extension__({ reg_t crx; __asm__ volatile("mov %%cr" #x ", %0" : "=r"(crx)); crx; })
#define x86_cpu_set_crx(x, val) __asm__ volatile("mov %0, %%cr" #x : : "r"(val) : "memory")
// clang-format on

#define x86_cpu_get_cr0() x86_cpu_get_crx(0)
#define x86_cpu_get_cr2() x86_cpu_get_crx(2)
#define x86_cpu_get_cr3() x86_cpu_get_crx(3)
#define x86_cpu_get_cr4() x86_cpu_get_crx(4)

#define x86_cpu_set_cr0(val) x86_cpu_set_crx(0, val)
#define x86_cpu_set_cr3(val) x86_cpu_set_crx(3, val)
#define x86_cpu_set_cr4(val) x86_cpu_set_crx(4, val)

should_inline void x86_cpu_invlpg(ptr_t addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

should_inline void x86_cpu_invlpg_all(void)
{
    __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" : : : "rax", "memory");
}

void x86_cpu_initialise_caps(void);
void x86_cpu_setup_xsave_area(void);
