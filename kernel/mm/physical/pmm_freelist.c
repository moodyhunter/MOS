// SPDX-License-Identifier: GPL-3.0-or-later

#define mos_pmm_impl

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mm/physical/pmm_internal.h>
#include <string.h>

static list_head pmlist_free_rw = LIST_HEAD_INIT(pmlist_free_rw);
spinlock_t pmlist_free_lock = SPINLOCK_INIT;
const list_head *const pmlist_free = &pmlist_free_rw;

/**
 * @brief Tries to add the given range to the free list by merging it with existing nodes.
 *
 * @param start The start address of the range.
 * @param n_pages The number of pages in the range.
 * @param type The type of the range.
 * @return true if the range is successfully added, false otherwise, in which case the caller should
 *         add the range as a new node.
 */
static bool pmm_internal_do_add_free_frames_try_merge(ptr_t start, size_t n_pages, pm_range_type_t type)
{
    const ptr_t end = start + n_pages * MOS_PAGE_SIZE;
    list_foreach(pmlist_node_t, current, pmlist_free_rw)
    {
        const ptr_t cstart = current->range.paddr;
        const ptr_t cend = cstart + current->range.npages * MOS_PAGE_SIZE;

        // detect overlapping regions
        const bool start_in_cregion = cstart <= start && start < cend;
        const bool end_in_cregion = cstart < end && end <= cend;
        const bool enclosing_cregion = start <= cstart && cend <= end;

        if (start_in_cregion || end_in_cregion || enclosing_cregion)
            mos_panic("physical memory region " PTR_RANGE " overlaps with existing region " PTR_RANGE, start, end, cstart, cend);

        if (cstart <= start)
            continue;

        // ! Now we have found the insertion point (i.e., before `current`)
        list_node_t *prev_node = list_node(current)->prev;

        const bool has_prev = prev_node != &pmlist_free_rw;
        pmlist_node_t *const prev = has_prev ? list_entry(prev_node, pmlist_node_t) : NULL;

        // check if we can extend previous at the end
        if (has_prev && (prev->range.paddr + prev->range.npages * MOS_PAGE_SIZE) == start)
        {
            // if so, extend the previous region
            prev->range.npages += n_pages;

            // continue check if we can connect the previous and current region
            if (cstart == end)
            {
                // if so, extend the previous region to include the current region
                prev->range.npages += current->range.npages;
                list_remove(current); // remove current from the list
                pmm_internal_list_node_delete(current);
            }
        }
        else if (cstart == end)
        {
            // extend the current region
            current->range.paddr = start, current->range.npages += n_pages;
        }
        else
        {
            // otherwise insert a new region before current
            pmlist_node_t *node = pmm_internal_list_node_create(start, n_pages, type);
            list_insert_before(current, node);
        }

#undef extend_previous_at_end
#undef extend_current_at_start
#undef insert_before_current

        // done
        return true;
    }

    // we have to insert the new region at the end of the list
    return false;
}

void pmm_impl_add_free_frames(ptr_t start, size_t n_pages, pm_range_type_t type)
{
    const ptr_t end = start + n_pages * MOS_PAGE_SIZE;

    if (unlikely(n_pages == 0))
    {
        pr_warn("physical memory region " PTR_RANGE " is empty after alignment", start, end);
        return;
    }

    if (end < 1 MB && type != PM_RANGE_RESERVED)
    {
        type = PM_RANGE_RESERVED;
        pr_info2("reserving a low memory region " PTR_RANGE " (%zu page(s))", start, end, n_pages);
    }

    switch (type)
    {
        case PM_RANGE_FREE:
        case PM_RANGE_RESERVED:
        {
            spinlock_acquire(&pmlist_free_lock);
            if (!pmm_internal_do_add_free_frames_try_merge(start, n_pages, type))
            {
                // merging failed, so we have to insert the new region at the end of the list
                pmlist_node_t *node = pmm_internal_list_node_create(start, n_pages, type);
                list_node_append(&pmlist_free_rw, list_node(node));
            }
            spinlock_release(&pmlist_free_lock);
            break;
        }
        case PM_RANGE_UNINITIALIZED: mos_panic("pmm_add_region_pages() called with uninitialized region type");
        case PM_RANGE_ALLOCATED: mos_panic("pmm_add_region_pages() called with allocated region type");
    }
}

void pmm_impl_add_free_node_unlocked(pmlist_node_t *node)
{
    if (!pmm_internal_do_add_free_frames_try_merge(node->range.paddr, node->range.npages, node->type))
    {
        // merging failed, so we have to insert the new region at the end of the list
        list_node_append(&pmlist_free_rw, list_node(node));
    }
    else
    {
        // merging succeeded, so we can delete the node for sure
        MOS_ASSERT(node->type == PM_RANGE_FREE || node->type == PM_RANGE_RESERVED);
        MOS_ASSERT(node->refcount == 0);
        pmm_internal_list_node_delete(node);
    }
}

