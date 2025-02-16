// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/table_ops.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/x86/acpi/acpi_types.hpp"

#include <algorithm>
#include <mos/mos_global.h>
#include <mos/syslog/printk.hpp>
#include <mos/types.hpp>
#include <mos/x86/acpi/acpi.hpp>
#include <mos/x86/acpi/madt.hpp>
#include <mos/x86/cpu/cpu.hpp>
#include <mos/x86/devices/port.hpp>
#include <mos/x86/x86_interrupt.hpp>
#include <mos/x86/x86_platform.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>
#include <stddef.h>

ptr_t x86_acpi_dsdt = 0;

static sysfs_item_t acpi_sysfs_items[] = {
    {},
};

SYSFS_AUTOREGISTER(acpi, acpi_sysfs_items);

#define do_verify_checksum(var, header, type)                                                                                                                            \
    var = container_of(header, type, sdt_header);                                                                                                                        \
    if (!verify_sdt_checksum(&(var)->sdt_header))                                                                                                                        \
        mos_panic(#type " checksum error");

struct acpi_sysfs_item_t : mos::NamedType<"X86.ACPI.SysfsItem">
{
    sysfs_item_t item;
    size_t size;
    phyframe_t *pages;

    acpi_sysfs_item_t(const char *name) : item(mos::string(name, 4))
    {
    }
};

static bool acpi_sysfs_mmap(sysfs_file_t *f, vmap_t *vmap, off_t offset)
{
    acpi_sysfs_item_t *const item = container_of(sysfs_file_get_item(f), acpi_sysfs_item_t, item);
    const ssize_t item_npages = ALIGN_UP_TO_PAGE(item->size) / MOS_PAGE_SIZE;
    if (offset >= item_npages)
        return false;

    const size_t npages = std::min((ssize_t) vmap->npages, item_npages - offset);                      // limit to the number of pages in the item
    mm_do_map(vmap->mmctx->pgd, vmap->vaddr, phyframe_pfn(item->pages), npages, vmap->vmflags, false); // no need to refcount
    return true;
}

static bool acpi_sysfs_munmap(sysfs_file_t *f, vmap_t *vmap, bool *unmapped)
{
    MOS_UNUSED(f);
    mm_do_unmap(vmap->mmctx->pgd, vmap->vaddr, vmap->npages, false);
    *unmapped = true;
    return true;
}

static void register_sysfs_acpi_rsdp(const acpi_rsdp_t *rsdp)
{
    acpi_sysfs_item_t *const item = mos::create<acpi_sysfs_item_t>("RSDP");
    item->size = rsdp->length;
    item->item.mem.mmap = acpi_sysfs_mmap;
    item->item.mem.munmap = acpi_sysfs_munmap;
    item->item.mem.size = item->size;
    item->item.type = SYSFS_MEM;
    item->pages = mm_get_free_pages(ALIGN_UP_TO_PAGE(item->size) / MOS_PAGE_SIZE);
    if (!item->pages)
        mos_panic("failed to allocate pages for ACPI table");

    memcpy((void *) phyframe_va(item->pages), rsdp, rsdp->length);
    pmm_ref(item->pages, true); // don't free the pages when the item is freed

    sysfs_register_file(&__sysfs_acpi, &item->item);
}

static void register_sysfs_acpi_node(const char table_name[4], const acpi_sdt_header_t *header)
{
    acpi_sysfs_item_t *const item = mos::create<acpi_sysfs_item_t>(table_name);
    item->size = header->length;
    item->item.mem.mmap = acpi_sysfs_mmap;
    item->item.mem.munmap = acpi_sysfs_munmap;
    item->item.mem.size = item->size;
    item->item.type = SYSFS_MEM;
    item->pages = mm_get_free_pages(ALIGN_UP_TO_PAGE(item->size) / MOS_PAGE_SIZE);
    if (!item->pages)
        mos_panic("failed to allocate pages for ACPI table");

    memcpy((void *) phyframe_va(item->pages), header, header->length);
    pmm_ref(item->pages, true); // don't free the pages when the item is freed

    sysfs_register_file(&__sysfs_acpi, &item->item);
}

should_inline bool verify_sdt_checksum(const acpi_sdt_header_t *tableHeader)
{
    u8 sum = 0;
    for (u32 i = 0; i < tableHeader->length; i++)
        sum += ((char *) tableHeader)[i];
    return sum == 0;
}

static void do_handle_sdt_header(const acpi_sdt_header_t *const header)
{
    register_sysfs_acpi_node(header->signature, header);
    pr_dinfo2(x86_acpi, "%.4s at %p, size %u", header->signature, (void *) header, header->length);

    if (strncmp(header->signature, ACPI_SIGNATURE_FADT, 4) == 0)
    {
        const acpi_fadt_t *x86_acpi_fadt;
        do_verify_checksum(x86_acpi_fadt, header, acpi_fadt_t);

        const acpi_sdt_header_t *const dsdt = (acpi_sdt_header_t *) pa_va(x86_acpi_fadt->dsdt);
        if (!verify_sdt_checksum(dsdt))
            mos_panic("DSDT checksum error");
        pr_dinfo2(x86_acpi, "DSDT at %p, size %u", (void *) dsdt, dsdt->length);
        x86_acpi_dsdt = (ptr_t) dsdt;
        register_sysfs_acpi_node("DSDT", dsdt);
    }
    else if (strncmp(header->signature, ACPI_SIGNATURE_MADT, 4) == 0)
    {
        do_verify_checksum(x86_acpi_madt, header, acpi_madt_t);
    }
}

static void do_iterate_sdts(const acpi_rsdp_t *rsdp)
{
    if (rsdp->v1.revision == 0)
    {
        const acpi_sdt_header_t *rsdt_header = (acpi_sdt_header_t *) pa_va(rsdp->v1.rsdt_addr);
        if (strncmp(rsdt_header->signature, "RSDT", 4) != 0)
            mos_panic("RSDT signature mismatch");

        const acpi_rsdt_t *x86_acpi_rsdt;
        do_verify_checksum(x86_acpi_rsdt, rsdt_header, acpi_rsdt_t);

        const size_t num_headers = (x86_acpi_rsdt->sdt_header.length - sizeof(acpi_sdt_header_t)) / sizeof(ptr32_t);
        for (size_t i = 0; i < num_headers; i++)
        {
            const acpi_sdt_header_t *const header = (acpi_sdt_header_t *) pa_va(x86_acpi_rsdt->sdts[i]);
            do_handle_sdt_header(header);
        }
    }
    else if (rsdp->v1.revision == 2)
    {
        const acpi_sdt_header_t *xsdt_header = (acpi_sdt_header_t *) pa_va(rsdp->xsdt_addr);
        if (strncmp(xsdt_header->signature, "XSDT", 4) != 0)
            mos_panic("XSDT signature mismatch");

        MOS_ASSERT_X(verify_sdt_checksum(xsdt_header), "acpi_xsdt_t checksum error");

        const acpi_xsdt_t *x86_acpi_xsdt = container_of(xsdt_header, acpi_xsdt_t, sdt_header);

        const size_t num_headers = (x86_acpi_xsdt->sdt_header.length - sizeof(acpi_sdt_header_t)) / sizeof(ptr64_t);
        for (size_t i = 0; i < num_headers; i++)
        {
            const acpi_sdt_header_t *const header = (acpi_sdt_header_t *) pa_va(x86_acpi_xsdt->sdts[i]);
            do_handle_sdt_header(header);
        }
    }
    else
    {
        mos_panic("ACPI: RSDP revision %d not supported", rsdp->v1.revision);
    }
}

void acpi_parse_rsdt(const acpi_rsdp_t *rsdp)
{
    pr_dinfo2(x86_acpi, "initializing ACPI with RSDP at %p", (void *) rsdp);
    register_sysfs_acpi_rsdp(rsdp);
    do_iterate_sdts(rsdp);
}

const acpi_rsdp_t *acpi_find_rsdp(ptr_t start, size_t size)
{
    for (ptr_t addr = start; addr < start + size; addr += 0x10)
    {
        if (strncmp((const char *) addr, ACPI_SIGNATURE_RSDP, 8) == 0)
        {
            pr_dinfo2(x86_acpi, "ACPI: RSDP magic at %p", (void *) addr);
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
            pr_dinfo2(x86_acpi, "ACPI: oem: '%.6s', revision: %d", rsdp->v1.oem_id, rsdp->v1.revision);

            if (rsdp->v1.revision != 0 && rsdp->v1.revision != 2)
                mos_panic("ACPI: RSDP revision %d not supported", rsdp->v1.revision);

            return rsdp;
        }
    }
    return NULL;
}
