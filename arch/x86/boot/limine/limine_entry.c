// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "limine: " fmt

#include "limine.h"
#include "mos/device/console.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/x86/devices/serial_console.h"
#include "mos/x86/x86_platform.h"

static volatile struct limine_memmap_request memmap_request = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0 };
static volatile struct limine_kernel_address_request kernel_address_request = { .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0 };
static volatile struct limine_module_request module_request = { .id = LIMINE_MODULE_REQUEST, .revision = 0 };

asmlinkage void limine_entry(void)
{
    console_register(&com1_console.con, MOS_PAGE_SIZE);

    if (memmap_request.response == NULL)
        mos_panic("No memory map found"); // are we able to panic at this early stage?

    struct limine_memmap_response *memmap_response = memmap_request.response;
    for (size_t i = 0; i < memmap_response->entry_count; i++)
    {
        const struct limine_memmap_entry *entry = memmap_response->entries[i];
        const char *typestr = NULL;
        switch (entry->type)
        {
            case LIMINE_MEMMAP_USABLE: typestr = "usable"; break;
            case LIMINE_MEMMAP_RESERVED: typestr = "reserved"; break;
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE: typestr = "ACPI reclaimable"; break;
            case LIMINE_MEMMAP_ACPI_NVS: typestr = "ACPI NVS"; break;
            case LIMINE_MEMMAP_BAD_MEMORY: typestr = "bad memory"; break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: typestr = "bootloader reclaimable"; break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES: typestr = "kernel and modules"; break;
            case LIMINE_MEMMAP_FRAMEBUFFER: typestr = "framebuffer"; break;
        }

        pr_info2("%30s: " PTR_RANGE " (%zu pages)", typestr, entry->base, entry->base + entry->length - 1, entry->length / MOS_PAGE_SIZE);

        pmm_region_t *region = &platform_info->pmm_regions[platform_info->num_pmm_regions++];
        region->pfn_start = entry->base / MOS_PAGE_SIZE;
        region->reserved = entry->type != LIMINE_MEMMAP_USABLE;
        region->nframes = entry->length / MOS_PAGE_SIZE;
        region->type = entry->type;
    }

    x86_start_kernel();
}