bool pmm_impl_get_free_frames(size_t n_pages, pmm_internal_op_callback_t callback, pmm_allocate_callback_t user_callback, void *user_arg)
{
    pmm_op_state_t state = { .pages_operated = 0, .pages_requested = n_pages };

    spinlock_acquire(&pmlist_free_lock);
    list_foreach(pmlist_node_t, curr, pmlist_free_rw)
    {
        // check if we are at the end of the list, or if we have allocated enough pages
        if (list_node(curr) == &pmlist_free_rw || n_pages == state.pages_operated)
            break;

        // skip reserved regions (of course!)
        if (curr->type != PM_RANGE_FREE)
            continue;

        const size_t npages_current = curr->range.npages;
        const size_t npages_left = n_pages - state.pages_operated; // number of pages left to allocate

#define ALLOC_DEBUG_FMT "  %8s: " PTR_RANGE " (%zu page(s))"
        if (npages_left >= npages_current)
        {
            // allocate the whole region
            MOS_ASSERT_X(curr->refcount == 0, "allocated a region with refcount != 0");
            mos_debug(pmm_impl, ALLOC_DEBUG_FMT, "whole", curr->range.paddr, curr->range.paddr + npages_current * MOS_PAGE_SIZE, npages_current);

            list_remove(curr);
            callback(&state, curr, user_callback, user_arg);
            state.pages_operated += npages_current;
        }
        else
        {
            // this branch only happens when we are at the end of the list
            // break the current region into two parts, always allocate the first part
            pmlist_node_t *new = pmm_internal_list_node_create(curr->range.paddr, npages_left, PM_RANGE_ALLOCATED);
            mos_debug(pmm_impl, ALLOC_DEBUG_FMT, "partial", new->range.paddr, new->range.paddr + npages_left *MOS_PAGE_SIZE, npages_left);

            curr->range.paddr += npages_left * MOS_PAGE_SIZE;
            curr->range.npages -= npages_left;

            callback(&state, new, user_callback, user_arg);
            state.pages_operated += npages_left;

            MOS_ASSERT(state.pages_operated == state.pages_requested);
            break; // we MUST have allocated enough pages, so we can stop iterating
        }
#undef ALLOC_DEBUG_FMT
    }
    spinlock_release(&pmlist_free_lock);

    if (state.pages_operated != n_pages)
        mos_panic("could not allocate %zu pages, only allocated %zu pages", n_pages, state.pages_operated);

    return true;
}

pmlist_node_t *pmm_impl_get_free_frames_at(ptr_t start_addr, size_t npages)
{
    const ptr_t end_addr = start_addr + npages * MOS_PAGE_SIZE;

    list_foreach(pmlist_node_t, this, pmlist_free_rw)
    {
#define this_start() (this->range.paddr)
#define this_end()   (this->range.paddr + this->range.npages * MOS_PAGE_SIZE)

        // if this region is completely before the region we want, skip it
        if (start_addr < this_start() || this_end() < end_addr)
            continue;

        //
        //       |-> Start of this region
        //       |               End of this region <-|
        // ======|====================================|======
        //  PREV | PART 1 | REGION TO RETURN | PART 2 | NEXT
        // ======|========|==================|========|======
        //                |-> start_addr     |
        //                        end_addr <-|
        //
        const size_t part_1_size = start_addr - this_start();
        const size_t part_2_size = this_end() - end_addr;

        if (part_1_size == 0 && part_2_size != 0)
        {
            // CASE 1: part 1 is empty, which means we are removing from the front of the region
            mos_debug(pmm_impl, "  case 1: split " PTR_RANGE ": new_start=" PTR_FMT, this_start(), this_end(), this_start() + npages * MOS_PAGE_SIZE);
            this->range.paddr = end_addr;
            this->range.npages = part_2_size / MOS_PAGE_SIZE;
        }
        else if (part_1_size != 0 && part_2_size == 0)
        {
            // CASE 2: part 2 is empty, which means we are removing the tail of part 1
            mos_debug(pmm_impl, "  case 2: split " PTR_RANGE ": new_end=" PTR_FMT, this_start(), this_end(), this_end() - npages * MOS_PAGE_SIZE);
            this->range.npages -= npages;
        }
        else if (part_1_size != 0 && part_2_size != 0)
        {
            // CASE 3: neither part 1 nor part 2 is empty, so we have to allocate a new entry for part 2
            mos_debug(pmm_impl, "  case 3: split " PTR_RANGE ": new_end=" PTR_FMT, this_start(), this_end(), this_end() - npages * MOS_PAGE_SIZE);
            this->range.npages = part_1_size / MOS_PAGE_SIZE;
            pmlist_node_t *part2 = pmm_internal_list_node_create(end_addr, part_2_size / MOS_PAGE_SIZE, this->type);
            list_insert_after(this, part2);
        }
        else
        {
            // CASE 4: both part 1 and part 2 are empty, which means we are removing the whole region
            mos_debug(pmm_impl, "  case 4: whole block " PTR_RANGE, this_start(), this_end());
            list_remove(this);
            return this;
        }
#undef this_start
#undef this_end

        return pmm_internal_list_node_create(start_addr, npages, this->type);
    }

    return NULL;
}

pmlist_node_t *pmm_impl_find_and_acquire_block(ptr_t needle, pm_range_type_t type)
{
    pmlist_node_t *node = NULL;

    spinlock_acquire(&pmlist_free_lock);
    list_foreach(pmlist_node_t, c, pmlist_free_rw)
    {
        if (c->type != type)
            continue;

        if (IN_RANGE(needle, c->range.paddr, c->range.paddr + c->range.npages * MOS_PAGE_SIZE))
        {
            node = c;
            list_remove(node);
            break;
        }
    }
    spinlock_release(&pmlist_free_lock);

    return node;
}
