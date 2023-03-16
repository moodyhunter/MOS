// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#define mos_pmm_impl

#include "mos/mm/physical/pmm.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "lib/sync/spinlock.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/physical/pmm_internal.h"

static pmlist_node_t pmm_early_storage[MOS_PMM_EARLY_MEMREGIONS] = { 0 };
static spinlock_t pmm_early_storage_lock = SPINLOCK_INIT;
bool pmm_use_kernel_heap = false;

pmlist_node_t *pmm_internal_list_node_create(uintptr_t start, size_t n_pages, pm_range_type_t type)
{
    MOS_ASSERT_X(type != PM_RANGE_UNINITIALIZED, "pmm_alloc_new_block() called with type == PMM_REGION_UNINITIALIZED");
    pmlist_node_t *node = NULL;
    if (likely(pmm_use_kernel_heap))
    {
        node = kmalloc(sizeof(pmlist_node_t)); // zeroed later
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
    }

    MOS_ASSERT_X(node, "MOS_PMM_EARLY_MEMREGIONS (%d) is too small!", MOS_PMM_EARLY_MEMREGIONS);

    memzero(node, sizeof(pmrange_t));
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
    pmm_use_kernel_heap = true;
    pr_info("pmm: switched to kernel heap");
}

void pmm_dump(void)
{
    pr_info("Physical Memory Manager dump:");
    char sbuf[32];

    size_t i = 0;
    list_foreach(pmlist_node_t, r, *pmlist_free)
    {
        format_size(sbuf, sizeof(sbuf), r->range.npages * MOS_PAGE_SIZE);
        const uintptr_t end = r->range.paddr + r->range.npages * MOS_PAGE_SIZE - 1;

        if (unlikely(r->type != PM_RANGE_FREE && r->type != PM_RANGE_RESERVED))
            pr_emerg("Invalid freelist region type %d", r->type);

        if (unlikely(r->refcount != 0))
            pr_emerg("Invalid refcount %zu", r->refcount);

        const char *const type = r->type == PM_RANGE_FREE ? "available" : "reserved";
        pr_info("%2zd: " PTR_FMT "-" PTR_FMT " (%zu page(s), %s, %s)", i, r->range.paddr, end, r->range.npages, sbuf, type);
        i++;
    }

    i = 0;
    pr_info("Allocated regions:");
    list_foreach(pmlist_node_t, a, *pmlist_allocated)
    {
        format_size(sbuf, sizeof(sbuf), a->range.npages * MOS_PAGE_SIZE);
        const uintptr_t end = a->range.paddr + a->range.npages * MOS_PAGE_SIZE - 1;

        if (unlikely(a->type != PM_RANGE_ALLOCATED && a->type != PM_RANGE_RESERVED))
            pr_emerg("Invalid allocated region type %d", a->type);

        const char *const type = a->type == PM_RANGE_ALLOCATED ? "allocated" : "reserved";
        pr_info("%2zd: " PTR_FMT "-" PTR_FMT " (%zu page(s), %s, %s, %zu refs)", i, a->range.paddr, end, a->range.npages, sbuf, type, a->refcount);
        i++;
    }
}

void pmm_add_region_bytes(uintptr_t start_addr, size_t nbytes, pm_range_type_t type)
{
    const uintptr_t start = ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t end = ALIGN_UP_TO_PAGE(start_addr + nbytes);
    const size_t n_pages = (end - start) / MOS_PAGE_SIZE;

    const size_t loss = (start - start_addr) + (end - (start_addr + nbytes));
    if (unlikely(loss))
        pr_warn("physical memory region " PTR_FMT "-" PTR_FMT " is not page-aligned, losing %zu bytes", start_addr, start_addr + nbytes, loss);

    pmm_internal_add_free_frames(start, n_pages, type);
}

// * Callback for pmm_allocate_frames (i.e. pmm_internal_allocate_free_frames)
static void pmm_internal_callback_handle_allocated_frames(const pmm_op_state_t *op_state, pmlist_node_t *node, pmm_allocate_callback_t user_callback, void *user_arg)
{
    node->refcount = 1;
    node->type = PM_RANGE_ALLOCATED;
    pmm_internal_add_node_to_allocated_list(node);
    if (user_callback)
        user_callback(op_state, &node->range, user_arg);
}

bool pmm_allocate_frames(size_t n_pages, pmm_allocate_callback_t callback, void *arg)
{
    return pmm_internal_acquire_free_frames(n_pages, pmm_internal_callback_handle_allocated_frames, callback, arg);
}

void pmm_ref_frames(uintptr_t paddr, size_t npages)
{
    pmm_internal_ref_range(paddr, npages);
}

void pmm_internal_callback_free_frames(pmlist_node_t *node, void *arg)
{
    MOS_UNUSED(arg);
    pmm_internal_add_free_frames_node(node);
}

void pmm_unref_frames(uintptr_t paddr, size_t npages)
{
    pmm_internal_unref_range(paddr, npages, pmm_internal_callback_free_frames, NULL);
}

uintptr_t pmm_reserve_frames(uintptr_t paddr, size_t npages)
{
    pmlist_node_t *node = pmm_internal_acquire_free_frames_at(paddr, npages);
    if (unlikely(!node))
    {
        // This is not a problem: the physical memory in question may be
        // a MMIO region, so that BIOS doesn't know about it.
        // We make up a fake node to satisfy the caller.
        node = pmm_internal_list_node_create(paddr, npages, PM_RANGE_RESERVED);
    }

    node->type = PM_RANGE_RESERVED;
    pmm_internal_add_node_to_allocated_list(node);
    return node->range.paddr;
}

pmrange_t pmm_reserve_block(uintptr_t needle)
{
    pmlist_node_t *node = pmm_internal_find_and_acquire_block(needle, PM_RANGE_RESERVED);
    if (unlikely(!node))
    {
        mos_warn("pmm_reserve_block(): failed to reserve block at " PTR_FMT, needle);
        return (pmrange_t){ 0 };
    }

    node->type = PM_RANGE_RESERVED;
    pmm_internal_add_node_to_allocated_list(node);
    return node->range;
}
