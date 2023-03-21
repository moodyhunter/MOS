// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/physical/pmm.h>
#include <mos/printk.h>
#include <mos/x86/mm/mm.h>
#include <stdlib.h>

void x86_pmm_region_setup(const multiboot_memory_map_t *map_entry, u32 count)
{
    pr_info("Multiboot memory map:");
    for (u32 i = 0; i < count; i++)
    {
        char size_buf[32];
        const multiboot_memory_map_t *entry = map_entry + i;

        const u64 region_length = entry->len;
        const u64 region_base = entry->phys_addr;

        format_size(size_buf, sizeof(size_buf), region_length);

        if (region_base > MOS_MAX_VADDR)
        {
            pr_warn("ignoring a %s high memory region starting at 0x%llx", size_buf, region_base);
            continue;
        }

        if (region_base + region_length > (u64) MOS_MAX_VADDR + 1)
        {
            pr_warn("truncating memory region at 0x%llx, it extends beyond the maximum address " PTR_FMT, region_base, MOS_MAX_VADDR);
            continue;
        }

        char *type_str = "";
        switch (entry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "Available"; break;
            case MULTIBOOT_MEMORY_RESERVED: type_str = "Reserved"; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: type_str = "ACPI Reclaimable"; break;
            case MULTIBOOT_MEMORY_NVS: type_str = "NVS"; break;
            case MULTIBOOT_MEMORY_BADRAM: type_str = "Bad RAM"; break;
            default: mos_panic("unsupported memory map type: %x", entry->type);
        }

        pr_info2("  %d: 0x%.8llx - 0x%.8llx: %-10s (%s)", i, region_base, region_base + region_length - 1, type_str, size_buf);
        pmm_add_region_bytes(region_base, region_length, entry->type == MULTIBOOT_MEMORY_AVAILABLE ? PM_RANGE_FREE : PM_RANGE_RESERVED);
    }
}
