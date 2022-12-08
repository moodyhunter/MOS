// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/acpi/acpi.h"

#include "lib/string.h"
#include "mos/boot/startup.h"
#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/x86_interrupt.h"
#include "mos/x86/x86_platform.h"

acpi_rsdt_t *x86_acpi_rsdt;
acpi_madt_t *x86_acpi_madt;
acpi_hpet_t *x86_acpi_hpet;
acpi_fadt_t *x86_acpi_fadt;

should_inline bool verify_sdt_checksum(acpi_sdt_header_t *tableHeader)
{
    u8 sum = 0;
    for (u32 i = 0; i < tableHeader->length; i++)
        sum += ((char *) tableHeader)[i];
    return sum == 0;
}

void x86_acpi_init()
{
    acpi_rsdp_t *rsdp = acpi_find_rsdp(X86_EBDA_MEMREGION_PADDR | BIOS_VADDR_MASK, EBDA_MEMREGION_SIZE);
    if (!rsdp)
    {
        rsdp = acpi_find_rsdp(X86_BIOS_MEMREGION_PADDR | BIOS_VADDR_MASK, BIOS_MEMREGION_SIZE);
        if (!rsdp)
            mos_panic("RSDP not found");
    }

    // !! "MUST" USE XSDT IF FOUND !!
    if (rsdp->xsdt_addr)
        mos_panic("XSDT not supported");

    x86_acpi_rsdt = container_of(BIOS_VADDR(rsdp->v1.rsdt_addr), acpi_rsdt_t, sdt_header);
    if (!verify_sdt_checksum(&x86_acpi_rsdt->sdt_header))
        mos_panic("RSDT checksum error");

    MOS_ASSERT(strncmp(x86_acpi_rsdt->sdt_header.signature, "RSDT", 4) == 0);

    const size_t count = (x86_acpi_rsdt->sdt_header.length - sizeof(acpi_sdt_header_t)) / sizeof(u32);
    for (size_t i = 0; i < count; i++)
    {
        acpi_sdt_header_t *addr = (acpi_sdt_header_t *) BIOS_VADDR((uintptr_t) x86_acpi_rsdt->sdts[i]);
        pr_info2("acpi: RSDT entry %zu: %.4s", i, addr->signature);

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
            x86_acpi_hpet = container_of(addr, acpi_hpet_t, header);
            MOS_WARNING_PUSH
            MOS_WARNING_DISABLE("-Waddress-of-packed-member")
            if (!verify_sdt_checksum(&x86_acpi_hpet->header))
                mos_panic("HPET checksum error");
            MOS_WARNING_POP
        }
        else
        {
            pr_info2("acpi: unknown entry %.4s", addr->signature);
        }
    }

    if (!x86_acpi_madt)
        mos_panic("MADT not found");

    madt_entry_foreach(entry, x86_acpi_madt)
    {
        switch (entry->type)
        {
            case 0:
            {
                acpi_madt_et0_lapic_t *lapic = container_of(entry, acpi_madt_et0_lapic_t, header);
                pr_info2("acpi: MADT entry LAPIC [%p]", (void *) lapic);
                break;
            }
            case 1:
            {
                acpi_madt_et1_ioapic_t *ioapic = container_of(entry, acpi_madt_et1_ioapic_t, header);
                pr_info2("acpi: MADT entry IOAPIC [%p]", (void *) ioapic);
                break;
            }
            case 2:
            {
                acpi_madt_et2_ioapic_override_t *int_override = container_of(entry, acpi_madt_et2_ioapic_override_t, header);
                pr_info2("acpi: MADT entry IOAPIC override [%p]", (void *) int_override);
                break;
            }
            case 3:
            {
                acpi_madt_et3_ioapic_nmi_t *int_override = container_of(entry, acpi_madt_et3_ioapic_nmi_t, header);
                pr_info2("acpi: MADT entry IOAPIC NMI [%p]", (void *) int_override);
                break;
            }
            case 4:
            {
                acpi_madt_et4_lapic_nmi_t *nmi = container_of(entry, acpi_madt_et4_lapic_nmi_t, header);
                pr_info2("acpi: MADT entry LAPIC NMI [%p]", (void *) nmi);
                break;
            }
            case 5:
            {
                acpi_madt_et5_lapic_addr_t *local_apic_nmi = container_of(entry, acpi_madt_et5_lapic_addr_t, header);
                pr_info2("acpi: MADT entry LAPIC address override [%p]", (void *) local_apic_nmi);
                break;
            }
            case 9:
            {
                acpi_madt_et9_lx2apic_t *local_sapic_override = container_of(entry, acpi_madt_et9_lx2apic_t, header);
                pr_info2("acpi: MADT entry local x2 SAPIC override [%p]", (void *) local_sapic_override);
                break;
            }
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
