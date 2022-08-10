// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    char signature[8];
    u8 checksum;
    char oem_id[6];
    u8 revision;
    u32 rsdt_addr;
} __packed acpi_rsdp_v1_t;
static_assert(sizeof(acpi_rsdp_v1_t) == 20, "acpi_rsdp_v1_t is not 20 bytes");

typedef struct
{
    acpi_rsdp_v1_t v1;
    u32 length;
    u64 xsdt_addr;
    u8 checksum;
    u8 reserved[3];
} __packed acpi_rsdp_v2_t;

typedef acpi_rsdp_v2_t acpi_rsdp_t;

typedef struct
{
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
} acpi_sdt_header_t;

void x86_acpi_init();
acpi_rsdp_t *find_acpi_rsdp(uintptr_t start, uintptr_t end);
bool verify_sdt_checksum(acpi_sdt_header_t *tableHeader);
