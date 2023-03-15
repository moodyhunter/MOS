// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmalloc.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

/**
 * @brief A node in the physical memory manager's linked list.
 *
 * @details
 * A valid pmlist_node_t can be in exactly one of the following states:
 * - free:         Declared as free by the bootloader and has not yet been allocated or reserved.
 *                 Stored in the free list, reading its reference count is undefined.
 *
 * - allocated:    The block is allocated by the kernel, and is not yet freed.
 *                 Stored in the allocated list, with a reference count initialized to 0.
 *
 * - reserved:     The block is reserved by the bootloader, or manually reserved by the kernel.
 *                 The block is in in the allocated list, and with a reference count initialized to 1,
 *                 so that it is never freed.
 *
 * An invalid pmlist_node_t has its `type' field set to `PMM_REGION_UNINITIALIZED' and is not in any list.
 * It is not safe to read any other fields of an invalid pmlist_node_t, such nodes can only be seen in
 * 'pmm_early_storage'.
 *
 */
typedef struct
{
    as_linked_list;
    pmrange_t range;
    atomic_t refcount;
    pm_range_type_t type;
} pmlist_node_t;

static pmlist_node_t pmm_early_storage[MOS_PMM_EARLY_MEMREGIONS] = { 0 };

static bool pmm_use_kernel_heap = false;
static spinlock_t pmm_region_lock = SPINLOCK_INIT;

static list_head pmlist_free_rw = LIST_HEAD_INIT(pmlist_free_rw);
const list_head *const pmlist_free = &pmlist_free_rw;

static list_head pmlist_allocated_rw = LIST_HEAD_INIT(pmlist_allocated_rw);
const list_head *const pmlist_allocated = &pmlist_allocated_rw;

static pmlist_node_t *pmm_alloc_new_block(uintptr_t start, size_t n_pages, pm_range_type_t type)
{
    MOS_ASSERT_X(type != PMM_REGION_UNINITIALIZED, "pmm_alloc_new_block() called with type == PMM_REGION_UNINITIALIZED");
    pmlist_node_t *node = NULL;
    if (likely(pmm_use_kernel_heap))
    {
        node = kmalloc(sizeof(pmlist_node_t)); // zeroed later
    }
    else
    {
        MOS_ASSERT_X(spinlock_is_locked(&pmm_region_lock), "pmm_alloc_new_block() called without holding pmm_region_lock");
        for (size_t i = 0; i < MOS_PMM_EARLY_MEMREGIONS; i++)
        {
            if (pmm_early_storage[i].type != PMM_REGION_UNINITIALIZED)
                continue;

            pmm_early_storage[i].type = type;
            node = &pmm_early_storage[i];
            break;
        }
    }

    MOS_ASSERT_X(node, "MOS_PMM_EARLY_MEMREGIONS (%d) is too small!", MOS_PMM_EARLY_MEMREGIONS);

    memzero(node, sizeof(pmrange_t));
    linked_list_init(list_node(node));
    node->range.paddr = start;
    node->range.npages = n_pages;

    return node;
}

static void pmm_dealloc_block(pmlist_node_t *node)
{
    // check if it's an early region
    for (size_t i = 0; i < MOS_PMM_EARLY_MEMREGIONS; i++)
    {
        if (&pmm_early_storage[i] == node)
        {
            pmm_early_storage[i].type = PMM_REGION_UNINITIALIZED;
            return;
        }
    }

    // no, it's a dynamically allocated region
    kfree(node);
}

