// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "mos/platform/platform.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/x86_platform.h"
#include "multiboot.h"

#include <stdlib.h>
#include <string.h>

static __nodiscard s64 do_align(u64 *pstart, size_t *psize, bool reserved)
{
    const u64 start_addr = *pstart;
    const u64 size = *psize;
    const u64 end_addr = start_addr + size;

    const u64 up_start = ALIGN_UP_TO_PAGE(start_addr);
    const u64 up_end = ALIGN_UP_TO_PAGE(end_addr);
    const u64 down_start = ALIGN_DOWN_TO_PAGE(start_addr);
    const u64 down_end = ALIGN_DOWN_TO_PAGE(end_addr);

    u64 new_start = start_addr, new_size = size;

    if (reserved)
    {
        // we can't shrink a reserved region, so just make it larger, inflate it to the nearest page boundary
        if (start_addr != down_start || end_addr != up_end)
        {
            new_start = down_start;
            new_size = up_end - down_start;
        }
    }
    else
    {
        // it's fine to shrink a free region
        if (start_addr != up_start || end_addr != down_end)
        {
            new_start = up_start;
            new_size = down_end - up_start;
        }
    }

    *pstart = new_start;
    *psize = new_size;

    return new_size - size;
}

static void mb_pmm_region_setup(const multiboot_memory_map_t *mb_maps, u32 count)
{
    platform_info->max_pfn = 0;
    u64 last_end_pfn = 0;
    for (size_t i = 0; i < count; i++)
    {
        const multiboot_memory_map_t *mbentry = mb_maps + i;

        size_t original_length = mbentry->len;
        const u64 original_base = mbentry->phys_addr;

        if (original_base > MOS_MAX_VADDR)
        {
            pr_warn("ignoring a 0x%llx (+ %llu bytes) high memory region", original_base, original_base + original_length - 1);
            continue;
        }

        if (original_base + original_length > (u64) MOS_MAX_VADDR + 1)
        {
            pr_warn("truncating memory region at 0x%llx, it extends beyond the maximum address " PTR_FMT, original_base, MOS_MAX_VADDR);
            original_length = (u64) MOS_MAX_VADDR + 1 - original_base;
        }

        const char *type_str = "<unknown>";
        switch (mbentry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "available"; break;
            case MULTIBOOT_MEMORY_RESERVED: type_str = "reserved"; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: type_str = "reclaimable"; break;
            case MULTIBOOT_MEMORY_NVS: type_str = "non-volatile"; break;
            case MULTIBOOT_MEMORY_BADRAM: type_str = "bad"; break;
            default: mos_panic("unsupported memory map type: %x", mbentry->type);
        }

        size_t aligned_length = original_length;
        u64 aligned_base = original_base;

        const s64 loss = do_align(&aligned_base, &aligned_length, mbentry->type != MULTIBOOT_MEMORY_AVAILABLE);

        const pfn_t region_pfn_start = aligned_base / MOS_PAGE_SIZE;
        const pfn_t region_pfn_end = region_pfn_start + aligned_length / MOS_PAGE_SIZE;

        if (last_end_pfn != aligned_base / MOS_PAGE_SIZE)
        {
            // there's a gap between the last region and this one
            // we fake a reserved region to fill the gap
            pr_info2("  %zu: gap of %zu pages", platform_info->num_pmm_regions, (size_t) (region_pfn_start - last_end_pfn));
            pmm_region_t *rgap = &platform_info->pmm_regions[platform_info->num_pmm_regions++];
            rgap->reserved = true;
            rgap->nframes = region_pfn_start - last_end_pfn;
            rgap->pfn_start = last_end_pfn;
            rgap->type = MULTIBOOT_MEMORY_RESERVED;
        }

        pr_info2("  %zu: " PTR_RANGE64 " %-10s", platform_info->num_pmm_regions, original_base, original_base + original_length, type_str);
        if (loss)
            pr_info2("     " PTR_RANGE64 " (aligned), pfn " PFN_RANGE ", %s %llu bytes", aligned_base, aligned_base + aligned_length, region_pfn_start, region_pfn_end,
                     loss > 0 ? "gained" : "lost", llabs(loss));

        if (aligned_length == 0)
        {
            pr_info2("     aligned to " PFN_RANGE ", region is empty", region_pfn_start, region_pfn_end);
            continue;
        }

        pmm_region_t *r = &platform_info->pmm_regions[platform_info->num_pmm_regions++];
        r->reserved = mbentry->type != MULTIBOOT_MEMORY_AVAILABLE;
        r->pfn_start = region_pfn_start;
        r->nframes = region_pfn_end - region_pfn_start;
        r->type = mbentry->type;

        last_end_pfn = region_pfn_end;
        platform_info->max_pfn = MAX(platform_info->max_pfn, r->pfn_start + r->nframes);
    }
}

void x86_multiboot_entry(mos_x86_multiboot_startup_info *info)
{
    console_register(&com1_console.con, MOS_PAGE_SIZE);
    const multiboot_info_t *mb_info = info->mb_info;

    if (mb_info->flags & MULTIBOOT_INFO_MODS && mb_info->mods_count != 0)
    {
        const multiboot_module_t *mod = (const multiboot_module_t *) mb_info->mods_addr;
        platform_info->initrd_npages = ALIGN_UP_TO_PAGE(mod->mod_end - mod->mod_start) / MOS_PAGE_SIZE;
        platform_info->initrd_pfn = ALIGN_DOWN_TO_PAGE(mod->mod_start) / MOS_PAGE_SIZE;
    }

    mos_cmdline_init(mb_info->cmdline);
    mb_pmm_region_setup(mb_info->mmap_addr, mb_info->mmap_length / sizeof(multiboot_memory_map_t));
    x86_start_kernel();
}
