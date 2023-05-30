// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#define mos_pmm_impl

#include "mos/mm/slab.h"

#include <mos/lib/sync/spinlock.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mm/physical/pmm_internal.h>
#include <stdlib.h>
#include <string.h>

static pmlist_node_t pmm_early_storage[MOS_PMM_EARLY_MEMREGIONS] = { 0 };
static spinlock_t pmm_early_storage_lock = SPINLOCK_INIT;
bool pmm_use_kernel_heap = false;
static slab_t *pmlist_node_cache = NULL;

// array of all physical frames
static phyframe_t *phyframes = (phyframe_t *) MOS_PHYFRAME_ARRAY_VADDR;
static size_t phyframes_count = 0;

pmlist_node_t *pmm_internal_list_node_create(ptr_t start, size_t n_pages, pm_range_type_t type)
{
    MOS_ASSERT_X(type != PM_RANGE_UNINITIALIZED, "pmm_internal_list_node_create() called with type == PM_RANGE_UNINITIALIZED");
    pmlist_node_t *node = NULL;
    if (likely(pmm_use_kernel_heap))
    {
        node = kmemcache_alloc(pmlist_node_cache);
    }
    else
    {
        spinlock_acquire(&pmm_early_storage_lock);
        for (size_t i = 0; i < MOS_PMM_EARLY_MEMREGIONS; i++)
        {
            if (pmm_early_storage[i].type != PM_RANGE_UNINITIALIZED)
                continue;

            node = &pmm_early_storage[i];
            break;
        }
        spinlock_release(&pmm_early_storage_lock);
        MOS_ASSERT_X(node, "MOS_PMM_EARLY_MEMREGIONS (%d) is too small!", MOS_PMM_EARLY_MEMREGIONS);
    }

    memzero(node, sizeof(*node));
    linked_list_init(list_node(node));
    node->range.paddr = start;
    node->range.npages = n_pages;
    node->type = type;

    return node;
}

void pmm_internal_list_node_delete(pmlist_node_t *node)
{
    // check if it's an early region
    spinlock_acquire(&pmm_early_storage_lock);
    for (size_t i = 0; i < MOS_PMM_EARLY_MEMREGIONS; i++)
    {
        if (&pmm_early_storage[i] == node)
        {
            pmm_early_storage[i].type = PM_RANGE_UNINITIALIZED;
            spinlock_release(&pmm_early_storage_lock);
            return;
        }
    }
    spinlock_release(&pmm_early_storage_lock);

    // no, it's a dynamically allocated region
    kfree(node);
}

void pmm_switch_to_kheap(void)
{
    MOS_ASSERT_X(!pmm_use_kernel_heap, "pmm_switch_to_kheap() called twice");
    pmlist_node_cache = kmemcache_create("pmlist_node", sizeof(pmlist_node_t));
    pmm_use_kernel_heap = true;
    pr_info("pmm: switched to kernel heap");
}

void pmm_dump_lists(void)
{
    pr_info("Physical Memory Manager dump:");

    size_t i = 0;
    list_foreach(pmlist_node_t, r, *pmlist_free)
    {
        const ptr_t end = r->range.paddr + r->range.npages * MOS_PAGE_SIZE - 1;

        if (unlikely(r->type != PM_RANGE_FREE && r->type != PM_RANGE_RESERVED))
            pr_emerg("Invalid freelist region type %d", r->type);

        if (unlikely(r->refcount != 0))
            pr_emerg("Invalid refcount %zu", r->refcount);

        const char *const type = r->type == PM_RANGE_FREE ? "free" : "reserved";
        pr_info2("%5zd: " PTR_RANGE " (%10zu page(s), %s)", i, r->range.paddr, end, r->range.npages, type);
        i++;
    }

    i = 0;
    pr_info("Allocated regions:");
    list_foreach(pmlist_node_t, a, *pmlist_allocated)
    {
        const ptr_t end = a->range.paddr + a->range.npages * MOS_PAGE_SIZE - 1;

        if (unlikely(a->type != PM_RANGE_ALLOCATED && a->type != PM_RANGE_RESERVED))
            pr_emerg("Invalid allocated region type %d", a->type);

        const char *const type = a->type == PM_RANGE_ALLOCATED ? "alloc" : "reserved";
        pr_info2("%5zd: " PTR_RANGE " (%10zu page(s), %4zu refs, %s)", i, a->range.paddr, end, a->range.npages, a->refcount, type);
        i++;
    }
}