static void pmm_add_free_pages(uintptr_t start, size_t n_pages, pm_range_type_t type)
{
    const uintptr_t end = start + n_pages * MOS_PAGE_SIZE;
    MOS_ASSERT_X(n_pages > 0, "pmm_add_free_pages() called with n_pages == 0");

    // traverse the list to find the correct insertion point
    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmlist_node_t, current, pmlist_free_rw)
    {
        const uintptr_t cstart = current->range.paddr;
        const uintptr_t cend = cstart + current->range.npages * MOS_PAGE_SIZE;

        // detect overlapping regions
        const bool start_in_cregion = cstart <= start && start < cend;
        const bool end_in_cregion = cstart < end && end <= cend;
        const bool enclosing_cregion = start <= cstart && cend <= end;

        if (start_in_cregion || end_in_cregion || enclosing_cregion)
            mos_panic("physical memory region " PTR_FMT "-" PTR_FMT " overlaps with existing region " PTR_FMT "-" PTR_FMT, start, end, cstart, cend);

        if (cstart <= start)
            continue;

// some helper macros to avoid code duplication
#define extend_previous_at_end()  prev->range.npages += n_pages
#define extend_current_at_start() current->range.paddr = start, current->range.npages += n_pages
#define insert_before_current()   list_insert_before(current, pmm_alloc_new_block(start, n_pages, type))

        // ! Now we have found the insertion point (i.e., before `current`)
        pmlist_node_t *prev = list_prev_entry(current, pmlist_node_t);
        if (list_node(prev) == pmlist_free)
        {
            // * `current` is the first list node
            prev = NULL;

            // check if we can extend current at the start
            if (cstart == end)
            {
                extend_current_at_start();
                goto done;
            }

            // otherwise insert a new region before current
            insert_before_current();
            goto done;
        }

        const uintptr_t pstart = prev->range.paddr;
        const uintptr_t pend = pstart + prev->range.npages * MOS_PAGE_SIZE;

        // check if we can extend previous at the end
        if (prev && pend == start)
        {
            extend_previous_at_end();

            // continue check if we can also 'fill the gap' between previous and current
            if (cstart == end)
            {
                // also extend the previous region till the end of current
                prev->range.npages += current->range.npages;
                list_remove(current); // remove current from the list
                pmm_dealloc_block(current);
                goto done;
            }

            goto done;
        }

        // check if we can extend current at the start
        if (cstart == end)
        {
            extend_current_at_start();
            goto done;
        }

        // otherwise insert a new region before current
        insert_before_current();
        goto done;
    }

#undef extend_previous_at_end
#undef extend_current_at_start
#undef insert_before_current

    // if we get here, we have to insert the new region at the end of the list
    pmlist_node_t *last = pmm_alloc_new_block(start, n_pages, type);
    list_node_append(&pmlist_free_rw, list_node(last));

done:
    spinlock_release(&pmm_region_lock);
}

