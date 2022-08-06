// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/x86_platform.h"

memblock_t x86_mem_regions[MEM_MAX_BLOCKS] = { 0 };
size_t x86_mem_regions_count = 0;

void x86_mem_init(const multiboot_mmap_entry_t *map_entry, u32 count)
{
    pr_info("Multiboot memory map:");
    u64 mem_size_total = 0, mem_size_available = 0;
    for (u32 i = 0; i < count; i++)
    {
        const multiboot_mmap_entry_t *entry = map_entry + i;
        x86_mem_add_region(entry->phys_addr, entry->len, entry->type == MULTIBOOT_MEMORY_AVAILABLE);
        mem_size_total += entry->len;

        char *type_str = "";
        switch (entry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "Available", mem_size_available += entry->len; break;
            case MULTIBOOT_MEMORY_RESERVED: type_str = "Reserved"; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: type_str = "ACPI Reclaimable"; break;
            case MULTIBOOT_MEMORY_NVS: type_str = "NVS"; break;
            case MULTIBOOT_MEMORY_BADRAM: type_str = "Bad RAM"; break;
            default: mos_panic("unsupported memory map type: %x", entry->type);
        }

        char size_buf[32];
        format_size(size_buf, sizeof(size_buf), entry->len);
        pr_debug("  %d: 0x%.8llx (+0x%.8llx): %-10s (%s)", i, entry->phys_addr, entry->len, type_str, size_buf);
    }

#define SIZE_BUF_LEN 8
    char buf[SIZE_BUF_LEN];
    char buf_available[SIZE_BUF_LEN];
    char buf_unavailable[SIZE_BUF_LEN];
    format_size(buf, sizeof(buf), mem_size_total);
    format_size(buf_available, sizeof(buf_available), mem_size_available);
    format_size(buf_unavailable, sizeof(buf_unavailable), mem_size_total - mem_size_available);
    pr_info("Total Memory: %s (%s available, %s unavailable)", buf, buf_available, buf_unavailable);
}

void x86_mem_add_region(u64 phys_addr, size_t size, bool available)
{
    if (x86_mem_regions_count == MEM_MAX_BLOCKS)
        mos_panic("too many memory regions added.");

    memblock_t *block = &x86_mem_regions[x86_mem_regions_count];
    block->paddr = phys_addr;
    block->size = size;
    block->available = available;
    x86_mem_regions_count++;
}
