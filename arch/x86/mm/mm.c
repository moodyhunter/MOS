// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "mos/boot/startup.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/physical/pmm.h"
#include "mos/x86/x86_platform.h"

#include <mos/kconfig.h>
#include <mos/printk.h>
#include <stdlib.h>
#include <string.h>

pfn_t phyframes_pfn = 0;
size_t phyframes_npages = 0;

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

void x86_pmm_region_setup(const multiboot_memory_map_t *map_entry, u32 count, pfn_t initrd_pfn, size_t initrd_npages)
{
    pmm_region_t regions[count * 2]; // ensure we have enough space for all regions
    size_t region_count = 0;

    u64 max_end_pfn = 0;
    u64 last_end_pfn = 0;
    for (size_t i = 0; i < count; i++)
    {
        const multiboot_memory_map_t *entry = map_entry + i;

        size_t original_length = entry->len;
        const u64 original_base = entry->phys_addr;

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
        switch (entry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "available"; break;
            case MULTIBOOT_MEMORY_RESERVED: type_str = "reserved"; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: type_str = "reclaimable"; break;
            case MULTIBOOT_MEMORY_NVS: type_str = "non-volatile"; break;
            case MULTIBOOT_MEMORY_BADRAM: type_str = "bad"; break;
            default: mos_panic("unsupported memory map type: %x", entry->type);
        }

        size_t aligned_length = original_length;
        u64 aligned_base = original_base;

        const s64 loss = do_align(&aligned_base, &aligned_length, entry->type != MULTIBOOT_MEMORY_AVAILABLE);

        const pfn_t region_pfn_start = aligned_base / MOS_PAGE_SIZE;
        const pfn_t region_pfn_end = region_pfn_start + aligned_length / MOS_PAGE_SIZE;

        if (last_end_pfn != aligned_base / MOS_PAGE_SIZE)
        {
            // there's a gap between the last region and this one
            // we fake a reserved region to fill the gap
            pmm_region_t *rgap = &regions[region_count];
            rgap->reserved = true;
            rgap->nframes = region_pfn_start - last_end_pfn;
            rgap->pfn_start = last_end_pfn;
            pr_info2("  %zu: gap of %zu pages", region_count, rgap->nframes);
            region_count++;
        }

        pr_info2("  %zu: " PTR_RANGE64 " %-10s", region_count, original_base, original_base + original_length, type_str);
        if (loss)
            pr_info2("     " PTR_RANGE64 " (aligned), pfn " PFN_RANGE ", %s %llu bytes", aligned_base, aligned_base + aligned_length, region_pfn_start, region_pfn_end,
                     loss > 0 ? "gained" : "lost", llabs(loss));

        if (aligned_length == 0)
        {
            pr_info2("     aligned to " PFN_RANGE ", region is empty", region_pfn_start, region_pfn_end);
            continue;
        }

        pmm_region_t *r = &regions[region_count];
        r->reserved = entry->type != MULTIBOOT_MEMORY_AVAILABLE;
        r->pfn_start = region_pfn_start;
        r->nframes = region_pfn_end - region_pfn_start;
        region_count++;

        last_end_pfn = region_pfn_end;
        max_end_pfn = MAX(max_end_pfn, r->pfn_start + r->nframes);
    }

    MOS_ASSERT_X(max_end_pfn % MOS_PAGE_SIZE == 0, "max_end is not page aligned");

    const size_t phyframes_count = max_end_pfn;

    phyframes_npages = ALIGN_UP_TO_PAGE(phyframes_count * sizeof(phyframe_t)) / MOS_PAGE_SIZE;
    mos_debug(pmm, "%zu pages required for the phyframes array", phyframes_npages);

    pmm_region_t *phyframes_region = NULL; // the region that will hold the phyframes array

    // now we need to find contiguous memory for the phyframes array
    for (u32 i = 0; i < region_count; i++)
    {
        pmm_region_t *r = &regions[i];

        if (r->reserved)
            continue;

        if (r->nframes < phyframes_npages)
            continue;

        phyframes_region = r;

        phyframes_pfn = r->pfn_start;
        phyframes_pfn = MAX(MOS_KERNEL_PFN(ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_END)), phyframes_pfn); // don't use the kernel's memory
        phyframes_pfn = MAX(ALIGN_UP_TO_PAGE(initrd_pfn + initrd_npages), phyframes_pfn);               // don't use the initrd's memory

        const pfn_t pfn_end = phyframes_pfn + phyframes_npages;

        pr_info2("using " PFN_RANGE " for the phyframes array", phyframes_pfn, pfn_end);

        // we have to map this into the startup page tables, because we don't have the kernel page tables yet
        mos_startup_map_pages(MOS_PHYFRAME_ARRAY_VADDR, phyframes_pfn << 12, phyframes_npages, VM_RW | VM_GLOBAL);
        // then we zero the array
        memzero((void *) MOS_PHYFRAME_ARRAY_VADDR, phyframes_npages * MOS_PAGE_SIZE);
        // then we can initialize the pmm
        pmm_init(phyframes_count);
        // and finally we can reserve this region
        pmm_reserve_frames(phyframes_pfn, phyframes_npages);
        break;
    }

    MOS_ASSERT_X(phyframes_region, "failed to find a region for the phyframes array");

    // add all the other regions
    for (u32 i = 0; i < region_count; i++)
    {
        pmm_region_t *r = &regions[i];
        if (r == phyframes_region)
            continue;
        if (r->nframes == 0) // ???
            continue;
        if (r->reserved)
            pmm_register_reserved_region(r->pfn_start, r->nframes);
    }
}
