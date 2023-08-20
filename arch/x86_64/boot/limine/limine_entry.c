// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "limine: " fmt

#include "limine.h"
#include "mos/cmdline.h"
#include "mos/device/console.h"
#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/setup.h"
#include "mos/x86/devices/serial_console.h"
#include "mos/x86/x86_platform.h"

#include <mos/mos_global.h>
#include <stdlib.h>

static volatile struct limine_memmap_request memmap_request = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0 };
static volatile struct limine_kernel_address_request kernel_address_request = { .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0 };
static volatile struct limine_module_request module_request = { .id = LIMINE_MODULE_REQUEST, .revision = 0 };
static volatile struct limine_hhdm_request hhdm_request = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };
static volatile struct limine_kernel_file_request kernel_file_request = { .id = LIMINE_KERNEL_FILE_REQUEST, .revision = 0 };

static void add_to_memmap(pfn_t start, size_t npages, bool reserved, u32 type, const char *typestr)
{
    if (start + npages < 1 MB / MOS_PAGE_SIZE)
    {
        type = LIMINE_MEMMAP_RESERVED;
        reserved = true;
    }

    pmm_region_t *entry = &platform_info->pmm_regions[platform_info->num_pmm_regions++];
    entry->reserved = reserved;
    entry->nframes = npages;
    entry->pfn_start = start;
    entry->type = type;
    pr_info2("%25s: " PFNADDR_RANGE " (%zu pages)", typestr, PFNADDR(entry->pfn_start, entry->pfn_start + entry->nframes), entry->nframes);

    if (!entry->reserved)
        platform_info->max_pfn = MAX(platform_info->max_pfn, entry->pfn_start + entry->nframes);
}

asmlinkage void limine_entry(void)
{
    console_register(&com1_console.con, MOS_PAGE_SIZE);

    if (kernel_file_request.response == NULL)
        mos_panic("No kernel file found");

    mos_cmdline_init(kernel_file_request.response->kernel_file->cmdline);
    setup_invoke_earlysetup();

    if (hhdm_request.response == NULL)
        mos_panic("No HHDM found");

    platform_info->direct_map_base = hhdm_request.response->offset;
    mos_debug(x86_startup, "Direct map base: " PTR_FMT, platform_info->direct_map_base);

    if (memmap_request.response == NULL)
        mos_panic("No memory map found"); // are we able to panic at this early stage?

    pfn_t last_end_pfn = 0;

    struct limine_memmap_response *memmap_response = memmap_request.response;
    for (size_t i = 0; i < memmap_response->entry_count; i++)
    {
        const struct limine_memmap_entry *entry = memmap_response->entries[i];

        const pfn_t start_pfn = entry->base / MOS_PAGE_SIZE;
        const size_t npages = entry->length / MOS_PAGE_SIZE;

        // there's a gap between the last region and this one
        // we fake a reserved region to fill the gap
        if (last_end_pfn != start_pfn)
            add_to_memmap(last_end_pfn, start_pfn - last_end_pfn, true, LIMINE_MEMMAP_RESERVED, "<hole>");
        last_end_pfn = start_pfn + npages;

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

        add_to_memmap(start_pfn, npages, entry->type != LIMINE_MEMMAP_USABLE, entry->type, typestr);
    }

    if (module_request.response == NULL)
        mos_panic("No modules found");

    struct limine_module_response *module_response = module_request.response;
    if (module_response->module_count != 1)
        mos_panic("Expected exactly one module, got %zu", module_response->module_count);

    struct limine_file *module = module_response->modules[0];
    mos_debug(x86_startup, "initrd: %s, " PTR_RANGE, module->path, (ptr_t) module->address, (ptr_t) module->address + module->size);
    platform_info->initrd_pfn = va_pfn(module->address);
    platform_info->initrd_npages = ALIGN_UP_TO_PAGE(module->size) / MOS_PAGE_SIZE;
    mos_debug(x86_startup, "initrd at " PFN_FMT ", size %zu pages", platform_info->initrd_pfn, platform_info->initrd_npages);

    if (kernel_address_request.response == NULL)
        mos_panic("No kernel address found");

    struct limine_kernel_address_response *kernel_address_response = kernel_address_request.response;
    platform_info->k_basepfn = kernel_address_response->physical_base / MOS_PAGE_SIZE;
    platform_info->k_basevaddr = kernel_address_response->virtual_base;

    mos_start_kernel();
}
