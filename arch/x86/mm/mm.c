// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "lib/stdlib.h"
#include "mos/printk.h"

memblock_t x86_mem_regions[MEM_MAX_N_REGIONS] = { 0 };
size_t x86_mem_regions_count = 0;

size_t x86_mem_size_total = 0;
size_t x86_mem_size_available = 0;

void x86_mem_init(const multiboot_mmap_entry_t *map_entry, u32 count)
{
    pr_info("Multiboot memory map:");
    for (u32 i = 0; i < count; i++)
    {
        const multiboot_mmap_entry_t *entry = map_entry + i;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
            x86_mem_add_available_region(entry->phys_addr, entry->len);

        x86_mem_size_total += entry->len;

        char *type_str = "";
        switch (entry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "Available", x86_mem_size_available += entry->len; break;
            case MULTIBOOT_MEMORY_RESERVED: type_str = "Reserved"; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: type_str = "ACPI Reclaimable"; break;
            case MULTIBOOT_MEMORY_NVS: type_str = "NVS"; break;
            case MULTIBOOT_MEMORY_BADRAM: type_str = "Bad RAM"; break;
            default: mos_panic("unsupported memory map type: %x", entry->type);
        }

        char size_buf[32];
        format_size(size_buf, sizeof(size_buf), entry->len);
        pr_info2("  %d: 0x%.8llx (+0x%.8llx): %-10s (%s)", i, entry->phys_addr, entry->len, type_str, size_buf);
    }

#define SIZE_BUF_LEN 8
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
