// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mmstat.hpp"

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/mm/paging/iterator.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/process.hpp"

#include <mos/mos_global.h>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>

typedef struct
{
    size_t npages;
} vmap_global_mstat_t;

static vmap_global_mstat_t stat[_MEM_MAX_TYPES] = { 0 };

const char *mem_type_names[_MEM_MAX_TYPES] = {
    [MEM_PAGETABLE] = "PageTable", //
    [MEM_SLAB] = "Slab",           //
    [MEM_PAGECACHE] = "PageCache", //
    [MEM_KERNEL] = "Kernel",       //
    [MEM_USER] = "User",           //
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
    char size_buf[32];
    format_size(size_buf, sizeof(size_buf), pmm_total_frames * MOS_PAGE_SIZE);
    sysfs_printf(f, "%-20s: %s, %zu pages\n", "Total", size_buf, pmm_total_frames);

    format_size(size_buf, sizeof(size_buf), pmm_allocated_frames * MOS_PAGE_SIZE);
    sysfs_printf(f, "%-20s: %s, %zu pages\n", "Allocated", size_buf, pmm_allocated_frames);

    format_size(size_buf, sizeof(size_buf), pmm_reserved_frames * MOS_PAGE_SIZE);
    sysfs_printf(f, "%-20s: %s, %zu pages\n", "Reserved", size_buf, pmm_reserved_frames);

    for (u32 i = 0; i < _MEM_MAX_TYPES; i++)
    {
        format_size(size_buf, sizeof(size_buf), stat[i].npages * MOS_PAGE_SIZE);
        sysfs_printf(f, "%-20s: %s, %zu pages\n", mem_type_names[i], size_buf, stat[i].npages);
    }
    return true;
}

static bool mmstat_sysfs_phyframe_stat_show(sysfs_file_t *f)
{
    const pfn_t pfn = (pfn_t) sysfs_file_get_data(f);
    if (pfn >= pmm_total_frames)
    {
        pr_warn("mmstat: invalid pfn %llu", pfn);
        return false;
    }

    const phyframe_t *frame = pfn_phyframe(pfn);
    sysfs_printf(f, "pfn: %llu\n", pfn);
    sysfs_printf(f, "type: %s\n", frame->state == phyframe::PHYFRAME_FREE ? "free" : frame->state == phyframe::PHYFRAME_ALLOCATED ? "allocated" : "reserved");
    sysfs_printf(f, "order: %u\n", frame->order);
    if (frame->state == phyframe::PHYFRAME_ALLOCATED)
        sysfs_printf(f, "refcnt: %zu\n", frame->alloc.refcount.load());

    return true;
}

static size_t mmstat_sysfs_phyframe_stat_store(sysfs_file_t *f, const char *buf, size_t count, off_t offset)
{
    MOS_UNUSED(offset);

    const pfn_t pfn = strntoll(buf, NULL, 10, count);
    if (pfn >= pmm_total_frames)
    {
        pr_warn("mmstat: invalid pfn %llu", pfn);
        return -EINVAL;
    }

    sysfs_file_set_data(f, (void *) pfn);
    return count;
}

static bool mmstat_sysfs_pagetable_show(sysfs_file_t *f)
{
    const pid_t pid = (pid_t) (ptr_t) sysfs_file_get_data(f);
    if (!pid)
    {
        pr_warn("mmstat: invalid pid %d", pid);
        return false;
    }

    const auto pproc = process_get(pid);
    if (!pproc)
    {
        pr_warn("mmstat: invalid pid %d", pid);
        return false;
    }

    const auto proc = *pproc;
    MMContext *mmctx = proc->mm;
    spinlock_acquire(&mmctx->mm_lock);

    pagetable_iter_t iter;
    pagetable_iter_init(&iter, mmctx->pgd, 0, MOS_USER_END_VADDR);

    pagetable_iter_range_t *range;
    while ((range = pagetable_iter_next(&iter)))
    {
        if (!range->present)
            continue;

        sysfs_printf(f, PTR_RANGE, range->vaddr, range->vaddr_end);
        sysfs_printf(f, " %pvf " PFN_RANGE, (void *) &range->flags, range->pfn, range->pfn_end);
        sysfs_printf(f, "\n");
    }

    spinlock_release(&mmctx->mm_lock);

    return true;
}

static bool mmstat_sysfs_vmaps_show(sysfs_file_t *f)
{
    const pid_t pid = (pid_t) (ptr_t) sysfs_file_get_data(f);
    if (!pid)
    {
        pr_warn("mmstat: invalid pid %d", pid);
        return false;
    }

    const auto pproc = process_get(pid);
    if (!pproc)
    {
        pr_warn("mmstat: invalid pid %d", pid);
        return false;
    }

    const auto proc = *pproc;

    int i = 0;
    spinlock_acquire(&proc->mm->mm_lock);
    list_foreach(vmap_t, vmap, proc->mm->mmaps)
    {
        sysfs_printf(f, "%3d: ", i++);
        sysfs_printf(f, "%pvm\n", (void *) vmap);
    }
    spinlock_release(&proc->mm->mm_lock);
    return true;
}

static size_t mmstat_sysfs_store_pid(sysfs_file_t *f, const char *buf, size_t count, off_t offset)
{
    MOS_UNUSED(offset);

    const pid_t pid = strntoll(buf, NULL, 10, count);
    if (!pid)
    {
        pr_warn("mmstat: invalid pid %d", pid);
        sysfs_file_set_data(f, (void *) 0);
        return -EINVAL;
    }

    sysfs_file_set_data(f, (void *) (ptr_t) pid);
    return count;
}

static sysfs_item_t mmstat_sysfs_items[] = {
    SYSFS_RO_ITEM("stat", mmstat_sysfs_stat),
    SYSFS_RW_ITEM("phyframe_stat", mmstat_sysfs_phyframe_stat_show, mmstat_sysfs_phyframe_stat_store),
    SYSFS_RW_ITEM("pagetable", mmstat_sysfs_pagetable_show, mmstat_sysfs_store_pid),
    SYSFS_RW_ITEM("vmaps", mmstat_sysfs_vmaps_show, mmstat_sysfs_store_pid),
};

SYSFS_AUTOREGISTER(mmstat, mmstat_sysfs_items);
