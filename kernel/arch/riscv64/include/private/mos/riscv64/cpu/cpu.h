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
} __packed platform_regs_t;

MOS_STATIC_ASSERT(sizeof(platform_regs_t) == 248, "Invalid platform_regs_t size");

#define read_csr(reg)       statement_expr(reg_t, { asm volatile("csrr %0, " #reg : "=r"(retval)); })
#define write_csr(reg, val) __asm__ volatile("csrw " #reg ", %0" ::"r"(val))
