// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/acpi/acpi.h"

#include "lib/containers.h"
#include "lib/string.h"
#include "mos/printk.h"

#define EBDA_START 0x00080000
#define EBDA_END   0x0009ffff
#define BIOS_START 0x000f0000
#define BIOS_END   0x000fffff

acpi_rsdt_t *x86_acpi_rsdt;
acpi_madt_t *x86_acpi_madt;
acpi_hpet_t *x86_acpi_hpet;
acpi_fadt_t *x86_acpi_fadt;

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

    x86_acpi_rsdt = container_of(rsdp->v1.rsdt_addr, acpi_rsdt_t, sdt_header);
    if (!verify_sdt_checksum(&x86_acpi_rsdt->sdt_header))
        mos_panic("RSDT checksum error");

    MOS_ASSERT(strncmp(x86_acpi_rsdt->sdt_header.signature, "RSDT", 4) == 0);

    const size_t count = (x86_acpi_rsdt->sdt_header.length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
    for (size_t i = 0; i < count; i++)
    {
        acpi_sdt_header_t *addr = x86_acpi_rsdt->sdts[i];
        pr_info2("acpi: RSDT entry %d: %.4s", i, addr->signature);

        if (strncmp(addr->signature, ACPI_SIGNATURE_FADT, 4) == 0)
        {
            x86_acpi_fadt = container_of(addr, acpi_fadt_t, sdt_header);
            if (!verify_sdt_checksum(&x86_acpi_fadt->sdt_header))
                mos_panic("FADT checksum error");
        }
        else if (strncmp(addr->signature, ACPI_SIGNATURE_MADT, 4) == 0)
        {
            x86_acpi_madt = container_of(addr, acpi_madt_t, sdt_header);
            if (!verify_sdt_checksum(&x86_acpi_madt->sdt_header))
                mos_panic("MADT checksum error");
        }
        else if (strncmp(addr->signature, ACPI_SIGNATURE_HPET, 4) == 0)
        {
            x86_acpi_hpet = container_of(addr, acpi_hpet_t, sdt_header);
            if (!verify_sdt_checksum((void *) &x86_acpi_hpet->sdt_header))
                mos_panic("HPET checksum error");
        }
        else
        {
            pr_info2("acpi: unknown entry %.4s", addr->signature);
        }
    }

    if (!x86_acpi_madt)
        mos_panic("MADT not found");
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
    for (uintptr_t addr = start; addr < end; addr += 0x10)
    {
        if (strncmp((const char *) addr, ACPI_SIGNATURE_RSDP, 8) == 0)
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
