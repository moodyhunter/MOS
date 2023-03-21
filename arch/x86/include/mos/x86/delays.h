// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.h>

always_inline u64 rdtsc(void)
{
#if MOS_BITS == 32
    u64 x;
    __asm__ volatile("rdtsc" : "=A"(x));
    return x;
#elif MOS_BITS == 64
    u64 a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return (d << 32) | a;
#else
#error "Unsupported MOS_BITS"
#endif
}

always_inline void mdelay(u64 ms)
{
    u64 end = rdtsc() + ms * 2000 * 1000;
    while (rdtsc() < end)
        ;
}

always_inline void udelay(u64 us)
{
    u64 end = rdtsc() + us * 2000;
    while (rdtsc() < end)
        ;
}