void pmm_register_phyframes(ptr_t start_addr, size_t nframes, pm_range_type_t type)
{
    pr_info2("pmm: adding region " PTR_RANGE " (%zu pages) as %s", start_addr, start_addr + nframes * MOS_PAGE_SIZE - 1, nframes,
             type == PM_RANGE_FREE ? "free" : "reserved");
    pmm_impl_add_free_frames(start_addr, nframes, type);

    // new API
    const size_t base_pfn = start_addr / MOS_PAGE_SIZE;

    // TODO: this takes years to complete, consider using a bitmap
    for (size_t i = 0; i < nframes; i++)
        phyframes[base_pfn + i].state = type == PM_RANGE_FREE ? PHYFRAME_FREE : PHYFRAME_RESERVED;

    phyframes_count = MAX(phyframes_count, base_pfn + nframes);
}

// * Callback for pmm_allocate_frames (i.e. pmm_internal_acquire_free_frames)
static void pmm_cb_allocate_frames(const pmm_op_state_t *op_state, pmlist_node_t *node, pmm_allocate_callback_t user_callback, void *user_arg)
{
    node->refcount = 1;
    node->type = PM_RANGE_ALLOCATED;
    pmm_impl_add_allocated_node(node);
    if (user_callback)
        user_callback(op_state, &node->range, user_arg);
}

bool pmm_allocate_frames(size_t n_pages, pmm_allocate_callback_t callback, void *arg)
{
    mos_debug(pmm, "allocating %zu page(s)", n_pages);
    return pmm_impl_get_free_frames(n_pages, pmm_cb_allocate_frames, callback, arg);
}

static void pmm_cb_unref_frames_unlocked(pmlist_node_t *n, void *arg)
{
    MOS_ASSERT(n->refcount == 0 && n->type == PM_RANGE_ALLOCATED);
    list_remove(n);
    MOS_UNUSED(arg);
    n->type = PM_RANGE_FREE;
    mos_debug(pmm_impl, "refcount drops to zero, freeing " PTR_RANGE ", %zu pages", n->range.paddr, n->range.paddr + n->range.npages * MOS_PAGE_SIZE, n->range.npages);
    pmm_impl_add_free_node_unlocked(n);
}

void pmm_ref_frames(ptr_t start, size_t n_pages)
{
    mos_debug(pmm, "ref range: " PTR_RANGE ", %zu pages", start, start + n_pages * MOS_PAGE_SIZE, n_pages);
    pmm_impl_walk_alloclist(start, n_pages, OP_REF, NULL, NULL);
}

void pmm_unref_frames(ptr_t start, size_t n_pages)
{
    mos_debug(pmm, "unref range: " PTR_RANGE ", %zu pages", start, start + n_pages * MOS_PAGE_SIZE, n_pages);
    spinlock_acquire(&pmlist_free_lock);
    pmm_impl_walk_alloclist(start, n_pages, OP_UNREF, pmm_cb_unref_frames_unlocked, NULL);
    spinlock_release(&pmlist_free_lock);
}

ptr_t pmm_reserve_frames(ptr_t paddr, size_t npages)
{
    mos_debug(pmm, "looking for region " PTR_RANGE, paddr, paddr + npages * MOS_PAGE_SIZE);
    pmlist_node_t *node = pmm_impl_get_free_frames_at(paddr, npages);
    if (unlikely(!node))
    {
        // This is not a problem: the physical memory in question may be
        // a MMIO region, so that BIOS doesn't know about it.
        // We make up a fake node to satisfy the caller.
        node = pmm_internal_list_node_create(paddr, npages, PM_RANGE_RESERVED);
    }

    node->type = PM_RANGE_RESERVED;
    pmm_impl_add_allocated_node(node);
    return node->range.paddr;
}

pmrange_t pmm_reserve_block(ptr_t needle)
{
    pmlist_node_t *node = pmm_impl_find_and_acquire_block(needle, PM_RANGE_RESERVED);
    if (unlikely(!node))
    {
        mos_warn("pmm_reserve_block(): failed to reserve block at " PTR_FMT, needle);
        return (pmrange_t){ 0 };
    }

    node->type = PM_RANGE_RESERVED;
    pmm_impl_add_allocated_node(node);
    return node->range;
}
