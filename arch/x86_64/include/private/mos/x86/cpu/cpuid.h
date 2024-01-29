// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/cpu/cpu.h"

#include <mos/mos_global.h>
#include <mos/types.h>

//!! this feature list must start at line 11 for correct counting (see below...)
#define CPU_FEATURE_FPU          1, 0, d, 0           // Floating-point unit on-chip
#define CPU_FEATURE_VME          1, 0, d, 1           // Virtual 8086 mode extensions
#define CPU_FEATURE_DE           1, 0, d, 2           // Debugging extensions
#define CPU_FEATURE_PSE          1, 0, d, 3           // Page Size Extension
#define CPU_FEATURE_TSC          1, 0, d, 4           // Time Stamp Counter
#define CPU_FEATURE_MSR          1, 0, d, 5           // Model Specific Registers
#define CPU_FEATURE_PAE          1, 0, d, 6           // Physical Address Extension
#define CPU_FEATURE_MCE          1, 0, d, 7           // Machine Check Exception
#define CPU_FEATURE_CX8          1, 0, d, 8           // CMPXCHG8 instruction
#define CPU_FEATURE_APIC         1, 0, d, 9           // APIC on-chip
#define CPU_FEATURE_SEP          1, 0, d, 11          // SYSENTER and SYSEXIT instructions
#define CPU_FEATURE_MTRR         1, 0, d, 12          // Memory Type Range Registers
#define CPU_FEATURE_PGE          1, 0, d, 13          // Page Global Enable
#define CPU_FEATURE_MCA          1, 0, d, 14          // Machine Check Architecture
#define CPU_FEATURE_CMOV         1, 0, d, 15          // Conditional move and FCMOV instructions
#define CPU_FEATURE_PAT          1, 0, d, 16          // Page Attribute Table
#define CPU_FEATURE_PSE36        1, 0, d, 17          // 36-bit page size extension
#define CPU_FEATURE_PSN          1, 0, d, 18          // Processor Serial Number
#define CPU_FEATURE_CLFSH        1, 0, d, 19          // CLFLUSH instruction
#define CPU_FEATURE_DS           1, 0, d, 21          // Debug store
#define CPU_FEATURE_ACPI         1, 0, d, 22          // Thermal controls MSR for ACPI
#define CPU_FEATURE_MMX          1, 0, d, 23          // MMX technology
#define CPU_FEATURE_FXSR         1, 0, d, 24          // FXSAVE and FXSTOR instructions
#define CPU_FEATURE_SSE          1, 0, d, 25          // Streaming SIMD Extensions
#define CPU_FEATURE_SSE2         1, 0, d, 26          // Streaming SIMD Extensions 2
#define CPU_FEATURE_SS           1, 0, d, 27          // Self Snoop
#define CPU_FEATURE_HTT          1, 0, d, 28          // Multi-Threading
#define CPU_FEATURE_TM1          1, 0, d, 29          // Thermal Monitor 1
#define CPU_FEATURE_IA64         1, 0, d, 30          // IA64 processor emulating x86
#define CPU_FEATURE_PBE          1, 0, d, 31          // Pending Break Enable
#define CPU_FEATURE_SSE3         1, 0, c, 0           // Streaming SIMD Extensions 3
#define CPU_FEATURE_SSSE3        1, 0, c, 9           // Supplemental Streaming SIMD Extensions 3
#define CPU_FEATURE_PCID         1, 0, c, 17          // Process-Context Identifiers
#define CPU_FEATURE_DCA          1, 0, c, 18          // Direct Cache Access
#define CPU_FEATURE_SSE4_1       1, 0, c, 19          // Streaming SIMD Extensions 4.1
#define CPU_FEATURE_SSE4_2       1, 0, c, 20          // Streaming SIMD Extensions 4.2
#define CPU_FEATURE_X2APIC       1, 0, c, 21          // x2APIC
#define CPU_FEATURE_MOVBE        1, 0, c, 22          // MOVBE instruction
#define CPU_FEATURE_POPCNT       1, 0, c, 23          // POPCNT instruction
#define CPU_FEATURE_TSC_DEADLINE 1, 0, c, 24          // Local APIC supports one-shot operation using a TSC deadline value
#define CPU_FEATURE_AES_NI       1, 0, c, 25          // AESNI instruction extensions
#define CPU_FEATURE_XSAVE        1, 0, c, 26          // XSAVE
#define CPU_FEATURE_OSXSAVE      1, 0, c, 27          // XSAVE and Processor Extended States
#define CPU_FEATURE_AVX          1, 0, c, 28          // Advanced Vector Extensions
#define CPU_FEATURE_F16C         1, 0, c, 29          // 16-bit floating-point conversion instructions
#define CPU_FEATURE_RDRAND       1, 0, c, 30          // RDRAND instruction
#define CPU_FEATURE_HYPERVISOR   1, 0, c, 31          // Running on a hypervisor
#define CPU_FEATURE_AVX2         7, 0, b, 5           // Advanced Vector Extensions 2
#define CPU_FEATURE_FSGSBASE     7, 0, b, 0           // RDFSBASE, RDGSBASE, WRFSBASE, WRGSBASE
#define CPU_FEATURE_LA57         7, 0, c, 16          // 5-Level Paging
#define CPU_FEATURE_XSAVES       0xd, 1, a, 3         // XSAVES, XSTORS, and IA32_XSS
#define CPU_FEATURE_NX           0x80000001, 0, d, 20 // No-Execute Bit
#define CPU_FEATURE_PDPE1GB      0x80000001, 0, d, 26 // GB pages

