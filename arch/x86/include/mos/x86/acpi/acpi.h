// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"
#include "mos/x86/acpi/acpi_types.h"

extern acpi_rsdt_t *x86_acpi_rsdt;
extern acpi_hpet_t *x86_acpi_hpet;
extern acpi_fadt_t *x86_acpi_fadt;

void acpi_parse_rsdt(acpi_rsdp_t *rsdp);
