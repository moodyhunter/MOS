// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "lib/stdlib.h"
#include "mos/printk.h"
#include "mos/x86/x86_platform.h"

static void mem_add_region(u64 phys_addr, size_t size, bool available)
{
    if (x86_platform.mem_regions.count >= MOS_MAX_SUPPORTED_MEMREGION)
        mos_panic("too many memory regions added.");

    memregion_t *block = &x86_platform.mem_regions.regions[x86_platform.mem_regions.count++];
    block->address = phys_addr;
    block->size_bytes = size;
    block->available = available;

    if (available)
        x86_platform.mem.available += size;
    x86_platform.mem.available += size;
}

void x86_mem_init(const multiboot_memory_map_t *map_entry, u32 count)
{
    x86_platform.mem_regions.count = 0;
    x86_platform.mem.total = 0;
    x86_platform.mem.available = 0;

    pr_info("Multiboot memory map:");
    for (u32 i = 0; i < count; i++)
    {
        char size_buf[32];
        const multiboot_memory_map_t *entry = map_entry + i;

        u64 region_length = entry->len;
        u64 region_base = entry->phys_addr;

        format_size(size_buf, sizeof(size_buf), region_length);

        if (region_base > MOS_MAX_VADDR)
        {
            pr_warn("ignoring a %s long memory region starting at 0x%llx", size_buf, region_base);
            continue;
        }

        if (region_base + region_length > (u64) MOS_MAX_VADDR + 1)
        {
            region_length = MOS_MAX_VADDR - region_base;
            format_size(size_buf, sizeof(size_buf), region_length);
            pr_warn("truncating memory region at 0x%llx, it extends beyond the maximum address " PTR_FMT, region_base, MOS_MAX_VADDR);
        }

        mem_add_region(region_base, region_length, entry->type == MULTIBOOT_MEMORY_AVAILABLE);

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
    }

#define SIZE_BUF_LEN 32
    char buf[SIZE_BUF_LEN];
    char buf_available[SIZE_BUF_LEN];
    char buf_unavailable[SIZE_BUF_LEN];
    format_size(buf, sizeof(buf), x86_platform.mem.total);
    format_size(buf_available, sizeof(buf_available), x86_platform.mem.available);
    format_size(buf_unavailable, sizeof(buf_unavailable), x86_platform.mem.total - x86_platform.mem.available);
    pr_info("Total Memory: %s (%s available, %s unavailable)", buf, buf_available, buf_unavailable);
}
