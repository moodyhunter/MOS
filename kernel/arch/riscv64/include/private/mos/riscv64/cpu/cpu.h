// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.h>

typedef struct _platform_regs
{
    reg_t ra, sp, gp, tp;
    reg_t t0, t1, t2;
    reg_t fp, s1;
    reg_t a0, a1, a2, a3, a4, a5, a6, a7;
    reg_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    reg_t t3, t4, t5, t6;

    // below are the CSR registers
    reg_t sstatus, sepc;
} __packed platform_regs_t;

MOS_STATIC_ASSERT(sizeof(platform_regs_t) == 264, "please also change cpu/interrupt.S");

#define read_csr(reg)       statement_expr(reg_t, { asm volatile("csrr %0, " #reg : "=r"(retval)); })
#define write_csr(reg, val) __asm__ volatile("csrw " #reg ", %0" ::"r"(val))

#define make_satp(mode, asid, ppn) ((u64) (mode) << 60 | ((u64) (asid) << 44) | (ppn))

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9
#define SATP_MODE_SV57 10

#define SSTATUS_SIE  BIT(1)  // Supervisor Interrupt Enable
#define SSTATUS_SPIE BIT(5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_SPP  BIT(8)  // Supervisor Previous Privilege
#define SSTATUS_SUM  BIT(18) // Supervisor User Memory Access Enable

#define SSTATUS_FS_OFF     0
#define SSTATUS_FS_INITIAL BIT(13)
#define SSTATUS_FS_CLEAN   BIT(14)
#define SSTATUS_FS_DIRTY   (BIT(13) | BIT(14))

#define SIE_SEIE BIT(9)
#define SIE_STIE BIT(5)
#define SIE_SSIE BIT(1)

extern noreturn void riscv64_trap_exit(platform_regs_t *regs);

extern const char __riscv64_trap_entry[];
extern const char __riscv64_usermode_trap_entry[];
