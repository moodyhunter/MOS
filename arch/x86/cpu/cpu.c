// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/cpu/cpu.h"

#include "mos/x86/interrupt/apic.h"

u32 x86_cpu_get_id(void)
{
    return apic_reg_read_offset_32(APIC_REG_LAPIC_ID);
}
