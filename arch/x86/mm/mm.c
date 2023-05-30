// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"
#include "mos/mm/physical/pmm.h"
#include "mos/x86/x86_platform.h"

#include <mos/kconfig.h>
#include <mos/printk.h>
#include <mos/x86/mm/mm.h>
#include <stdlib.h>
#include <string.h>

static __nodiscard s64 do_align(u64 *pstart, u64 *psize, pm_range_type_t type)
{
    const u64 start_addr = *pstart;
    const u64 size = *psize;
    const u64 end_addr = start_addr + size;

    const u64 up_start = ALIGN_UP_TO_PAGE(start_addr);
    const u64 up_end = ALIGN_UP_TO_PAGE(end_addr);
    const u64 down_start = ALIGN_DOWN_TO_PAGE(start_addr);
    const u64 down_end = ALIGN_DOWN_TO_PAGE(end_addr);

    u64 new_start = start_addr, new_size = size;

    switch (type)
    {
        case PM_RANGE_FREE:
            // it's fine to shrink a free region
            if (start_addr != up_start || end_addr != down_end)
            {
                new_start = up_start;
                new_size = down_end - up_start;
            }
            break;
        case PM_RANGE_RESERVED:
            // we can't shrink a reserved region, so just make it larger, inflate it to the nearest page boundary
            if (start_addr != down_start || end_addr != up_end)
            {
                new_start = down_start;
                new_size = up_end - down_start;
            }
            break;
        case PM_RANGE_UNINITIALIZED:
        case PM_RANGE_ALLOCATED: mos_panic("invalid range type: %d", type);
    }

    *pstart = new_start;
    *psize = new_size;

    return new_size - size;
}

void x86_pmm_region_setup(const multiboot_memory_map_t *map_entry, u32 count, ptr_t initrd_paddr, size_t initrd_size)
{
    struct range
    {
        u64 start, size;
        bool usable;
    } regions[count];

    u64 max_end = 0;
    for (u32 i = 0; i < count; i++)
    {
        const multiboot_memory_map_t *entry = map_entry + i;

        u64 region_length = entry->len;
        const u64 region_base = entry->phys_addr;

        if (region_base > MOS_MAX_VADDR)
        {
            pr_warn("ignoring a 0x%llx (+ %llu bytes) high memory region", region_base, region_base + region_length - 1);
            continue;
        }

        if (region_base + region_length > (u64) MOS_MAX_VADDR + 1)
        {
            pr_warn("truncating memory region at 0x%llx, it extends beyond the maximum address " PTR_FMT, region_base, MOS_MAX_VADDR);
            region_length = (u64) MOS_MAX_VADDR + 1 - region_base;
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

        pr_info2("  %d: " PTR_RANGE64 " %-10s", i, region_base, region_base + region_length, type_str);

        struct range *r = &regions[i];
        r->start = region_base;
        r->size = region_length;
        r->usable = entry->type == MULTIBOOT_MEMORY_AVAILABLE;

        const s64 loss = do_align(&r->start, &r->size, r->usable ? PM_RANGE_FREE : PM_RANGE_RESERVED);
        if (loss)
            pr_info2("     aligned to " PTR_RANGE64 ", %s %llu bytes", r->start, r->start + r->size, loss > 0 ? "gained" : "lost", llabs(loss));

        max_end = MAX(max_end, r->start + r->size);
    }

    MOS_ASSERT_X(max_end % MOS_PAGE_SIZE == 0, "max_end is not page aligned");

    const size_t phyframes_count = max_end / MOS_PAGE_SIZE;
    pr_info("the system has %zu frames in total", phyframes_count);

    const size_t array_memsize = phyframes_count * sizeof(phyframe_t);
    mos_debug(pmm, "%zu bytes required for the phyframes array", array_memsize);

    struct range *rphyframes = NULL; // the region that will hold the phyframes array

    // now we need to find contiguous memory for the phyframes array
    for (u32 i = 0; i < count; i++)
    {
        struct range *r = &regions[i];

        if (!r->usable)
            continue;

        if (r->size < array_memsize)
            continue;

        ptr_t phyaddr = r->start;
        phyaddr = MAX((ptr_t) __MOS_KERNEL_END - MOS_KERNEL_START_VADDR, phyaddr); // don't use the kernel's memory
        phyaddr = MAX(ALIGN_UP_TO_PAGE(initrd_paddr + initrd_size), phyaddr);      // don't use the initrd's memory

        rphyframes = r;

        pr_info2("using " PTR_RANGE " for the phyframes array", phyaddr, phyaddr + array_memsize - 1);
        mm_early_map_kernel_pages(MOS_PHYFRAME_ARRAY_VADDR, phyaddr, array_memsize / MOS_PAGE_SIZE, VM_RW | VM_GLOBAL);
        memzero((void *) MOS_PHYFRAME_ARRAY_VADDR, array_memsize); // zero the array

        // the region with phyframes, and the rest of the region
        if (phyaddr > r->start)
        {
            // before the phyframes array
            pmm_register_phyframes(r->start, (phyaddr - r->start) / MOS_PAGE_SIZE, PM_RANGE_FREE);
        }

        // the phyframes array
        pmm_register_phyframes(phyaddr, array_memsize / MOS_PAGE_SIZE, PM_RANGE_RESERVED);

        if (phyaddr + array_memsize < r->start + r->size)
        {
            // after the phyframes array
            pmm_register_phyframes(phyaddr + array_memsize, (r->size - array_memsize) / MOS_PAGE_SIZE, PM_RANGE_FREE);
        }
        break;
    }

    MOS_ASSERT_X(rphyframes, "failed to find a region for the phyframes array");

    // add all the other regions
    for (u32 i = 0; i < count; i++)
    {
        struct range *r = &regions[i];
        if (r == rphyframes) // we already added this region
            continue;
        if (r->size == 0)
            continue;
        pmm_register_phyframes(r->start, r->size / MOS_PAGE_SIZE, r->usable ? PM_RANGE_FREE : PM_RANGE_RESERVED);
    }
}
