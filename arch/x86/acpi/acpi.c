// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/acpi/acpi.h"

#include "lib/string.h"
#include "mos/boot/startup.h"
#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/x86/acpi/madt.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/x86_interrupt.h"
#include "mos/x86/x86_platform.h"

const acpi_rsdt_t *x86_acpi_rsdt;
const acpi_hpet_t *x86_acpi_hpet;
const acpi_fadt_t *x86_acpi_fadt;

should_inline bool verify_sdt_checksum(const acpi_sdt_header_t *tableHeader)
{
    u8 sum = 0;
    for (u32 i = 0; i < tableHeader->length; i++)
        sum += ((char *) tableHeader)[i];
    return sum == 0;
}

void acpi_parse_rsdt(acpi_rsdp_t *rsdp)
{
    pr_info("Initializing ACPI with RSDP at %p", (void *) rsdp);
    // !! "MUST" USE XSDT IF FOUND !!
    if (rsdp->xsdt_addr)
        mos_panic("XSDT not supported");

    x86_acpi_rsdt = container_of(BIOS_VADDR(rsdp->v1.rsdt_addr), acpi_rsdt_t, sdt_header);
    if (!verify_sdt_checksum(&x86_acpi_rsdt->sdt_header))
        mos_panic("RSDT checksum error");

    if (strncmp(x86_acpi_rsdt->sdt_header.signature, "RSDT", 4) != 0)
        mos_panic("RSDT signature mismatch");

    const size_t num_headers = (x86_acpi_rsdt->sdt_header.length - sizeof(acpi_sdt_header_t)) / sizeof(u32); // TODO: uintptr_t?
    for (size_t i = 0; i < num_headers; i++)
    {
        const acpi_sdt_header_t *const header = BIOS_VADDR_TYPE(x86_acpi_rsdt->sdts[i], acpi_sdt_header_t *);

        if (strncmp(header->signature, ACPI_SIGNATURE_FADT, 4) == 0)
        {
            x86_acpi_fadt = container_of(header, acpi_fadt_t, sdt_header);
            if (!verify_sdt_checksum(&x86_acpi_fadt->sdt_header))
                mos_panic("FADT checksum error");
            pr_info2("acpi: FADT at %p", (void *) x86_acpi_fadt);
        }
        else if (strncmp(header->signature, ACPI_SIGNATURE_MADT, 4) == 0)
        {
            x86_acpi_madt = container_of(header, acpi_madt_t, sdt_header);
            if (!verify_sdt_checksum(&x86_acpi_madt->sdt_header))
                mos_panic("MADT checksum error");
            pr_info2("acpi: MADT at %p", (void *) x86_acpi_madt);
        }
        else if (strncmp(header->signature, ACPI_SIGNATURE_HPET, 4) == 0)
        {
            x86_acpi_hpet = container_of(header, acpi_hpet_t, header);
            MOS_WARNING_PUSH
            MOS_WARNING_DISABLE("-Waddress-of-packed-member")
            if (!verify_sdt_checksum(&x86_acpi_hpet->header))
                mos_panic("HPET checksum error");
            MOS_WARNING_POP
            pr_info2("acpi: HPET at %p", (void *) x86_acpi_hpet);
        }
        else
        {
            pr_warn("acpi: unknown entry %.4s", header->signature);
        }
    }
}

acpi_rsdp_t *acpi_find_rsdp(uintptr_t start, size_t size)
{
    for (uintptr_t addr = start; addr < start + size; addr += 0x10)
    {
        if (strncmp((const char *) addr, ACPI_SIGNATURE_RSDP, 8) == 0)
        {
            pr_info2("ACPI: RSDP magic at %p", (void *) addr);
            acpi_rsdp_t *rsdp = (acpi_rsdp_t *) addr;

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
