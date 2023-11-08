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

#define x86_cpuid(return_reg, ...)                                                                                                                                       \
    __extension__({                                                                                                                                                      \
        u32 a = 0, b = 0, c = 0, d = 0, leaf = 0;                                                                                                                        \
        __VA_ARGS__;                                                                                                                                                     \
        __get_cpuid(leaf, &a, &b, &c, &d);                                                                                                                               \
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

void x86_cpu_get_caps_all(void);
