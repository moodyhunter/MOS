// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/cpu/cpu.h"

#include <mos/mos_global.h>
#include <mos/types.h>

#define CPU_FEATURE_SSE3         1, 0, c, 0           // Streaming SIMD Extensions 3
#define CPU_FEATURE_SSSE3        1, 0, c, 9           // Supplemental Streaming SIMD Extensions 3
#define CPU_FEATURE_SSE4_1       1, 0, c, 19          // Streaming SIMD Extensions 4.1
#define CPU_FEATURE_SSE4_2       1, 0, c, 20          // Streaming SIMD Extensions 4.2
#define CPU_FEATURE_X2APIC       1, 0, c, 21          // x2APIC
#define CPU_FEATURE_MOVBE        1, 0, c, 22          // MOVBE instruction
#define CPU_FEATURE_POPCNT       1, 0, c, 23          // POPCNT instruction
#define CPU_FEATURE_TSC_DEADLINE 1, 0, c, 24          // Local APIC supports one-shot operation using a TSC deadline value
#define CPU_FEATURE_XSAVE        1, 0, c, 26          // XSAVE
#define CPU_FEATURE_OSXSAVE      1, 0, c, 27          // XSAVE and Processor Extended States
#define CPU_FEATURE_AVX          1, 0, c, 28          // Advanced Vector Extensions
#define CPU_FEATURE_RDRAND       1, 0, c, 30          // RDRAND instruction
#define CPU_FEATURE_FPU          1, 0, d, 0           // Floating-point unit on-chip
#define CPU_FEATURE_TSC          1, 0, d, 4           // Time Stamp Counter
#define CPU_FEATURE_MSR          1, 0, d, 5           // Model Specific Registers
#define CPU_FEATURE_PAE          1, 0, d, 6           // Physical Address Extension
#define CPU_FEATURE_APIC         1, 0, d, 9           // APIC on-chip
#define CPU_FEATURE_SEP          1, 0, d, 11          // SYSENTER and SYSEXIT instructions
#define CPU_FEATURE_PGE          1, 0, d, 13          // Page Global Enable
#define CPU_FEATURE_MMX          1, 0, d, 23          // MMX technology
#define CPU_FEATURE_FXSR         1, 0, d, 24          // FXSAVE and FXSTOR instructions
#define CPU_FEATURE_AVX2         7, 0, b, 5           // Advanced Vector Extensions 2
#define CPU_FEATURE_FSGSBASE     7, 0, b, 0           // RDFSBASE, RDGSBASE, WRFSBASE, WRGSBASE
#define CPU_FEATURE_LA57         7, 0, c, 16          // 5-Level Paging
#define CPU_FEATURE_XSAVES       0xd, 1, a, 3         // XSAVES, XSTORS, and IA32_XSS
#define CPU_FEATURE_NX           0x80000001, 0, d, 20 // No-Execute Bit
#define CPU_FEATURE_PDPE1GB      0x80000001, 0, d, 26 // GB pages

#define x86_cpu_get_feature_impl(leaf, subleaf, reg, bit) (per_cpu(platform_info->cpu)->cpuinfo.cpuid[X86_CPUID_LEAF_ENUM(leaf, subleaf, reg, )] & (1 << bit))

#define cpu_has_feature(feat) x86_cpu_get_feature_impl(feat)

#define FOR_ALL_CPU_FEATURES(M)                                                                                                                                          \
    M(SSE3)                                                                                                                                                              \
    M(SSSE3)                                                                                                                                                             \
    M(SSE4_1)                                                                                                                                                            \
    M(SSE4_2)                                                                                                                                                            \
    M(X2APIC)                                                                                                                                                            \
    M(MOVBE)                                                                                                                                                             \
    M(POPCNT)                                                                                                                                                            \
    M(TSC_DEADLINE)                                                                                                                                                      \
    M(XSAVE)                                                                                                                                                             \
    M(OSXSAVE)                                                                                                                                                           \
    M(AVX)                                                                                                                                                               \
    M(RDRAND)                                                                                                                                                            \
    M(FPU)                                                                                                                                                               \
    M(TSC)                                                                                                                                                               \
    M(MSR)                                                                                                                                                               \
    M(PAE)                                                                                                                                                               \
    M(APIC)                                                                                                                                                              \
    M(SEP)                                                                                                                                                               \
    M(PGE)                                                                                                                                                               \
    M(MMX)                                                                                                                                                               \
    M(FXSR)                                                                                                                                                              \
    M(LA57)                                                                                                                                                              \
    M(AVX2)                                                                                                                                                              \
    M(FSGSBASE)                                                                                                                                                          \
    M(XSAVES)                                                                                                                                                            \
    M(NX)                                                                                                                                                                \
    M(PDPE1GB)

#define FOR_ALL_SUPPORTED_CPUID_LEAF(M)                                                                                                                                  \
    M(1, 0, c)                                                                                                                                                           \
    M(1, 0, d)                                                                                                                                                           \
    M(7, 0, b)                                                                                                                                                           \
    M(7, 0, c)                                                                                                                                                           \
    M(0xd, 1, a)                                                                                                                                                         \
    M(0x80000001, 0, d)

#define X86_CPUID_LEAF_ENUM(leaf, subleaf, reg, ...) X86_CPUID_##leaf##_##subleaf##_##reg

enum
{
#define _do_define_enum(leaf, subleaf, reg) X86_CPUID_LEAF_ENUM(leaf, subleaf, reg, _unused_),
    FOR_ALL_SUPPORTED_CPUID_LEAF(_do_define_enum)
#undef _do_define_enum
    _X86_CPUID_COUNT,
};

typedef reg32_t x86_cpuid_array[_X86_CPUID_COUNT];
