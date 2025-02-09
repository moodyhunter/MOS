// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/x86/acpi/acpi_types.hpp>

extern const acpi_madt_t *x86_acpi_madt;
extern ptr_t x86_ioapic_phyaddr;

u32 x86_ioapic_get_irq_override(u32 irq);
void madt_parse_table(void);
