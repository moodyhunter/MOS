// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/mm/paging.h"

void x86_setup_mm(multiboot_mmap_entry_t *map_entry, u32 count)
{
    pr_info("Multiboot memory map:");
    for (u32 i = 0; i < count; i++)
    {
        multiboot_mmap_entry_t *entry = map_entry + i;
        mos_mem_add_region(entry->addr, entry->len, entry->type == MULTIBOOT_MEMORY_AVAILABLE);

        char *type_str = "";
        char size_buf[32];
        switch (entry->type)
        {
            case MULTIBOOT_MEMORY_AVAILABLE: type_str = "Available"; break;
            case MULTIBOOT_MEMORY_RESERVED: type_str = "Reserved"; break;
            case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: type_str = "ACPI Reclaimable"; break;
            case MULTIBOOT_MEMORY_NVS: type_str = "NVS"; break;
            case MULTIBOOT_MEMORY_BADRAM: type_str = "Bad RAM"; break;
            default: mos_panic("unsupported memory map type: %x", entry->type);
        }
        format_size(size_buf, sizeof(size_buf), entry->len);

        pr_debug("  %d: 0x%.8llx (+0x%.8llx): %-10s (%s)", i, entry->addr, entry->len, type_str, size_buf);
    }

    mos_mem_finish_setup();
    x86_mm_setup_paging();
}
