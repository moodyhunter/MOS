// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.hpp>
#include <mos_string.hpp>

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
    ".quad 1b\n\t" \
    ".quad %0\n\t" \
    ".quad %1\n\t" \
    ".quad %2\n\t"
// clang-format on

#define MOS_PLATFORM_DEBUG_MODULES(X)                                                                                                                                    \
    X(riscv64_startup)                                                                                                                                                   \
    /**/

#define MOS_PLATFORM_MEMORY_BARRIER() __asm__ __volatile__("fence.i" ::: "memory")

struct platform_regs_t : mos::NamedType<"Platform.Registers">
{
    platform_regs_t()
    {
        memzero(this, sizeof(*this));
    }

    platform_regs_t(const platform_regs_t *regs)
    {
        *this = *regs;
    }

    reg_t ra, sp, gp, tp;
    reg_t t0, t1, t2;
    reg_t fp, s1;
    reg_t a0, a1, a2, a3, a4, a5, a6, a7;
    reg_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    reg_t t3, t4, t5, t6;

    // below are the CSR registers
    reg_t sstatus, sepc;
};

typedef struct _platform_process_options
{
    int __unused;
} platform_process_options_t;

typedef struct _platform_thread_options
{
    reg64_t f[32]; // f0-f31
    reg32_t fcsr;  // fcsr
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
