// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

#define MOS_PLATFORM_PAGING_LEVELS 4

#define MOS_PLATFORM_HAS_FDT 1

#define PML2_HUGE_CAPABLE 1
#define PML3_HUGE_CAPABLE 1
#define PML4_HUGE_CAPABLE 1

#define PML1_SHIFT 12
#define PML2_SHIFT 21
#define PML3_SHIFT 30
#define PML4_SHIFT 39

#define PML1_MASK 0x1FFULL
#define PML2_MASK 0x1FFULL
#define PML3_MASK 0x1FFULL
#define PML4_MASK 0x1FFULL

#define PML1_ENTRIES 512
#define PML2_ENTRIES 512
#define PML3_ENTRIES 512
#define PML4_ENTRIES 512

#define MOS_USER_END_VADDR     0x00007FFFFFFFFFFF
#define MOS_KERNEL_START_VADDR 0xFFFF800000000000

#define MOS_ELF_PLATFORM EM_RISCV

#define MOS_PLATFORM_PANIC_INSTR "unimp"

// clang-format off
#define MOS_PLATFORM_PANIC_POINT_ASM \
    ".quad 1b\n\t"  \
    ".quad %0\n\t" \
    ".quad %1\n\t" \
    ".quad %2\n\t"
// clang-format on

#define MOS_PLATFORM_DEBUG_MODULES(X)                                                                                                                                    \
    X(riscv64_startup)                                                                                                                                                   \
    /**/

typedef struct _platform_process_options
{
    int __unused;
} platform_process_options_t;

typedef struct _platform_thread_options
{
    int __unused;
} platform_thread_options_t;

typedef struct _platform_cpuinfo
{
    int __unused;
} platform_cpuinfo_t;

typedef struct _platform_arch_info
{
    void *fdt; ///< pointer to the device tree
    ptr_t rsdp_addr;
    u32 rsdp_revision;
} platform_arch_info_t;
