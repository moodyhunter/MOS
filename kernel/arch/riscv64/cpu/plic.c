// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/riscv64/cpu/plic.h"

#include "mos/mm/mm.h"

#include <mos/types.h>

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC                 pa_va(0x0c000000L)
#define PLIC_PRIORITY        (PLIC + 0x0)
#define PLIC_PENDING         (PLIC + 0x1000)
#define PLIC_MENABLE(hart)   (PLIC + 0x2000 + (hart) * 0x100)
#define PLIC_SENABLE(hart)   (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart) * 0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_MCLAIM(hart)    (PLIC + 0x200004 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart)    (PLIC + 0x201004 + (hart) * 0x2000)

#define PLIC_IRQPRIO(irq) (PLIC + irq * 4)

u32 plic_claim_irq()
{
    return *(u32 *) PLIC_SCLAIM(0);
}

void plic_complete(u32 irq)
{
    *(u32 *) PLIC_SCLAIM(0) = irq;
}

void plic_enable_irq(u32 irq)
{
    *(u32 *) PLIC_IRQPRIO(irq) = 1;      // set priority to a non-zero value
    *(u32 *) PLIC_SENABLE(0) = 1 << irq; // enable the irq
    *(u32 *) PLIC_SPRIORITY(0) = 0;
}
