// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mm.h"

#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/x86/acpi/acpi.h>
#include <mos/x86/acpi/madt.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

const acpi_rsdt_t *x86_acpi_rsdt;
const acpi_hpet_t *x86_acpi_hpet;
const acpi_fadt_t *x86_acpi_fadt;
ptr_t x86_acpi_dsdt = 0;

SYSFS_AUTOREGISTER(acpi, NULL);

typedef struct
{
    sysfs_item_t item;
    ptr_t vaddr;
    size_t size;
} acpi_sysfs_item_t;

static bool acpi_sysfs_show(sysfs_file_t *f)
{
    acpi_sysfs_item_t *const item = sysfs_file_get_data(f);
    return sysfs_put_data(f, (void *) item->vaddr, item->size) > 0;
}

static void register_sysfs_acpi_node(const char table_name[4], const acpi_sdt_header_t *header)
{
    acpi_sysfs_item_t *const item = kzalloc(sizeof(acpi_sysfs_item_t));
    item->vaddr = (ptr_t) header;
    item->size = header->length;
    item->item.name = strndup(table_name, 4);
    item->item.show = acpi_sysfs_show;
    item->item.type = SYSFS_RO;

    sysfs_register_file(&__sysfs_acpi, &item->item, item);
}

should_inline bool verify_sdt_checksum(const acpi_sdt_header_t *tableHeader)
{
    u8 sum = 0;
    for (u32 i = 0; i < tableHeader->length; i++)
        sum += ((char *) tableHeader)[i];
    return sum == 0;
}

#define do_verify_checksum(var, header, type)                                                                                                                            \
    var = container_of(header, type, sdt_header);                                                                                                                        \
    if (!verify_sdt_checksum(&(var)->sdt_header))                                                                                                                        \
        mos_panic(#type " checksum error");

void acpi_parse_rsdt(acpi_rsdp_t *rsdp)
{
    mos_debug(x86_acpi, "initializing ACPI with RSDP at %p", (void *) rsdp);
    // !! "MUST" USE XSDT IF FOUND !!
    if (rsdp->xsdt_addr)
        mos_panic("XSDT not supported");

    const acpi_sdt_header_t *rsdt_header = (acpi_sdt_header_t *) pa_va(rsdp->v1.rsdt_addr);
    do_verify_checksum(x86_acpi_rsdt, rsdt_header, acpi_rsdt_t);

    if (strncmp(x86_acpi_rsdt->sdt_header.signature, "RSDT", 4) != 0)
        mos_panic("RSDT signature mismatch");

    const size_t num_headers = (x86_acpi_rsdt->sdt_header.length - sizeof(acpi_sdt_header_t)) / sizeof(ptr32_t);
    for (size_t i = 0; i < num_headers; i++)
    {
        const acpi_sdt_header_t *const header = (acpi_sdt_header_t *) pa_va(x86_acpi_rsdt->sdts[i]);
        register_sysfs_acpi_node(header->signature, header);
        mos_debug(x86_acpi, "%.4s at %p, size %u", header->signature, (void *) header, header->length);

        if (strncmp(header->signature, ACPI_SIGNATURE_FADT, 4) == 0)
        {
            do_verify_checksum(x86_acpi_fadt, header, acpi_fadt_t);

            const acpi_sdt_header_t *const dsdt = (acpi_sdt_header_t *) pa_va(x86_acpi_fadt->dsdt);
            if (!verify_sdt_checksum(dsdt))
                mos_panic("DSDT checksum error");
            mos_debug(x86_acpi, "DSDT at %p, size %u", (void *) dsdt, dsdt->length);
            x86_acpi_dsdt = (ptr_t) dsdt;
            register_sysfs_acpi_node("DSDT", dsdt);
        }
        else if (strncmp(header->signature, ACPI_SIGNATURE_MADT, 4) == 0)
        {
            do_verify_checksum(x86_acpi_madt, header, acpi_madt_t);
        }
        else if (strncmp(header->signature, ACPI_SIGNATURE_HPET, 4) == 0)
        {
            MOS_WARNING_PUSH
            MOS_WARNING_DISABLE("-Waddress-of-packed-member")
            do_verify_checksum(x86_acpi_hpet, header, acpi_hpet_t);
            MOS_WARNING_POP
        }
        else
        {
#if MOS_DEBUG_FEATURE(x86_acpi)
            pr_warn("acpi: unknown %.4s", header->signature);
#endif
        }
    }
}

acpi_rsdp_t *acpi_find_rsdp(ptr_t start, size_t size)
{
    for (ptr_t addr = start; addr < start + size; addr += 0x10)
    {
        if (strncmp((const char *) addr, ACPI_SIGNATURE_RSDP, 8) == 0)
        {
            mos_debug(x86_acpi, "ACPI: RSDP magic at %p", (void *) addr);
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
            mos_debug(x86_acpi, "ACPI: oem: '%s', revision: %d", rsdp->v1.oem_id, rsdp->v1.revision);

            if (rsdp->v1.revision != 0)
                mos_panic("ACPI: RSDP revision %d not supported", rsdp->v1.revision);

            return rsdp;
        }
    }
    return NULL;
}
