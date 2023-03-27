// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/x86/acpi/acpi_types.h>

extern const acpi_madt_t *x86_acpi_madt;
extern uintptr_t x86_ioapic_address;
extern u32 x86_cpu_lapic[MOS_MAX_CPU_COUNT];

u32 x86_ioapic_get_irq_override(u32 irq);
void madt_parse_table(void);
