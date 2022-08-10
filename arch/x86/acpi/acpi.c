// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/acpi/acpi.h"

#include "lib/string.h"
#include "mos/printk.h"

#define EBDA_START 0x00080000
#define EBDA_END   0x0009ffff
#define BIOS_START 0x000f0000
#define BIOS_END   0x000fffff

#define ACPI_RSDP_SIGNATURE "RSD PTR "

void x86_acpi_init()
{
    acpi_rsdp_t *rsdp = find_acpi_rsdp(EBDA_START, EBDA_END);
    if (!rsdp)
    {
        rsdp = find_acpi_rsdp(BIOS_START, BIOS_END);
        if (!rsdp)
            mos_panic("RSDP not found");
    }

    // !! "MUST" USE XSDT IF FOUND !!
    acpi_sdt_header_t *rsdt = (acpi_sdt_header_t *) rsdp->v1.rsdt_addr;
    if (!verify_sdt_checksum(rsdt))
        mos_panic("RSDT checksum error");
}

bool verify_sdt_checksum(acpi_sdt_header_t *tableHeader)
{
    u8 sum = 0;
    for (u32 i = 0; i < tableHeader->length; i++)
        sum += ((char *) tableHeader)[i];
    return sum == 0;
}

acpi_rsdp_t *find_acpi_rsdp(uintptr_t start, uintptr_t end)
{
    for (u64 addr = start; addr < end; addr += 0x10)
    {
        if (strncmp((const char *) addr, "RSD PTR ", 8) == 0)
        {
            pr_info2("ACPI: RSDP magic at %p", (void *) addr);
            acpi_rsdp_t *rsdp = (acpi_rsdp_t *) ((uintptr_t) addr - offsetof(acpi_rsdp_t, v1.signature));

            // check the checksum
            u8 sum = 0;
            for (u32 i = 0; i < sizeof(acpi_rsdp_v1_t); i++)
                sum += ((u8 *) rsdp)[i];

            if (sum != 0)
            {
                pr_info2("ACPI: RSDP checksum failed");
                continue;
            }
            pr_info2("ACPI: RSDP checksum ok");
            pr_info("ACPI: oem: '%s', revision: %d", rsdp->v1.oem_id, rsdp->v1.revision);

            if (rsdp->v1.revision != 0)
                mos_panic("ACPI: RSDP revision %d not supported", rsdp->v1.revision);

            return rsdp;
        }
    }
    return NULL;
}
