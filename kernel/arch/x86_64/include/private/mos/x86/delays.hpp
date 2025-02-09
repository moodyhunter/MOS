// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.hpp>

should_inline u64 rdtsc(void)
{
    u64 a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d) : : "memory");
    return (d << 32) | a;
}

should_inline void mdelay(u64 ms)
{
    u64 end = rdtsc() + ms * 2000 * 1000;
    while (rdtsc() < end)
        ;
}

should_inline void udelay(u64 us)
{
    u64 end = rdtsc() + us * 2000;
    while (rdtsc() < end)
        ;
}