// clang-format off
#define FOR_ALL_CPU_FEATURES(M) \
    M(FPU)      M(VME)      M(DE)       M(PSE)  M(TSC)      M(MSR)      M(PAE)          M(MCE)      M(CX8)      M(APIC)         \
    M(SEP)      M(MTRR)     M(PGE)      M(MCA)  M(CMOV)     M(PAT)      M(PSE36)        M(PSN)      M(CLFSH)    M(DS)           \
    M(ACPI)     M(MMX)      M(FXSR)     M(SSE)  M(SSE2)     M(SS)       M(HTT)          M(TM1)      M(IA64)     M(PBE)          \
    M(SSE3)     M(SSSE3)    M(PCID)     M(DCA)  M(SSE4_1)   M(SSE4_2)   M(X2APIC)       M(MOVBE)    M(POPCNT)   M(TSC_DEADLINE) \
    M(AES_NI)   M(XSAVE)    M(OSXSAVE)  M(AVX)  M(F16C)     M(RDRAND)   M(HYPERVISOR)   M(AVX2)     M(FSGSBASE) M(LA57)         \
    M(XSAVES)   M(NX)       M(PDPE1GB)
// clang-format on

#define _do_count(leaf) __COUNTER__,
MOS_STATIC_ASSERT(sizeof((int[]){ FOR_ALL_CPU_FEATURES(_do_count) }) / sizeof(int) == __LINE__ - 23, "FOR_ALL_CPU_FEATURES is incomplete");
#undef _do_count

#define x86_cpu_get_feature_impl(leaf, subleaf, reg, bit) (per_cpu(platform_info->cpu)->cpuinfo.cpuid[X86_CPUID_LEAF_ENUM(leaf, subleaf, reg, )] & (1 << bit))

#define cpu_has_feature(feat) x86_cpu_get_feature_impl(feat)

#define FOR_ALL_SUPPORTED_CPUID_LEAF(M)                                                                                                                                  \
    M(1, 0, d)                                                                                                                                                           \
    M(1, 0, c)                                                                                                                                                           \
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

#define XCR0_X87          BIT(0)
#define XCR0_SSE          BIT(1)
#define XCR0_AVX          BIT(2)
#define XCR0_BNDREGS      BIT(3)
#define XCR0_BNDCSR       BIT(4)
#define XCR0_OPMASK       BIT(5)
#define XCR0_ZMM_Hi256    BIT(6)
#define XCR0_Hi16_ZMM     BIT(7)
#define XCR0_PT           BIT(8)
#define XCR0_PKRU         BIT(9)
#define XCR0_PASID        BIT(10)
#define XCR0_CET_U        BIT(11)
#define XCR0_CET_S        BIT(12)
#define XCR0_HDC          BIT(13)
#define XCR0_UINTR        BIT(14)
#define XCR0_LBR          BIT(15)
#define XCR0_HMP          BIT(16)
#define XCR0_AMX_TILECFG  BIT(17)
#define XCR0_AMX_TILEDATA BIT(18)
#define XCR0_APX_EXGPRS   BIT(19)
