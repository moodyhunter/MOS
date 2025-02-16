// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpuid.hpp"

#include <mos/types.hpp>

#pragma once

#define MOS_PLATFORM_PAGING_LEVELS 4 // PML4, PDPT, PD, PT
#define MOS_USER_END_VADDR         0x00007FFFFFFFFFFF
#define MOS_KERNEL_START_VADDR     0xFFFF800000000000

#define MOS_PLATFORM_HAS_FDT 0

#define PML1_SHIFT   12
#define PML1_MASK    0x1FFL // 9 bits page table offset
#define PML1_ENTRIES 512

#define PML2_SHIFT        21
#define PML2_MASK         0x1FFL // 9 bits page directory offset
#define PML2_ENTRIES      512
#define PML2_HUGE_CAPABLE 1 // 2MB pages

#define PML3_SHIFT        30
#define PML3_MASK         0x1FFL // 9 bits page directory pointer offset
#define PML3_ENTRIES      512
#define PML3_HUGE_CAPABLE 1 // 1GB pages

#define PML4_SHIFT        39
#define PML4_MASK         0x1FFL // 9 bits page map level 4 offset
#define PML4_ENTRIES      512
#define PML4_HUGE_CAPABLE -1

#define MOS_ELF_PLATFORM EM_X86_64

#define MOS_PLATFORM_PANIC_INSTR "ud2"

// clang-format off
#define MOS_PLATFORM_PANIC_POINT_ASM \
    ".quad 1b\n\t"  \
    ".quad %c0\n\t" \
    ".quad %c1\n\t" \
    ".quad %c2\n\t"
// clang-format on

// clang-format off
#define MOS_PLATFORM_DEBUG_MODULES(X) \
    X(x86_cpu)      \
    X(x86_lapic)    \
    X(x86_ioapic)   \
    X(x86_startup)  \
    X(x86_acpi)
// clang-format on

#define MOS_PLATFORM_MEMORY_BARRIER() __asm__ __volatile__("" ::: "memory")

typedef struct _platform_process_options
{
    bool iopl;
} platform_process_options_t;

typedef struct _platform_thread_options
{
    ptr_t fs_base, gs_base;
    u8 *xsaveptr;
} platform_thread_options_t;

typedef struct _platform_cpuinfo
{
    x86_cpuid_array cpuid;
} platform_cpuinfo_t;

typedef struct _platform_arch_info
{
    ptr_t rsdp_addr;
    u32 rsdp_revision;
} platform_arch_info_t;
