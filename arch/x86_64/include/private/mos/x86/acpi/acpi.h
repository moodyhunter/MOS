// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#include <mos/x86/acpi/acpi_types.h>

const acpi_rsdp_t *acpi_find_rsdp(ptr_t start, size_t size);
void acpi_parse_rsdt(const acpi_rsdp_t *rsdp);