static pmrange_t *pmm_acquire_pages_at(uintptr_t start_addr, size_t npages, pm_range_type_t type)
{
    const uintptr_t end_addr = start_addr + npages * MOS_PAGE_SIZE;

    mos_debug(pmm, "allocating " PTR_FMT "-" PTR_FMT, start_addr, end_addr);

    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmlist_node_t, this, pmlist_free_rw)
    {
#define this_start (this->range.paddr)
#define this_end   (this->range.paddr + this->range.npages * MOS_PAGE_SIZE)

        // if this region is completely before the region we want to remove, skip it
        if (start_addr < this_start || this_end < end_addr)
            continue;

        MOS_ASSERT_X(this->type == type, "pmm_acquire_pages_at(): region type mismatch");

        //
        //       |-> Start of this region
        //       |               End of this region <-|
        // ======|====================================|======
        //  PREV | PART 1 | REGION TO REMOVE | PART 2 | NEXT
        // ======|========|==================|========|======
        //                |-> start_addr     |
        //                        end_addr <-|
        //
        const size_t part_1_size = start_addr - this_start;
        const size_t part_2_size = this_end - end_addr;

        if (part_1_size == 0 && part_2_size != 0)
        {
            // part 1 is empty, which means we are removing from the front of the region
            this->range.paddr = end_addr;
            this->range.npages = part_2_size / MOS_PAGE_SIZE;
            mos_debug(pmm, "case 1: shrink " PTR_FMT ", new_size=%zd", this_start, this->range.npages);
        }
        else if (part_1_size != 0 && part_2_size == 0)
        {
            // part 2 is empty, which means we are removing the tail of part 1
            mos_debug(pmm, "case 2: shrink " PTR_FMT "-" PTR_FMT ": new_end=" PTR_FMT, this_start, this_end, this_start + (this->range.npages - npages) * MOS_PAGE_SIZE);
            this->range.npages -= npages;
        }
        else if (part_1_size == 0 && part_2_size == 0)
        {
            // both part 1 and part 2 are empty, which means we are removing the whole region
            mos_debug(pmm, "case 3: remove block " PTR_FMT "-" PTR_FMT, this_start, this_end);

            // remove it from the list
            list_remove(this);
            pmm_dealloc_block(this);
        }
        else
        {
            // neither part 1 nor part 2 is empty, so we have to allocate a new entry for part 2
            mos_debug(pmm, "case 4: split " PTR_FMT "-" PTR_FMT "->" PTR_FMT "-" PTR_FMT "," PTR_FMT "-" PTR_FMT, this_start, this_end, this_start, start_addr, end_addr,
                      end_addr + part_2_size);
            this->range.npages = part_1_size / MOS_PAGE_SIZE;
            pmlist_node_t *new = pmm_alloc_new_block(end_addr, part_2_size / MOS_PAGE_SIZE, this->type);
            list_insert_after(this, new); // insert after this
        }

        spinlock_release(&pmm_region_lock);

        // allocate the new block
        // no matter how we split the region, we always allocate the region
        // from start_addr to end_addr
        pmlist_node_t *new = pmm_alloc_new_block(start_addr, npages, type);
        new->refcount = 0;
        return &new->range;

#undef this_start
#undef this_end
    }

    spinlock_release(&pmm_region_lock);
    mos_panic("cannot allocate contiguous pages " PTR_FMT "-" PTR_FMT, start_addr, end_addr);
}

void pmm_dump(void)
{
    pr_info("Physical Memory Manager dump:");
    char sbuf[32];

    size_t i = 0;
    list_foreach(pmlist_node_t, r, add_const(pmlist_free_rw))
    {
        format_size(sbuf, sizeof(sbuf), r->range.npages * MOS_PAGE_SIZE);
        const uintptr_t end = r->range.paddr + r->range.npages * MOS_PAGE_SIZE - 1;
        const char *const type = r->type == PMM_REGION_FREE ? "available" : "reserved";
        pr_info("%2zd: " PTR_FMT "-" PTR_FMT " (%zu page(s), %s, %s)", i, r->range.paddr, end, r->range.npages, sbuf, type);
        i++;
    }
}

void pmm_switch_to_kheap(void)
{
    MOS_ASSERT_X(!pmm_use_kernel_heap, "pmm_switch_to_kheap() called twice");
    pmm_use_kernel_heap = true;
    pr_info("pmm: switched to kernel heap");
}

void mos_pmm_add_region(uintptr_t start_addr, size_t nbytes, pm_range_type_t type)
{
    // TODO: should we align up to page size?
    const uintptr_t start = ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t end = ALIGN_UP_TO_PAGE(start_addr + nbytes);
    const size_t npages = (end - start) / MOS_PAGE_SIZE;

    if (unlikely(npages == 0))
    {
        pr_warn("physical memory region " PTR_FMT "-" PTR_FMT " is empty after alignment", start, end);
        return;
    }

    const size_t loss = (start - start_addr) + (end - (start_addr + nbytes));
    if (unlikely(loss))
        pr_warn("physical memory region " PTR_FMT "-" PTR_FMT " is not page-aligned, losing %zu bytes", start_addr, start_addr + nbytes, loss);

    if (unlikely(end < 1 MB))
    {
        type = PMM_REGION_RESERVED;
        pr_info2("reserving a low memory region " PTR_FMT "-" PTR_FMT " (%zu page(s))", start, end, npages);
    }

    pmm_add_free_pages(start, npages, type);
}

