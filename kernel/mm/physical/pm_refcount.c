// SPDX-License-Identifier: GPL-3.0-or-later

#define mos_pmm_impl

#include <mos/mm/kmalloc.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mm/physical/pmm_internal.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <stdlib.h>
#include <string.h>

static list_head pmlist_allocated_rw = LIST_HEAD_INIT(pmlist_allocated_rw);
static spinlock_t pmlist_allocated_lock = SPINLOCK_INIT;
const list_head *const pmlist_allocated = &pmlist_allocated_rw;

/**
 * @brief Inserts a node into the list of allocated memory regions.
 *
 * @param node The node to insert.
 */
static void insert_allocated_node(pmlist_node_t *node)
{
    list_foreach(pmlist_node_t, current, pmlist_allocated_rw)
    {
        if (current->range.paddr < node->range.paddr)
            continue;

        const ptr_t c_start = current->range.paddr;
        const size_t c_size = current->range.npages * MOS_PAGE_SIZE;
        const ptr_t n_start = node->range.paddr;
        const size_t n_size = node->range.npages * MOS_PAGE_SIZE;

        if (SUBSET_RANGE(n_start, n_size, c_start, c_size) || SUBSET_RANGE(c_start, c_size, n_start, n_size))
            mos_panic("pmm: trying to insert a node that is a subset of an existing node");

        list_insert_before(current, node);
        return;
    }

    list_node_append(&pmlist_allocated_rw, list_node(node));
}

void pmm_internal_iterate_allocated_list_range(ptr_t start, size_t npages, refcount_operation_t op, pmm_internal_unref_callback_t callback, void *arg)
{
    if (op != OP_REF && op != OP_UNREF)
        mos_panic("pmm: invalid refcount operation");

    if (op == OP_UNREF && callback == NULL)
        mos_panic("pmm: callback must be set when decrementing refcount");

    // start: start of the range to iterate over, will change as we iterate
    // npages: number of pages to iterate over, will change as we iterate
    const ptr_t end = start + npages * MOS_PAGE_SIZE;

    spinlock_acquire(&pmlist_allocated_lock);
    list_foreach(pmlist_node_t, this, pmlist_allocated_rw)
    {
        const ptr_t cstart = this->range.paddr;
        const ptr_t cend = this->range.paddr + this->range.npages * MOS_PAGE_SIZE;

        if (npages == 0)
            break;

        if (cend <= start)
            continue; // not yet reached the range

        if (unlikely(cstart > start))
        {
            pr_emerg("the list is sorted, so this should never happen (cstart: " PTR_FMT ", start: " PTR_FMT ")", cstart, start);
            pr_emerg("have you reserved the memory before mapping it?");
            mos_panic("pmm: invalid list state");
        }

        // There are two cases:
        // 1. 'this' is completely contained within the range, or is equal to the range
        if (cstart == start && this->range.npages <= npages)
        {
            mos_debug(pmm_impl, "  entire node " PTR_RANGE " is within range " PTR_RANGE "", cstart, cend, start, end);
            switch (op)
            {
                case OP_REF: this->refcount++; break;
                case OP_UNREF:
                {
                    this->refcount--;
                    if (this->refcount == 0)
                        list_remove(this), callback(this, arg);
                    break;
                }
            }

            // update iteration state
            npages -= this->range.npages;
            start += this->range.npages * MOS_PAGE_SIZE;
            continue;
        }

        //
        //       |-> Start of this region
        //       |                End of this region <-|
        // ======|=======|=====================|=======|======
        //  PREV |       |        THIS         |       | NEXT
        // ======|=======|=====================|=======|======
        //       |-> A <-|                     |-> B <-|
        //               |                     |
        //               |-> split point 1     |-> split point 2
        //

        // 2. 'this' is a superset of the range, we split it into two or three nodes

        const ptr_t p1_start = cstart;
        const ptr_t p2_start = end;

        const size_t p1_npages = (start < cstart ? cstart - start : 0) / MOS_PAGE_SIZE;
        const size_t p2_npages = (end < cend ? cend - end : 0) / MOS_PAGE_SIZE;

        mos_debug(pmm_impl, "  node: " PTR_RANGE ", we want " PTR_RANGE ", part1: %c, part2: %c", cstart, cend, start, end, p1_npages ? 'y' : 'n', p2_npages ? 'y' : 'n');

        MOS_ASSERT(p1_npages || p2_npages);

        const size_t effective_npages = this->range.npages - p1_npages - p2_npages;
        npages -= effective_npages;
        start += effective_npages * MOS_PAGE_SIZE;

        if (p1_npages)
        {
            mos_debug(pmm_impl, "    part 1: " PTR_RANGE ", npages: %zu", p1_start, p1_start + p1_npages * MOS_PAGE_SIZE, p1_npages);

            pmlist_node_t *p1 = pmm_internal_list_node_create(cstart, p1_npages, this->type);
            p1->refcount = this->refcount;
            this->range.npages -= p1_npages;
            this->range.paddr = p1_start;
            insert_allocated_node(p1);
        }

        // in this case, the node must be the last one
        if (p2_npages)
        {
            MOS_ASSERT_X(npages == 0 && start == end, "this should be the last node");
            mos_debug(pmm_impl, "    part 2: " PTR_RANGE ", npages: %zu", p2_start, p2_start + p2_npages * MOS_PAGE_SIZE, p2_npages);

            pmlist_node_t *p2 = pmm_internal_list_node_create(p2_start, p2_npages, this->type);
            p2->refcount = this->refcount;
            this->range.npages -= p2_npages;
            insert_allocated_node(p2);
        }

        switch (op)
        {
            case OP_REF: this->refcount++; break;
            case OP_UNREF:
            {
                this->refcount--;
                if (this->refcount == 0)
                    list_remove(this), callback(this, arg);
                break;
            }
        }
    }
    spinlock_release(&pmlist_allocated_lock);

    if (npages != 0)
        mos_panic("pmm: tried to iterate over a range that is not allocated");
}
void pmm_internal_add_node_to_allocated_list(pmlist_node_t *node)
{
    MOS_ASSERT(node->type == PM_RANGE_ALLOCATED || node->type == PM_RANGE_RESERVED);
    spinlock_acquire(&pmlist_allocated_lock);
    insert_allocated_node(node);
    spinlock_release(&pmlist_allocated_lock);
}
