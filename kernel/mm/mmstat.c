// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mmstat.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/physical/pmm.h"
#include "mos/printk.h"

typedef struct
{
    size_t npages;
} vmap_global_mstat_t;

static vmap_global_mstat_t stat[_MEM_MAX_TYPES] = { 0 };

const char *mem_type_names[_MEM_MAX_TYPES] = {
    [MEM_PAGETABLE] = "PageTable",
    [MEM_SLAB] = "Slab",
    [MEM_KERNEL] = "Kernel",
    [MEM_USER] = "User",
};

void mmstat_inc(mmstat_type_t type, size_t size)
{
    MOS_ASSERT(type < _MEM_MAX_TYPES);
    stat[type].npages += size;
}

void mmstat_dec(mmstat_type_t type, size_t size)
{
    MOS_ASSERT(type < _MEM_MAX_TYPES);
    stat[type].npages -= size;
}

// ! sysfs support

static bool mmstat_sysfs_stat(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s: %zu pages\n", "Total", pmm_total_frames);
    sysfs_printf(f, "%-20s: %zu pages\n", "Allocated", pmm_allocated_frames);
    sysfs_printf(f, "%-20s: %zu pages\n", "Reserved", pmm_reserved_frames);

    for (u32 i = 0; i < _MEM_MAX_TYPES; i++)
        sysfs_printf(f, "%-20s: %zu pages\n", mem_type_names[i], stat[i].npages);
    return true;
}

static sysfs_item_t mmstat_sysfs_items[] = {
    SYSFS_RO_ITEM("stat", mmstat_sysfs_stat),
    SYSFS_END_ITEM,
};

SYSFS_AUTOREGISTER(mmstat, mmstat_sysfs_items);