bool pmm_allocate_frames(size_t n_pages, pmm_op_callback_t callback, void *arg)
{
    mos_debug(pmm, "allocating %zu page(s)", n_pages);

    // typedef void (*pmm_op_callback_t)(size_t i, const pmm_op_state_t *op_state, const pmrange_t *current, void *arg);
    size_t i = 0;
    pmm_op_state_t op_state = { .pages_operated = 0, .pages_requested = n_pages };

#define n_left() (n_pages - (op_state.pages_operated))
    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmlist_node_t, current, pmlist_free_rw)
    {
    // this is necessary because we might remove the current node from the list
    // and we don't want to skip the next node
    next_node:
        // check if we are at the end of the list, or if we have allocated enough pages
        if (list_node(current) == &pmlist_free_rw || n_left() == 0)
            break;

        // skip reserved regions (of course!)
        if (current->type != PMM_REGION_FREE)
            continue;

        // check if we can allocate the whole region
        if (current->range.npages <= n_left())
        {
            MOS_ASSERT_X(current->refcount == 0, "allocated a region with refcount != 0");
            mos_debug(pmm, "  %8s: " PTR_FMT "-" PTR_FMT " (%zu page(s))", "whole", current->range.paddr, current->range.paddr + current->range.npages * MOS_PAGE_SIZE,
                      current->range.npages);
            pmlist_node_t *next = list_next_entry(current, pmlist_node_t);
            list_remove(current);

            callback(i++, &op_state, &current->range, arg);

            op_state.pages_operated += current->range.npages;
            current = next;
            goto next_node;
        }

        // only allocate a part of the region
        MOS_ASSERT(current->range.npages > n_left()); // of course, otherwise we would have allocated the whole region

        size_t n_left = n_left(); // number of pages left to allocate, don't use the macro here!

        pmlist_node_t *new = pmm_alloc_new_block(current->range.paddr, n_left, PMM_REGION_ALLOCATED);
        current->range.paddr += n_left * MOS_PAGE_SIZE;
        current->range.npages -= n_left;
        mos_debug(pmm, "  %8s: " PTR_FMT "-" PTR_FMT " (%zu page(s))", "partial", new->range.paddr, new->range.paddr + new->range.npages *MOS_PAGE_SIZE,
                  new->range.npages);

        callback(i++, &op_state, &new->range, arg);
        op_state.pages_operated += n_left;
    }
    spinlock_release(&pmm_region_lock);
#undef n_left

    if (op_state.pages_operated != n_pages)
        mos_panic("could not allocate %zu pages, only allocated %zu pages", n_pages, op_state.pages_operated);

    return true;
}

void pmm_ref_frames(uintptr_t paddr, size_t npages)
{
}

void pmm_unref_frames(uintptr_t paddr, size_t npages)
{
}

void pmm_reserve_frames(uintptr_t paddr, size_t npages)
{
}

pmrange_t pmm_ref_reserved_region(uintptr_t needle)
{
    pmrange_t range = { .paddr = 0, .npages = 0 };
    spinlock_acquire(&pmm_region_lock);
    list_foreach(pmlist_node_t, current, pmlist_free_rw)
    {
        if (current->type != PMM_REGION_RESERVED)
            continue;

        if (current->range.paddr <= needle && needle < current->range.paddr + current->range.npages * MOS_PAGE_SIZE)
        {
            range = current->range;
            current->refcount++;
            break;
        }
    }
    spinlock_release(&pmm_region_lock);

    if (range.paddr == 0)
        mos_panic("could not find reserved region for address " PTR_FMT, needle);

    // TODO: move the region to the allocated list and refcount it

    return range;
}
