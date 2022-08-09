// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "lib/stdlib.h"
#include "mos/printk.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/x86_platform.h"

memblock_t x86_mem_regions[MEM_MAX_N_REGIONS] = { 0 };
size_t x86_mem_regions_count = 0;

size_t x86_mem_size_total = 0;
size_t x86_mem_size_available = 0;

void x86_mem_init(const multiboot_mmap_entry_t *map_entry, u32 count)
{
    pr_info("Multiboot memory map:");
    for (u32 i = 0; i < count; i++)
    {
        char size_buf[32];
        const multiboot_mmap_entry_t *entry = map_entry + i;

        u64 region_length = entry->len;
        u64 region_base = entry->phys_addr;

        format_size(size_buf, sizeof(size_buf), region_length);

        if (region_base > X86_MAX_MEM_SIZE)
        {
            pr_warn("ignoring a %s long memory region starting at 0x%llx", size_buf, region_base);
            continue;
        }

        if (region_base + region_length > (u64) X86_MAX_MEM_SIZE + 1)
        {
            region_length = X86_MAX_MEM_SIZE - region_base;
            format_size(size_buf, sizeof(size_buf), region_length);
            pr_warn("truncating memory region at 0x%llx, it extends beyond the maximum address 0x%x", region_base, X86_MAX_MEM_SIZE);
        }

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
            x86_mem_add_available_region(region_base, region_length);

        x86_mem_size_total += region_length;

        char *type_str = "";
        switch (entry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "Available", x86_mem_size_available += region_length; break;
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
    format_size(buf, sizeof(buf), x86_mem_size_total);
    format_size(buf_available, sizeof(buf_available), x86_mem_size_available);
    format_size(buf_unavailable, sizeof(buf_unavailable), x86_mem_size_total - x86_mem_size_available);
    pr_info("Total Memory: %s (%s available, %s unavailable)", buf, buf_available, buf_unavailable);
}

void x86_mem_add_available_region(u64 phys_addr, size_t size)
{
    if (x86_mem_regions_count == MEM_MAX_N_REGIONS)
        mos_panic("too many memory regions added.");

    memblock_t *block = &x86_mem_regions[x86_mem_regions_count];
    block->paddr = phys_addr;
    block->size_bytes = size;
    block->available = true;
    x86_mem_regions_count++;
}
