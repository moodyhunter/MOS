// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/physical/pmm.h>
#include <mos/printk.h>
#include <mos/x86/mm/mm.h>
#include <stdlib.h>

static __nodiscard u64 do_align(u64 *pstart, u64 *psize, pm_range_type_t type)
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

    const u64 alignment_loss = new_size > size ? new_size - size : size - new_size;
    return alignment_loss;
}

void x86_pmm_region_setup(const multiboot_memory_map_t *map_entry, u32 count)
{
    struct range
    {
        u64 start, size;
        bool usable;
    } regions[count];

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

        const u64 loss = do_align(&r->start, &r->size, r->usable ? PM_RANGE_FREE : PM_RANGE_RESERVED);
        if (loss)
            pr_info2("     aligned to " PTR_RANGE64 ", lost %llu bytes", r->start, r->start + r->size, loss);
    }

    // finally add the regions into the PMM
    for (u32 i = 0; i < count; i++)
    {
        struct range *r = &regions[i];
        pmm_add_region_frames(r->start, r->size / MOS_PAGE_SIZE, r->usable ? PM_RANGE_FREE : PM_RANGE_RESERVED);
    }
}
