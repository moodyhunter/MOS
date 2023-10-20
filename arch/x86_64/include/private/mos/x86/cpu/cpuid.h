// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/cpu/cpu.h"

#include <mos/mos_global.h>
#include <mos/types.h>

#define CPU_FEATURE_SSE3     1, 0, c, 0           // Streaming SIMD Extensions 3
#define CPU_FEATURE_SSSE3    1, 0, c, 9           // Supplemental Streaming SIMD Extensions 3
#define CPU_FEATURE_SSE4_1   1, 0, c, 19          // Streaming SIMD Extensions 4.1
#define CPU_FEATURE_SSE4_2   1, 0, c, 20          // Streaming SIMD Extensions 4.2
#define CPU_FEATURE_X2APIC   1, 0, c, 21          // x2APIC
#define CPU_FEATURE_MOVBE    1, 0, c, 22          // MOVBE instruction
#define CPU_FEATURE_POPCNT   1, 0, c, 23          // POPCNT instruction
#define CPU_FEATURE_XSAVE    1, 0, c, 26          // XSAVE
#define CPU_FEATURE_OSXSAVE  1, 0, c, 27          // XSAVE and Processor Extended States
#define CPU_FEATURE_AVX      1, 0, c, 28          // Advanced Vector Extensions
#define CPU_FEATURE_RDRAND   1, 0, c, 30          // RDRAND instruction
#define CPU_FEATURE_FPU      1, 0, d, 0           // Floating-point unit on-chip
#define CPU_FEATURE_TSC      1, 0, d, 4           // Time Stamp Counter
#define CPU_FEATURE_MSR      1, 0, d, 5           // Model Specific Registers
#define CPU_FEATURE_PAE      1, 0, d, 6           // Physical Address Extension
#define CPU_FEATURE_APIC     1, 0, d, 9           // APIC on-chip
#define CPU_FEATURE_SEP      1, 0, d, 11          // SYSENTER and SYSEXIT instructions
#define CPU_FEATURE_PGE      1, 0, d, 13          // Page Global Enable
#define CPU_FEATURE_MMX      1, 0, d, 23          // MMX technology
#define CPU_FEATURE_FXSR     1, 0, d, 24          // FXSAVE and FXSTOR instructions
#define CPU_FEATURE_SSE      1, 0, d, 25          // Streaming SIMD Extensions
#define CPU_FEATURE_SSE2     1, 0, d, 26          // Streaming SIMD Extensions 2
#define CPU_FEATURE_LA57     7, 0, c, 16          // 5-Level Paging
#define CPU_FEATURE_AVX2     7, 0, b, 5           // Advanced Vector Extensions 2
#define CPU_FEATURE_FSGSBASE 7, 0, b, 0           // RDFSBASE, RDGSBASE, WRFSBASE, WRGSBASE
#define CPU_FEATURE_NX       0x80000001, 0, d, 20 // No-Execute Bit
#define CPU_FEATURE_PDPE1GB  0x80000001, 0, d, 26 // GB pages

#define x86_cpu_get_feature_impl(leaf, subleaf, reg, bit) (x86_cpuid(reg, a = leaf, c = subleaf) >> bit & 1)

#define cpu_get_feature(feat) x86_cpu_get_feature_impl(feat)
#define cpu_has_feature(feat)                                                                                                                                            \
    statement_expr(bool, {                                                                                                                                               \
        static int __feature = -1;                                                                                                                                       \
        if (unlikely(__feature == -1))                                                                                                                                   \
            __feature = x86_cpu_get_feature_impl(feat);                                                                                                                  \
        retval = __feature == 0;                                                                                                                                         \
    })
